/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2005 EQEMu Development Team (http://eqemulator.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "../common/strings.h"

#include "queryserv.h"
#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"
#include "zone.h"

#include "../common/repositories/zone_repository.h"
#include "../common/content/world_content_service.h"
#include "../common/events/player_event_logs.h"

extern QueryServ* QServ;
extern WorldServer worldserver;
extern Zone* zone;

static uint32 NextZoneTransferTransactionID()
{
	static uint32 transaction_id = 0;

	++transaction_id;
	if (transaction_id == 0)
		++transaction_id;

	return transaction_id;
}

void Client::SetPendingZoneTransfer(ZoneMode mode, uint32 zone_id, const glm::vec4 &destination, uint8 ignore_restrictions)
{
	if (dead && m_pending_zone_transfer.phase != ZoneTransferPhase::None)
	{
		// Death has already selected bind. Nothing else may replace it before the client starts that transfer.
		LogInfo("Ignoring a new zone transfer for dead client [{}]; the bind transfer is already waiting for the client", GetName());
		return;
	}

	if (m_pending_zone_transfer.phase != ZoneTransferPhase::None &&
		m_pending_zone_transfer.phase != ZoneTransferPhase::AwaitingZoneStatusRequest)
	{
		LogInfo(
			"Ignoring a new zone transfer for client [{}]; the current transfer is already in phase [{}]",
			GetName(),
			static_cast<int>(m_pending_zone_transfer.phase)
		);
		return;
	}

	m_pending_zone_transfer.phase = ZoneTransferPhase::AwaitingZoneStatusRequest;
	m_pending_zone_transfer.mode = mode;
	m_pending_zone_transfer.zone_id = zone_id;
	m_pending_zone_transfer.destination = destination;
	m_pending_zone_transfer.ignore_restrictions = ignore_restrictions;
	m_pending_zone_transfer.transaction_id = 0;
	pending_zone_transfer_timer.Start(ZoneTransferRequestTimeoutMs);
}

void Client::ClearPendingZoneTransfer()
{
	m_pending_zone_transfer = PendingZoneTransfer();
	pending_zone_transfer_timer.Disable();
}

void Client::SendZoneStatusResponse(uint32 zone_id, ZoningMessage result)
{
	cheat_manager.SetExemptStatus(Port, true);

	auto outapp = new EQApplicationPacket(OP_ZoneChange, sizeof(ZoneChange_Struct));
	auto response = reinterpret_cast<ZoneChange_Struct *>(outapp->pBuffer);
	strn0cpy(response->char_name, GetName(), sizeof(response->char_name));
	response->zoneID = zone_id;
	response->success = result;
	outapp->priority = 6;
	FastQueuePacket(&outapp, true, client_state);
}

void Client::HandleZoneTransferResponse(uint32 current_zone_id, uint32 requested_zone_id, uint32 transaction_id, int8 response)
{
	if (client_state == CLIENT_KICKED || client_state == DISCONNECTED)
	{
		LogInfo("Ignoring zone transfer response for [{}] after client removal was requested", GetName());
		ClearPendingZoneTransfer();
		return;
	}

	if (transaction_id == 0 ||
		m_pending_zone_transfer.phase != ZoneTransferPhase::AwaitingWorldResponse ||
		current_zone_id != zone->GetZoneID() ||
		m_pending_zone_transfer.zone_id != requested_zone_id ||
		m_pending_zone_transfer.transaction_id != transaction_id)
	{
		LogInfo(
			"Ignoring stale zone transfer response for [{}]: transaction [{}], current zone [{}], requested zone [{}], pending transaction [{}], pending zone [{}], phase [{}]",
			GetName(),
			transaction_id,
			current_zone_id,
			requested_zone_id,
			m_pending_zone_transfer.transaction_id,
			m_pending_zone_transfer.zone_id,
			static_cast<int>(m_pending_zone_transfer.phase)
		);
		return;
	}

	if (response > 0)
	{
		if (!CommitPendingZoneTransfer())
			return;

		m_pending_zone_transfer.phase = ZoneTransferPhase::AwaitingDeleteSpawn;
		pending_zone_transfer_timer.Start(ZoneTransferDepartureTimeoutMs);

		if (m_stream == nullptr || IsLD())
		{
			FinishPendingZoneTransfer();
			return;
		}

		SendZoneStatusResponse(requested_zone_id, ZoningMessage::ZoneSuccess);
		return;
	}

	if (dead)
		dead_timer.Start(DeadClientRemovalTimeoutMs);

	SendZoneStatusResponse(requested_zone_id, ZoningMessage::ZoneNotReady);
	ClearPendingZoneTransfer();
	if (m_stream == nullptr && !IsLD())
		LinkDead();

	switch (response)
	{
	case -2:
		Message(Chat::Red, "You do not own the required locations to enter this zone.");
		break;
	case -1:
		Message(Chat::Red, "The zone is currently full, please try again later.");
		break;
	case 0:
		Message(Chat::Red, "All zone servers are taken at this time, please try again later.");
		break;
	}
}


// This is the client's IsZoneAvailable() request. A successful world reply commits the destination and OP_DeleteSpawn finishes the departure.
void Client::Handle_OP_ZoneChange(const EQApplicationPacket *app)
{
	if (app->size != sizeof(ZoneChange_Struct))
	{
		LogError("Wrong size: OP_ZoneChange, size=[{}], expected [{}]", app->size, sizeof(ZoneChange_Struct));
		DumpPacket(app);
		return;
	}

	auto zone_change = reinterpret_cast<const ZoneChange_Struct *>(app->pBuffer);
	LogInfo(
		"Zone request from [{}]: char_name [{}], zoneID [{}], zone_reason [{}], success [{}]",
		GetName(),
		zone_change->char_name,
		zone_change->zoneID,
		zone_change->zone_reason,
		zone_change->success
	);

	const uint32 target_zone_id = zone_change->zoneID;
	if (m_pending_zone_transfer.phase == ZoneTransferPhase::AwaitingWorldResponse)
	{
		if (m_pending_zone_transfer.zone_id == target_zone_id)
		{
			LogInfo("Ignoring a repeated zone status request for [{}] while waiting for world", GetName());
		}
		else
		{
			LogWarning(
				"Ignoring a zone status request for [{}] to zone [{}] while waiting for world to answer zone [{}]",
				GetName(),
				target_zone_id,
				m_pending_zone_transfer.zone_id
			);
		}
		return;
	}

	if (m_pending_zone_transfer.phase == ZoneTransferPhase::AwaitingDeleteSpawn)
	{
		if (m_pending_zone_transfer.zone_id == target_zone_id)
		{
			SendZoneStatusResponse(target_zone_id, ZoningMessage::ZoneSuccess);
		}
		else
		{
			LogWarning(
				"Ignoring a zone status request for [{}] to zone [{}] while waiting for departure to zone [{}]",
				GetName(),
				target_zone_id,
				m_pending_zone_transfer.zone_id
			);
		}
		return;
	}

	// A death which began before OP_ZoneChange owns the destination. Any status request can advance the bind transfer which was already saved and latched.
	if (dead && m_pending_zone_transfer.phase == ZoneTransferPhase::AwaitingZoneStatusRequest)
	{
		if (m_pending_zone_transfer.zone_id != target_zone_id)
		{
			LogWarning(
				"Dead client [{}] requested zone [{}] while the bind transfer is waiting for zone [{}]. Using the bind transfer.",
				GetName(),
				target_zone_id,
				m_pending_zone_transfer.zone_id
			);
		}

		RequestZoneTransferApproval(
			m_pending_zone_transfer.mode,
			m_pending_zone_transfer.zone_id,
			m_pending_zone_transfer.destination,
			m_pending_zone_transfer.ignore_restrictions
		);
		return;
	}

	if (target_zone_id == 0)
	{
		LogError("Zoning [{}]: Client requested invalid zone id [0]", GetName());
		SendZoneStatusResponse(target_zone_id, ZoningMessage::ZoneDown);
		return;
	}

	if (dead && m_pending_zone_transfer.phase == ZoneTransferPhase::None)
	{
		// A failed bind check clears the transfer, but the client may still send its current zone recovery query. Do not let a dead client begin another move.
		SendZoneStatusResponse(
			target_zone_id,
			target_zone_id == zone->GetZoneID() ? ZoningMessage::ZoneSuccess : ZoningMessage::ZoneDown
		);
		return;
	}

	// Ordinary zone points use reason 0 and must be resolved from zone-point data, even if an older server-selected destination has the same zone id.
	const bool client_zone_point = zone_change->zone_reason == ZC_Teleport;
	const bool use_pending_destination =
		!client_zone_point &&
		m_pending_zone_transfer.phase == ZoneTransferPhase::AwaitingZoneStatusRequest &&
		m_pending_zone_transfer.zone_id == target_zone_id;
	if (client_zone_point && m_pending_zone_transfer.phase == ZoneTransferPhase::AwaitingZoneStatusRequest)
	{
		LogInfo(
			"Resolving reason 0 zone point for [{}] without using the pending destination for zone [{}]",
			GetName(),
			m_pending_zone_transfer.zone_id
		);
	}

	ZonePoint *zone_point = nullptr;
	if (!use_pending_destination)
	{
		// The client does a second status query for its current zone after a declined status check for its original destination.
		if (target_zone_id == zone->GetZoneID())
		{
			SendZoneStatusResponse(target_zone_id, ZoningMessage::ZoneSuccess);
			return;
		}

		// client side 'zone lines' are lists of DRNTP BSP regions that either encode the destination directly or reference a teleport index.
		// the 'zone_points' table in the database combines two things: the teleport records sent in OP_Zonepoints and an eqemu server side
		// bounding box teleport system called virtual zone points.
		//
		// the x/y positions on the zone_point here are server side authored validation reusing the virtual zone point fields for this.
		// teleport coords sent to the client do not have an x/y position, they are just destination coords.  this validation relies on there
		// being 'fake' records in zone_points for the directly encoded destinations that don't naturally reference a TP index.
		//
		// TODO: check the zone geometry regions, project the client's position to estimate where they touched it
		// and resolve destination coordinates from the the effective DRNTP string.
		zone_point = zone->GetClosestZonePoint(glm::vec3(GetPosition()), target_zone_id, this, ZONEPOINT_ZONE_RANGE);
		if (zone_point == nullptr || zone_point->target_zone_id != target_zone_id)
		{
			LogError("Zoning [{}]: Invalid unsolicited zone request to zone id [{}]", GetName(), target_zone_id);
			if (GetBindZoneID() == target_zone_id)
				cheat_manager.CheatDetected(MQGate, glm::vec3(GetX(), GetY(), GetZ()));
			else
				cheat_manager.CheatDetected(MQZone, glm::vec3(GetX(), GetY(), GetZ()));

			SendZoneStatusResponse(target_zone_id, ZoningMessage::ZoneDown);
			return;
		}
	}

	const char *target_zone_name = ZoneName(target_zone_id);
	auto zone_data = GetZone(target_zone_id);
	if (target_zone_name == nullptr || zone_data == nullptr)
	{
		Message(Chat::Red, "Invalid target zone ID.");
		LogError("Zoning [{}]: Unable to get zone information for zone id '[{}]'.", GetName(), target_zone_id);
		SendZoneStatusResponse(target_zone_id, ZoningMessage::ZoneDown);
		if (use_pending_destination)
			ClearPendingZoneTransfer();
		return;
	}

	std::string export_string = fmt::format("{} {}", zone->GetZoneID(), target_zone_id);
	if (parse->EventPlayer(EVENT_ZONE, this, export_string, 0) != 0)
	{
		SendZoneStatusResponse(target_zone_id, ZoningMessage::ZoneDown);
		if (use_pending_destination)
			ClearPendingZoneTransfer();
		return;
	}

	glm::vec4 destination;
	uint8 ignore_restrictions = 0;
	ZoneMode mode = ZoneUnsolicited;
	if (use_pending_destination)
	{
		destination = m_pending_zone_transfer.destination;
		ignore_restrictions = m_pending_zone_transfer.ignore_restrictions;
		mode = m_pending_zone_transfer.mode;
	}
	else
	{
		const uint32 current_zone_id = zone->GetZoneID();
		float target_x = zone_point->target_x;
		float target_y = zone_point->target_y;
		float target_z = zone_point->target_z;
		float target_heading = zone_point->target_heading;

		if (current_zone_id == Zones::FREPORTE && target_zone_id == Zones::NRO)
			target_x = GetX() + 1044.0f;
		else if (current_zone_id == Zones::NRO && target_zone_id == Zones::FREPORTE)
			target_x = GetX() - 1044.0f;
		else if (current_zone_id == Zones::QRG && target_zone_id == Zones::QEYTOQRG)
			target_z = GetZ() + 2.0f;
		else if (current_zone_id == Zones::QEYNOS2 && target_zone_id == Zones::QEYTOQRG)
			target_y += 10.0f;

		const bool preserve_z = target_z >= 999999.0f;
		destination = ResolveClientTeleportDestination(target_zone_id, glm::vec4(target_x, target_y, target_z, target_heading));

		// This is not client behavior. It is an extra margin to keep the player above the floor when the zone point preserves Z.
		if (preserve_z)
			destination.z += 5.0f;
	}

	auto zoning_message = ZoningMessage::ZoneSuccess;

	if (!ignore_restrictions && (Admin() < zone_data->min_status || GetLevel() < zone_data->min_level))
		zoning_message = ZoningMessage::ZoneNoExperience;

	if (!ignore_restrictions && !zone_data->flag_needed.empty())
	{
		if (Admin() < minStatusToIgnoreZoneFlags && !HasZoneFlag(target_zone_id))
		{
			LogInfo(
				"Client [{}] does not have the proper flag to enter [{}] ({})",
				GetCleanName(),
				target_zone_name,
				target_zone_id
			);
			Message(Chat::Red, "You do not have the flag to enter %s.", target_zone_name);
			zoning_message = ZoningMessage::ZoneNoExperience;
		}
	}

	if (Admin() < minStatusToIgnoreZoneFlags && IsMule() &&
		target_zone_id != Zones::BAZAAR && target_zone_id != Zones::NEXUS && target_zone_id != Zones::POKNOWLEDGE)
	{
		zoning_message = ZoningMessage::ZoneNoExperience;
		LogCharacterDetail("[CLIENT] Character is a mule and cannot leave Bazaar/Nexus/PoK!");
	}

	if (WorldContentService::Instance()->GetCurrentExpansion() >= Expansion::Classic && !GetGM())
	{
		auto zones = ZoneRepository::GetWhere(
			database,
			fmt::format(
				"expansion <= {} AND short_name = '{}'",
				WorldContentService::Instance()->GetCurrentExpansion(),
				target_zone_name
			)
		);

		LogInfo(
			"Checking zone request [{}] for expansion [{}] ({}) success [{}]",
			target_zone_name,
			WorldContentService::Instance()->GetCurrentExpansion(),
			WorldContentService::Instance()->GetCurrentExpansionName(),
			!zones.empty() ? "true" : "false"
		);

		if (zones.empty())
			zoning_message = ZoningMessage::ZoneNoExpansion;
	}

	if (WorldContentService::Instance()->GetCurrentExpansion() >= Expansion::Classic && GetGM())
		LogInfo("[{}] Bypassing Expansion zone checks because GM status is set", GetCleanName());

	if (zoning_message == ZoningMessage::ZoneSuccess)
	{
		RequestZoneTransferApproval(mode, target_zone_id, destination, ignore_restrictions);
		return;
	}

	LogError("Zoning [{}]: Rules prevent this char from zoning into [{}]", GetName(), target_zone_name);
	SendZoneStatusResponse(target_zone_id, zoning_message);
	if (use_pending_destination)
		ClearPendingZoneTransfer();
}

void Client::RequestZoneTransferApproval(ZoneMode mode, uint32 zone_id, const glm::vec4 &destination, uint8 ignore_restrictions)
{
	LogInfo("Beginning zone transfer for [{}] to zone [{}], mode [{}]", GetName(), zone_id, (int)mode);

	// Reaching OP_ZoneChange means the client has left gameplay even though world has not answered yet.
	Mob *pet = GetPet();
	if (pet && pet->IsCharmedPet())
		FadePetCharmBuff();

	entity_list.RemoveFromHateLists(this);
	m_client_npc_aggro_scan_timer.Reset();

	EndShield();
	DepopPet();

	m_pending_zone_transfer.phase = ZoneTransferPhase::AwaitingWorldResponse;
	m_pending_zone_transfer.mode = mode;
	m_pending_zone_transfer.zone_id = zone_id;
	m_pending_zone_transfer.destination = destination;
	m_pending_zone_transfer.ignore_restrictions = ignore_restrictions;
	m_pending_zone_transfer.transaction_id = NextZoneTransferTransactionID();
	pending_zone_transfer_timer.Start(ZoneTransferApprovalTimeoutMs);
	// The dead client removal timer only runs while no active zone transfer timer owns progress.
	if (dead)
		dead_timer.Disable();

	auto pack = new ServerPacket(ServerOP_ZoneToZoneRequest, sizeof(ZoneToZone_Struct));
	auto request = reinterpret_cast<ZoneToZone_Struct *>(pack->pBuffer);
	request->response = 0;
	request->current_zone_id = zone->GetZoneID();
	request->requested_zone_id = zone_id;
	request->admin = admin;
	request->ignorerestrictions = ignore_restrictions;
	request->transaction_id = m_pending_zone_transfer.transaction_id;
	strn0cpy(request->name, GetName(), sizeof(request->name));
	request->guild_id = GuildID();
	worldserver.SendPacket(pack);
	safe_delete(pack);
}

bool Client::CommitPendingZoneTransfer()
{
	if (m_pending_zone_transfer.phase != ZoneTransferPhase::AwaitingWorldResponse)
		return false;

	PendingZoneTransfer &transfer = m_pending_zone_transfer;

	LogInfo("Committing zone transfer for [{}] to zone [{}], mode [{}]", GetName(), transfer.zone_id, (int)transfer.mode);

	if (PlayerEventLogs::Instance()->IsEventEnabled(PlayerEvent::ZONING)) {
		auto e = PlayerEvent::ZoningEvent{};
		e.from_zone_long_name = zone->GetLongName();
		e.from_zone_short_name = zone->GetShortName();
		e.from_zone_id = zone->GetZoneID();
		e.to_zone_long_name = ZoneLongName(transfer.zone_id);
		e.to_zone_short_name = ZoneName(transfer.zone_id);
		e.to_zone_id = transfer.zone_id;

		RecordPlayerEventLog(PlayerEvent::ZONING, e);
	}

	if (transfer.zone_id == Zones::AIRPLANE)
		BuffFadeAll(true);

	LogInfo("Zoning [{}] to: [{}] ([{}]) x = [{}], y = [{}], z = [{}]", m_pp.name, ZoneName(transfer.zone_id), transfer.zone_id, transfer.destination.x, transfer.destination.y, transfer.destination.z);

	//set the player's coordinates in the new zone so they have them
	//when they zone into it
	m_pp.x = transfer.destination.x;
	m_pp.y = transfer.destination.y;
	m_pp.z = transfer.destination.z;
	m_pp.heading = transfer.destination.w;
	m_pp.zone_id = transfer.zone_id;
	m_lock_save_position = true;
	//Force a save so its waiting for them when they zone
	Save(2);

	UpdateZoneChangeCount(transfer.zone_id);
	return true;
}

bool Client::FinishPendingZoneTransfer()
{
	if (m_pending_zone_transfer.phase != ZoneTransferPhase::AwaitingDeleteSpawn)
		return false;

	DisconnectForZoneTransfer();
	ClearPendingZoneTransfer();
	return true;
}

void Client::HandlePendingZoneTransferTimeout()
{
	const ZoneTransferPhase phase = m_pending_zone_transfer.phase;
	const uint32 zone_id = m_pending_zone_transfer.zone_id;

	if (phase == ZoneTransferPhase::AwaitingWorldResponse)
	{
		LogWarning("Zone transfer for [{}] to zone [{}] timed out waiting for world", GetName(), zone_id);
		SendZoneStatusResponse(zone_id, ZoningMessage::ZoneNotReady);
		ClearPendingZoneTransfer();
		if (m_stream == nullptr && !IsLD())
			LinkDead();

		if (dead)
			dead_timer.Start(DeadClientRemovalTimeoutMs);
		return;
	}

	if (phase == ZoneTransferPhase::AwaitingDeleteSpawn)
	{
		LogWarning("Zone transfer for [{}] to zone [{}] timed out waiting for OP_DeleteSpawn", GetName(), zone_id);
		FinishPendingZoneTransfer();
		return;
	}

	ClearPendingZoneTransfer();
}

glm::vec4 Client::ResolveClientTeleportDestination(uint32 zone_id, const glm::vec4 &requested) const
{
	// The client applies these changes in CDisplay::DoTeleportB. This path is used for zone points and OP_RequestClientZoneChange, but not spells or death.
	static constexpr float PreserveCoordinate = 999999.0f;
	static constexpr float PreserveHeading = 513.0f;

	glm::vec4 destination = requested;

	if (requested.x >= PreserveCoordinate)
		destination.x = GetX();

	if (requested.y >= PreserveCoordinate)
		destination.y = GetY();

	if (requested.z >= PreserveCoordinate)
		destination.z = GetZ();

	if (requested.w >= PreserveHeading)
		destination.w = GetHeading() * 2.0f;

	if (zone_id != zone->GetZoneID() &&
		(GetRace() == Race::Dwarf || GetRace() == Race::Halfling || GetRace() == Race::Gnome))
	{
		destination.z += 1.0f;
	}

	return destination;
}

void Client::SendTeleportPacket(uint32 zone_id, const glm::vec4 &destination, uint32 reason)
{
	// Reason 0 is reserved for ordinary client zone points. A server requested move must use a nonzero reason so its status request uses the saved destination.
	if (reason == ZC_Teleport)
	{
		LogError("SendTeleportPacket received the reserved zone point reason [0]; using server teleport reason [1]");
		reason = 1;
	}

	auto outapp = new EQApplicationPacket(OP_RequestClientZoneChange, sizeof(RequestClientZoneChange_Struct));
	auto request = reinterpret_cast<RequestClientZoneChange_Struct *>(outapp->pBuffer);
	request->zone_id = zone_id;
	request->x = destination.x;
	request->y = destination.y;
	request->z = destination.z;
	request->heading = destination.w;
	request->type = reason;
	outapp->priority = 6;
	FastQueuePacket(&outapp);
}

// solar: this expects the 512 scale heading, GetHeading() * 2.0f
void Client::MovePC(uint32 zoneID, float x, float y, float z, float heading, uint8 ignorerestrictions, ZoneMode zm)
{
	if (dead || IsZoningOut())
	{
		LogInfo("Ignoring a movement request for client [{}] while dead or zoning out", GetName());
		return;
	}

	if (IsAIControlled())
		StopNavigation();
	if (currently_fleeing)
		currently_fleeing = false;

	// From what I have read, dragged corpses should stay with the player for intra-zone summons, but we can implement that later.
	ClearDraggedCorpses();

	if (zoneID == 0)
		zoneID = zone->GetZoneID();

	const char *zone_name = ZoneName(zoneID);
	auto zone_data = GetZone(zoneID);
	if (zone_name == nullptr || zone_data == nullptr)
	{
		Message(Chat::Red, "Invalid zone number specified");
		return;
	}

	glm::vec4 destination(x, y, z, heading);
	bool transfer = zoneID != zone->GetZoneID();

	switch (zm)
	{
	case ZoneToSafeCoords:
		if (zoneID == zone->GetZoneID())
		{
			destination = zone->GetSafePoint();
		}
		else
		{
			destination = glm::vec4(zone_data->safe_x, zone_data->safe_y, zone_data->safe_z, zone_data->safe_heading);
		}
		LogInfo(
			"Zoning [{}] to safe coords ([{}],[{}],[{}],[{}]) in [{}] ([{}])",
			GetName(),
			destination.x,
			destination.y,
			destination.z,
			destination.w,
			zone_name,
			zoneID
		);
		break;

	case GMSummon:
		if (!GetGM())
			Message(Chat::Yellow, "You have been summoned by a GM!");
		ignorerestrictions = 1;
		break;

	case ZoneSolicited:
		break;

	case GateToBindPoint:
		destination = glm::vec4(m_pp.binds[0].x, m_pp.binds[0].y, m_pp.binds[0].z, m_pp.binds[0].heading);
		break;

	case SummonPC:
		Message_StringID(Chat::Yellow, StringID::BEEN_SUMMONED);
		if (zoneID != zone->GetZoneID())
		{
			LogError("Client::MovePC received a local movement mode for another zone.");
			return;
		}
		break;

	case Rewind:
		Message(Chat::Yellow, "Rewinding to previous location.");
		if (zoneID != zone->GetZoneID())
		{
			LogError("Client::MovePC received a local movement mode for another zone.");
			return;
		}
		LogInfo(
			"[{}] has requested a /rewind from [{}], [{}], [{}], to [{}], [{}], [{}] in [{}]",
			GetName(),
			m_Position.x,
			m_Position.y,
			m_Position.z,
			destination.x,
			destination.y,
			destination.z,
			zone->GetShortName()
		);
		break;

	default:
		LogError("Client::MovePC received a request to perform an unsupported client zone operation.");
		return;
	}

	cheat_manager.SetExemptStatus(Port, true);

	if (IsLooting())
	{
		Corpse *corpse = entity_list.GetCorpseByID(entity_id_being_looted);
		if (corpse == nullptr)
		{
			Corpse::SendLootReqErrorPacket(this);
		}
		else
		{
			Corpse::SendEndLootErrorPacket(this);
			corpse->EndLoot(this, nullptr);
		}
		SetLooting(0);
	}

	if (Trader)
		Trader_EndTrader();

	const glm::vec4 committed_destination = ResolveClientTeleportDestination(zoneID, destination);

	if (transfer)
	{
		// Store the destination after applying the changes the client will make when it receives the teleport packet.
		SetPendingZoneTransfer(zm, zoneID, committed_destination, ignorerestrictions);

		// Send the original destination. The client applies the same changes before it starts zoning.
		SendTeleportPacket(zoneID, destination);
	}
	else
	{
		if (GetPetID() != 0 && GetGM())
		{
			Mob *pet = GetPet();
			if (pet != nullptr)
			{
				pet->SetPetOrder(SPO_Follow);
				pet->GMMove(committed_destination.x + 15.0f, committed_destination.y, committed_destination.z);
			}
		}

		m_Position.x = committed_destination.x;
		m_Position.y = committed_destination.y;
		m_Position.z = committed_destination.z;
		m_Position.w = committed_destination.w * 0.5f; // MovePC takes the full 512 scale heading; internal heading is 256 scale.

		// Send the original destination so the client resolves the same sentinel values.
		SendTeleportPacket(zoneID, destination);

		// Proximity events may move the client again, so process them after sending this move.
		entity_list.ProcessMove(this, glm::vec3(m_Position));
		m_Proximity = glm::vec3(m_Position);
		SendPosition(true);
	}

	LogEQMacDetail(
		"Player [{}] has requested a move to LOC x=[{}], y=[{}], z=[{}], heading=[{}] in zoneid=[{}]",
		GetName(),
		destination.x,
		destination.y,
		destination.z,
		destination.w,
		zoneID
	);
}

void Client::GoToSafeCoords(uint16 zone_id) {
	if(zone_id == 0)
		zone_id = zone->GetZoneID();

	MovePC(zone_id, 0.0f, 0.0f, 0.0f, 0.0f, 0, ZoneToSafeCoords);
}


void Mob::Gate() {
	GoToBind();
}

void Client::Gate() 
{
	Mob::Gate();
}

void NPC::Gate() {
	entity_list.FilteredMessageClose_StringID(this, true, RuleI(Range,SpellMessages), Chat::SpellCrit, FilterSpellCrits, StringID::GATES, GetCleanName());
	
	if (GetHPRatio() < 25.0f)
	{
		SetHP(GetMaxHP() / 4);
	}

	Mob::Gate();
}

void Client::SetBindPoint(int to_zone, const glm::vec3& location) {
	if (to_zone == -1) {
		m_pp.binds[0].zoneId = zone->GetZoneID();
		m_pp.binds[0].x = m_Position.x;
		m_pp.binds[0].y = m_Position.y;
		m_pp.binds[0].z = m_Position.z;
		m_pp.binds[0].heading = m_Position.w * 2.0f;
	}
	else {
		m_pp.binds[0].zoneId = to_zone;
		m_pp.binds[0].x = location.x;
		m_pp.binds[0].y = location.y;
		m_pp.binds[0].z = location.z;
		m_pp.binds[0].heading = m_Position.w * 2.0f;
	}
	database.SaveCharacterBinds(this);
}

void Client::SetBindPoint2(int to_zone, const glm::vec4& location) {
	if (to_zone == -1) {
		m_pp.binds[0].zoneId = zone->GetZoneID();
		m_pp.binds[0].x = m_Position.x;
		m_pp.binds[0].y = m_Position.y;
		m_pp.binds[0].z = m_Position.z;
		m_pp.binds[0].heading = m_Position.w * 2.0f;
	}
	else {
		m_pp.binds[0].zoneId = to_zone;
		m_pp.binds[0].x = location.x;
		m_pp.binds[0].y = location.y;
		m_pp.binds[0].z = location.z;
		m_pp.binds[0].heading = location.w * 2.0f;
	}
	database.SaveCharacterBinds(this);
}

void Client::GoToBind(uint8 bindnum) {
	// if the bind number is invalid, use the primary bind
	if(bindnum > 4)
		bindnum = 0;

	// move the client, which will zone them if needed.
	// ignore restrictions on the zone request..?
	if(bindnum == 0)
		MovePC(m_pp.binds[0].zoneId, 0.0f, 0.0f, 0.0f, 0.0f, 1, GateToBindPoint);
	else
		MovePC(m_pp.binds[bindnum].zoneId, m_pp.binds[bindnum].x, m_pp.binds[bindnum].y, m_pp.binds[bindnum].z, m_pp.binds[bindnum].heading, 1);
}

void Client::GoToDeath()
{
	dead = true;
	ClearPendingZoneTransfer();
	ClearPendingTranslocate();
	ClearPendingResurrection();
	ClearPendingSacrifice();

	// This is a back stop for a dead client which never begins the transfer.  It is restarted if the transfer fails.
	dead_timer.Start(DeadClientRemovalTimeoutMs, true);

	if (IsLooting())
	{
		Corpse *corpse = entity_list.GetCorpseByID(entity_id_being_looted);
		if (corpse == nullptr)
		{
			Corpse::SendLootReqErrorPacket(this);
		}
		else
		{
			Corpse::SendEndLootErrorPacket(this);
			corpse->EndLoot(this, nullptr);
		}
		SetLooting(0);
	}

	CloseTradeskillObject();
	CloseTraderSession();

	const auto &bind = m_pp.binds[0];
	if (!IsLD())
	{
		m_pp.zone_id = bind.zoneId;
		database.MoveCharacterToZone(CharacterID(), m_pp.zone_id);
	}
	else
	{
		m_pp.zone_id = database.MoveCharacterToBind(CharacterID());
		m_Position = glm::vec4(bind.x, bind.y, bind.z, bind.heading * 0.5f);
	}

	m_pp.x = bind.x;
	m_pp.y = bind.y;
	m_pp.z = bind.z;
	m_pp.heading = bind.heading;
	m_lock_save_position = true;

	m_pp.intoxication = 0;
	m_pp.air_remaining = CalculateLungCapacity();
	Save();

	if (IsLD())
		return;

	glm::vec4 destination(bind.x, bind.y, bind.z, bind.heading);

	LogInfo(
		"Player [{}] has died and will be zoned to bind point in zone: [{}] at LOC x=[{}], y=[{}], z=[{}], heading=[{}]",
		GetName(),
		ZoneLongName(bind.zoneId),
		destination.x,
		destination.y,
		destination.z,
		destination.w
	);

	cheat_manager.SetExemptStatus(Port, true);
	SetPendingZoneTransfer(ZoneSolicited, bind.zoneId, destination, 1);
}

void Client::SetZoneFlag(uint32 zone_id) {
	if(HasZoneFlag(zone_id))
		return;

	ClearZoneFlag(zone_id);

	ZoneFlags_Struct* zfs = new ZoneFlags_Struct;
	zfs->zoneid = zone_id;
	ZoneFlags.Insert(zfs);

	std::string query = StringFormat("INSERT INTO character_zone_flags (id,zoneID) VALUES(%d,%d)", CharacterID(), zone_id);
	auto results = database.QueryDatabase(query);
	if(!results.Success())
		LogError("MySQL Error while trying to set zone flag for [{}]: [{}]", GetName(), results.ErrorMessage().c_str());
}

void Client::ClearZoneFlag(uint32 zone_id) {
	if(!HasZoneFlag(zone_id))
		return;

	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		if (zfs->zoneid == zone_id)
		{
			iterator.RemoveCurrent(true);
		}
		iterator.Advance();
	}

	std::string query = StringFormat("DELETE FROM character_zone_flags WHERE id=%d AND zoneID=%d", CharacterID(), zone_id);
	auto results = database.QueryDatabase(query);
	if(!results.Success())
		LogError("MySQL Error while trying to clear zone flag for [{}]: [{}]", GetName(), results.ErrorMessage().c_str());

}

void Client::LoadZoneFlags(LinkedList<ZoneFlags_Struct*>* ZoneFlags) 
{
	ZoneFlags->Clear();
	std::string query = StringFormat("SELECT zoneID from character_zone_flags WHERE id=%d order by zoneID", CharacterID());
	auto results = database.QueryDatabase(query);
    if (!results.Success()) {
        LogError("MySQL Error while trying to load zone flags for [{}]: [{}]", GetName(), results.ErrorMessage().c_str());
        return;
    }

	for(auto row = results.begin(); row != results.end(); ++row)
	{
		ZoneFlags_Struct* zfs = new ZoneFlags_Struct;
		zfs->zoneid = atoi(row[0]);
		ZoneFlags->Insert(zfs);
	}
}

bool Client::HasZoneFlag(uint32 zone_id) {

	if(GetGM())
		return true;

	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		if (zfs->zoneid == zone_id)
		{
			return true;
		}
		iterator.Advance();
	}
	return false;
}

void Client::SendZoneFlagInfo(Client *to) {
	if(ZoneFlags.Count() == 0) {
		to->Message(Chat::White, "%s has no zone flags.", GetName());
		return;
	}

	to->Message(Chat::White, "Flags for %s:", GetName());
	char empty[1] = { '\0' };
	LinkedListIterator<ZoneFlags_Struct*> iterator(ZoneFlags);
	iterator.Reset();
	while (iterator.MoreElements())
	{
		ZoneFlags_Struct* zfs = iterator.GetData();
		uint32 zoneid = zfs->zoneid;

		const char *zone_short_name = ZoneName(zoneid);
		std::string zone_long_name = ZoneLongName(zoneid);

		char flag_name[128];

		auto z = GetZone(zoneid);
		if (!z) {
			strcpy(flag_name, "(ERROR GETTING NAME)");
		}

		to->Message(Chat::White, "Has Flag %s for zone %s (%d,%s)", flag_name, zone_long_name.c_str(), zoneid, zone_short_name);

		iterator.Advance();
	}
}

bool Client::CanBeInZone(uint32 zoneid)
{
	//check some critial rules to see if this char needs to be booted from the zone
	//only enforce rules here which are serious enough to warrant being kicked from
	//the zone

	if(Admin() >= RuleI(GM, MinStatusToZoneAnywhere))
		return true;

	// If zoneid is 0, then we are just checking the current zone. In that case the player has already been allowed 
	// to zone, and we're checking if we should boot them to bazaar.
	const char *target_zone_name = zoneid > 0 ? ZoneName(zoneid) : zone->GetShortName();
	uint32 target_zone_id = zoneid > 0 ? zoneid : zone->GetZoneID();

	float safe_x, safe_y, safe_z, safe_heading;
	int16 min_status = 0;
	uint8 min_level = 0;

	auto z = GetZone(ZoneID(target_zone_name));
	if(!z) {
		return false;
	}

	safe_x = z->safe_x;
	safe_y = z->safe_y;
	safe_z = z->safe_z;
	safe_heading = z->safe_heading;
	min_status = z->min_status;
	min_level = z->min_level;

	if(GetLevel() < min_level) {
		LogCharacterDetail("Character does not meet min level requirement ([{}] < [{}])!", GetLevel(), min_level);
		return false;
	}
	if(Admin() < min_status) {
		LogCharacterDetail("Character does not meet min status requirement ([{}] < [{}])!", Admin(), min_status);
		return false;
	}

	if(!z->flag_needed.empty()) {
		//the flag needed string is not empty, meaning a flag is required.
		if(Admin() < minStatusToIgnoreZoneFlags && !HasZoneFlag(target_zone_id)) {
			LogInfo("Character [{}] does not have the flag to be in this zone [{}]!", GetCleanName(), z->flag_needed);
			return false;
		}
	}

	if (Admin() < minStatusToIgnoreZoneFlags && IsMule() && 
		(target_zone_id != Zones::BAZAAR && target_zone_id != Zones::NEXUS && target_zone_id != Zones::POKNOWLEDGE))
	{
		LogCharacterDetail("Character is a mule and cannot leave Bazaar/Nexus/PoK!");
		Message(Chat::Red, "Trader accounts may not leave Bazaar, Plane of Knowledge, or Nexus!");
		return false;
	}

	return true;
}

void Client::UpdateZoneChangeCount(uint32 zoneID)
{
	if(zoneID != GetZoneID() && !ignore_zone_count)
	{
		++m_pp.zone_change_count;
		ignore_zone_count = true;
	}

}

