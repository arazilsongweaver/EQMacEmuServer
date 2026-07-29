/*
	Copyright (C) 2005 Michael S. Finger

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

#include "global_define.h"
#include "eqemu_logsys.h"
#include "eq_packet.h"
#include "eq_stream.h"
#include "op_codes.h"
#include "platform.h"
#include "strings.h"
#include "rulesys.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

#ifdef _WINDOWS
	#include <time.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <fcntl.h>
	#include <arpa/inet.h>
#endif

//for logsys
#define _L "[{}]:[{}]: "
#define __L , long2ip(remote_ip).c_str(), ntohs(remote_port)

Fragment::Fragment()
{
	data = 0;
	size = 0;
	memset(this->data, 0, this->size);
}

Fragment::~Fragment()
{
	data = 0;
	size = 0;
}

void Fragment::SetData(uchar* d, uint32 s)
{
	safe_delete_array(data);
	size = s;
	data = new uchar[s];
	memcpy(data, d, s);
}

void Fragment::ClearData()
{
	safe_delete_array(data);
}

FragmentGroup::FragmentGroup(uint16 seq, uint16 opcode, uint16 num_fragments)
{
	this->seq = seq;
	this->opcode = opcode;
	this->num_fragments = num_fragments;
	fragment = new Fragment[num_fragments];
}

FragmentGroup::~FragmentGroup()
{
	for(int i=0;i<num_fragments;i++)
	{
		fragment[i].ClearData();
	}
	safe_delete_array(fragment);//delete[] fragment;
}

void FragmentGroup::Add(uint16 frag_id, uchar* data, uint32 size)
{
	//The frag_id references a fragment within the group
	if(frag_id < num_fragments)
	{
		fragment[frag_id].SetData(data, size);
	}
	//The frag_id is attempting to reference an element outside the bounds of the group array
	else
	{
		return;
	}
}

uchar* FragmentGroup::AssembleData(uint32* size)
{
	uchar* buf;
	uchar* p;
	int i;

	*size = 0;	
	for(i=0; i<num_fragments; i++)
	{
		*size+=fragment[i].GetSize();		
	}
	buf = new uchar[*size];
	p = buf;
	for(i=0; i<num_fragments; i++)
	{
		memcpy(p, fragment[i].GetData(), fragment[i].GetSize());
		p += fragment[i].GetSize();
	}
	return buf;
}


void FragmentGroupList::Add(FragmentGroup* add_group)
{
	fragment_group_list.push_back(add_group);
}

FragmentGroup* FragmentGroupList::Get(uint16 find_seq)
{
	std::vector<FragmentGroup *>::iterator it;
	it = fragment_group_list.begin();
	while (it != fragment_group_list.end()) {
		if ((*it)->GetSeq() == find_seq)
			return (*it);
		++it;
	}
	return nullptr;
}

void FragmentGroupList::Remove(uint16 remove_seq)
{
	std::vector<FragmentGroup *>::iterator it;
	it = fragment_group_list.begin();
	while (it != fragment_group_list.end()) {
		if ((*it)->GetSeq() == remove_seq) {
			safe_delete(*it);
			it = fragment_group_list.erase(it);
			return;
		}
		++it;
	}
}

void FragmentGroupList::RemoveAll()
{
	for (std::vector<FragmentGroup*>::iterator it = fragment_group_list.begin(); it != fragment_group_list.end(); ++it) {
		safe_delete(*it);
	}
}

EQOldStream::EQOldStream(sockaddr_in in, int fd_sock)
{
	pm_state = ESTABLISHED;
	dwLastCACK = 0;
	dwLastARSP = 0;
	dwFragSeq  = 0;
	listening_socket = fd_sock;
	datarate_timer = new Timer(100);
	no_ack_sent_timer = new Timer(250);
	bTimeout = false;
	bTimeoutTrigger = false;
	sent_Start = false;
	sent_Fin = false;

	/* 
		on a normal server there is always data getting sent 
		so the packetloss indicator in eq stays at 0%

		This timer will send dummy data if nothing has been sent
		in 1000ms. This is not needed. When the client doesnt
		get any data it will send a special ack request to ask
		if we are still alive. The eq client will show around 40%
		packetloss at this time. It is not real packetloss. Its
		just thinking there is packetloss since no data is sent.
		The EQ servers doesnt have a timer like this one.

		short version: This timer is not needed, it just keeps
		the green bar green.
	*/
	keep_alive_timer = new Timer(450);
	no_ack_sent_timer->Disable();
	dataflow = 0;
	SetDataRate(RuleI(World,StreamDataRate) * 10);
	debug_level = 0;
	LOG_PACKETS = false;
	isWriting = false;
	OpMgr = nullptr;
	remote_ip = in.sin_addr.s_addr; //in.sin_addr.S_un.S_addr; 
	remote_port = in.sin_port;
	packetspending = 0;
	active_users = 0;
	LastPacket=0;
	RateThreshold=RATEBASE/10;
	DecayRate=DECAYBASE/10;
	bTimeoutTrigger = false;
}

EQOldStream::EQOldStream()
{
	pm_state = ESTABLISHED;
	dwLastCACK = 0;
	dwLastARSP = 0;
	dwFragSeq  = 0;
	listening_socket = 0;		    
	datarate_timer = new Timer(100);
	no_ack_sent_timer = new Timer(250);
	bTimeout = false;
	bTimeoutTrigger = false;
	sent_Start = false;
	sent_Fin = false;
	dataflow = 0;
	SetDataRate(RuleI(World, StreamDataRate) * 5);
	arsp_response = 0;
	/* 
		on a normal server there is always data getting sent 
		so the packetloss indicator in eq stays at 0%

		This timer will send dummy data if nothing has been sent
		in 1000ms. This is not needed. When the client doesnt
		get any data it will send a special ack request to ask
		if we are still alive. The eq client will show around 40%
		packetloss at this time. It is not real packetloss. Its
		just thinking there is packetloss since no data is sent.
		The EQ servers doesnt have a timer like this one.

		short version: This timer is not needed, it just keeps
		the green bar green.
	*/
	keep_alive_timer = new Timer(450);
	no_ack_sent_timer->Disable();

	debug_level = 0;
	LOG_PACKETS = false;
	OpMgr = nullptr;
	remote_ip = 0; 
	remote_port = 0;
	packetspending = 0;
	active_users = 0;
	LastPacket=0;
	isWriting = false;
	RateThreshold=RATEBASE/10;
	DecayRate=DECAYBASE/10;
}

EQOldStream::~EQOldStream()
{
	LogNetcodeDetail("Killing EQOldStream");
	safe_delete(no_ack_sent_timer);//delete no_ack_sent_timer;
	safe_delete(keep_alive_timer);//delete keep_alive_timer;
	safe_delete(datarate_timer);
	LogNetcodeDetail("Killing outbound and inbound packet queue");
	RemoveData();
	SetState(CLOSED);
}

void EQOldStream::ResendBefore(uint16 dwARQ)
{
	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	//LogNetcodeDetail(_L "Resend Before Called [{}] B0." __L, dwARQ);
	std::deque<EQOldPacket*>::iterator it;
	for (it = SendQueue.begin(); it != SendQueue.end();)
	{
		if ((*it)->HDR.a1_ARQ && !(*it)->acked) {
			if (dwARQ > 5000) {
				if ((*it)->dwARQ <= dwARQ) {
					if (dwARQ > 60000) {
						if ((*it)->dwARQ > 5000) {
							(*it)->Resend = true;
							//LogNetcodeDetail(_L "Flagging [{}] for resend B2." __L, (*it)->dwARQ);
						}
					}
					else {
						(*it)->Resend = true;
						//LogNetcodeDetail(_L "Flagging [{}] for resend B3." __L, (*it)->dwARQ);
					}
				}
			}
			else {
				if ((*it)->dwARQ <= dwARQ || (*it)->dwARQ > 60000) {
					(*it)->Resend = true;
					//LogNetcodeDetail(_L "Flagging [{}] for resend B4." __L, (*it)->dwARQ);
				}
			}
		}
		it++;
	}
}

void EQOldStream::ResendRequest(uint16 count_size, const uchar* bits, uint16 arsp_start) {
	if (count_size == 0 || bits == nullptr)
		return;
	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	int j = 0;
	int i = 0;
	std::deque<EQOldPacket*>::iterator it;
	uint8 bit_fields = bits[0];
	uint16 match_arsp = arsp_start;
	match_arsp++;
	for (it = SendQueue.begin(); it != SendQueue.end() && i < count_size;)
	{
		if ((*it)->HDR.a1_ARQ && (*it)->dwARQ == match_arsp) {
			if (bit_fields & 0x80) {
				// this is an ack actually
				(*it)->Resend = false;
				(*it)->acked = true;
				//LogNetcodeDetail(_L "Acking packet [{}]." __L, match_arsp);
			} else {
				(*it)->Resend = true;
				//LogNetcodeDetail(_L "Flagging [{}] for resend." __L, match_arsp);
			}
			j++;
			match_arsp++;
			if (j < 8) {
				bit_fields <<= 1;
			}
			else {
				j = 0;
				i++;
				if (i == count_size)
					break;
				bit_fields = bits[i];
			}
		}
		else {
			it++;
		}
	}
}

void EQOldStream::IncomingARSP(uint16 dwARSP) 
{
	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	EQOldPacket* pack = 0;
	
	//LogNetcodeDetail( _L "ARSP Received [{}]. ARSP0" __L, dwARSP);
	bool acked;
	std::deque<EQOldPacket*>::iterator it;
	for(it = SendQueue.begin(); it != SendQueue.end();)
	{
		if ((*it)->HDR.a1_ARQ) {
			acked = false;
			if (dwARSP > 5000) {
				if ((*it)->dwARQ <= dwARSP) {
					if (dwARSP > 60000) {
						if ((*it)->dwARQ > 5000)
							acked = true;
					}
					else {
						acked = true;
					}
				}
			}
			else {
				if ((*it)->dwARQ <= dwARSP || (*it)->dwARQ > 60000) {
					acked = true;
				}
			}
			if (acked) {
				//LogNetcodeDetail(_L "Acking Packet [{}]." __L, (*it)->dwARQ);
				safe_delete(*it);
				it = SendQueue.erase(it);
				dwLastARSP = dwARSP;
			} else {
				it++;
			}
		}
		else {
			it++;
		}
	}
}

void EQOldStream::IncomingARQ(uint16 dwARQ) 
{
	uint16 nextACK = dwLastCACK;
	nextACK++;
	if (dwARQ == nextACK) {
		CACK.dwARQ = dwARQ;
		dwLastCACK = dwARQ;
	}
			    
	if (!no_ack_sent_timer->Enabled())
	{
		no_ack_sent_timer->Start(250); // Agz: If we dont get any outgoing packet we can put an 
		// ack response in before 250ms has passed we send a pure ack response        if (debug_level >= 2)
//            cout << Timer::GetCurrentTime() << " no_ack_sent_timer->Start(200)" << endl;
	}
}

void EQOldStream::OutgoingARQ(uint16 dwARQ)   //An ack request is sent
{
}

void EQOldStream::OutgoingARSP(void)
{
	no_ack_sent_timer->Disable(); // Agz: We have sent the ack response
//    if (debug_level >= 2)
//        cout << Timer::GetCurrentTime() << " no_ack_sent_timer->Disable()" << endl;
}

/************ PARCE A EQPACKET ************/
void EQOldStream::ParceEQPacket(uint16 dwSize, uchar* pPacket)
{
	if(pm_state != EQStreamState::ESTABLISHED)
		return;

	std::lock_guard<std::mutex> lock(MInboundOldQueue);
	/************ DECODE PACKET ************/
	EQOldPacket* pack = new EQOldPacket(pPacket, dwSize);
	pack->DecodePacket(dwSize, pPacket);
	if (ProcessPacket(pack, false))
	{
		safe_delete(pack);//delete pack;
	}
	CheckBufferedPackets();
}

void EQOldStream::RemoveData()
{
	std::lock(MInboundOldQueue, MOutboundOldQueue);
	std::lock_guard<std::mutex> inlock(MInboundOldQueue, std::adopt_lock);
	std::lock_guard<std::mutex> outlock(MOutboundOldQueue, std::adopt_lock);
	EQRawApplicationPacket* p = 0;	
	std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
	while (itr != OutQueue.end()) {
			safe_delete(*itr);
			itr++;
	}
	OutQueue.clear();
	// Send all the packets we "made"
	for(int i = 0; i < buffered_packets.size(); i++) {
		safe_delete(buffered_packets[i]);
	}
	buffered_packets.clear();
	fragment_group_list.RemoveAll();
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);

		safe_delete(p);
		packit = SendQueue.erase(packit);
	}
}

/*
	Return true if its safe to delete this packet now, if we buffer it return false
	this way i can skip memcpy the packet when buffering it
*/
bool EQOldStream::ProcessPacket(EQOldPacket* pack, bool from_buffer)
{
	// simulate incoming packet loss
	//if (pack->HDR.a1_ARQ && rand() % 100 < 20) {
	//	Log(Logs::Detail, Logs::Netcode, _L "Dropping incoming packet to simulate packet loss arq %i" __L, pack->dwARQ);
	//	return true;
	//}
	/************ CHECK FOR ACK/SEQ RESET ************/ 
	if(pack->HDR.a5_SEQStart)
	{
		if (dwLastCACK == (unsigned int)pack->dwARQ - (unsigned int)1)
		{
			LogNetcodeDetail(_L "[EQOldStream] Packet invalid seqstart? {}:{}" __L, pack->dwARQ, pack->dwSEQ);
			return true;
		}
		//      cout << "resetting SACK.dwGSQ1" << endl;
		//      SACK.dwGSQ      = 0;            //Main sequence number SHORT#2
		dwLastCACK      = pack->dwARQ-1;//0;
		LogNetcodeDetail(_L "[EQOldStream] Packet seqstart [{}]:[{}]" __L, pack->dwARQ, pack->dwSEQ);
		//      CACK.dwGSQ = 0xFFFF; changed next if to else instead
	}

	CACK.dwGSQ = pack->dwSEQ; //Get current sequence #.
	// LogNetcodeDetail(_L "Packet incoming arq[{}]:seq[{}]:arsp[{}]" __L, pack->dwARQ, pack->dwSEQ, pack->dwARSP);

	/************ Process ack responds ************/
	// Quagmire: Moved this to above "ack request" checking in case the packet is dropped in there
	if (pack->HDR.b2_ARSP) {
		IncomingARSP(pack->dwARSP);
	}
	if (pack->HDR.b3_Unknown) {
		ResendBefore(pack->b3ARSP);
	} 
	if (pack->b4_size > 0) {
		ResendRequest(pack->b4_size, pack->ack_fields, pack->dwARSP);
	}

	/************ End process ack rsp ************/

	// Does this packet contain an ack request?
	if(pack->HDR.a1_ARQ)
	{
		uint16 dwnextCACK = dwLastCACK;
		dwnextCACK++;
		if (pack->dwARQ != dwnextCACK) {
			// Is this packet a resend we have already processed?
			if ((pack->dwARQ == dwLastCACK) || // this was last ack request
				(dwLastCACK > 5000 && pack->dwARQ < dwLastCACK && (dwLastCACK < 60000 || (dwLastCACK > 60000 && pack->dwARQ > 5000))) || // in past
				(dwLastCACK < 5000 && (pack->dwARQ > 60000 || pack->dwARQ < dwLastCACK))) { // in past
				// LogNetcodeDetail( _L "Packet discarded [{}]:[{}]" __L, pack->dwARQ, pack->dwSEQ);
				return true;
			}

			// Debug check, if we want to buffer a packet we got from the buffer something is wrong...
			if (from_buffer)
			{
				return true;
				//cerr << "ERROR: Rebuffering a packet in EQPacketManager::ProcessPacket" << endl;
			}

			std::vector<EQOldPacket*>::iterator iterator;
			iterator = buffered_packets.begin();
			while(iterator != buffered_packets.end())
			{
				if ((*iterator)->dwARQ == pack->dwARQ)
				{
					// LogNetcodeDetail( _L "Packet already buffered [{}]" __L, pack->dwARQ);
					return true; // This packet was already buffered
				}
				iterator++;
			}
			if (!no_ack_sent_timer->Enabled())
				no_ack_sent_timer->Start(250);

			// LogNetcodeDetail( _L "Buffering Packet [{}]" __L, pack->dwARQ);
			buffered_packets.push_back(pack);

			return false;
		}
	}

	// this is for dumping the bit flag header
	/*char* str;
	if (pack->b4_size > 0) {
		int j = 0;
		str = new char[pack->b4_size * 8 + 1];
		for (int i = 0; i < pack->b4_size; i++) {
			for (int k = 0; k < 8; k++) {
				sprintf(&str[i * 8 + k], "%s", pack->ack_fields[i] & 0x80 ? "1" : "0");
				pack->ack_fields[i] <<= 1;
			}

		}
		str[pack->b4_size * 8 + 1] = '\0';
	}
	LogNetcodeDetail( _L "Packet incoming arq%i:seq%i:arsp%i a0:%d|a1:%d|a2:%d|a3:%d|a4:%d|a5:%d|a6:%d|a7:%d|b0:%d|b1:%d|b2:%d|b3:%d|b4:%s|b5:%d|b6:%d|b7:%d" __L,
	pack->dwARQ, pack->dwSEQ, pack->dwARSP,
	pack->HDR.a0_Unknown ? 1 : 0,
	pack->HDR.a1_ARQ ? pack->dbASQ_low : 0,
	pack->HDR.a2_Closing ? 1 : 0,
	pack->HDR.a3_Fragment ? 1 : 0,
	pack->HDR.a4_ASQ ? pack->dbASQ_high : 0,
	pack->HDR.a5_SEQStart ? 1 : 0,
	pack->HDR.a6_Closing ? 1 : 0,
	pack->HDR.a7_SEQEnd ? 1 : 0,
	pack->HDR.b0_SpecARQ ? 1 : 0,
	pack->HDR.b1_Unknown ? 1 : 0,
	pack->HDR.b2_ARSP ? 1 : 0,
	pack->HDR.b3_Unknown ? pack->b3ARSP : 0,
	pack->b4_size > 0 ? str : "0",
	pack->HDR.b5_Unknown ? 1 : 0,
	pack->HDR.b6_Unknown ? 1 : 0,
	pack->HDR.b7_Unknown ? 1 : 0);*/

	/************ START ACK REQ CHECK ************/
	if (pack->HDR.a1_ARQ)
		IncomingARQ(pack->dwARQ);

	if (pack->HDR.b0_SpecARQ || pack->HDR.a5_SEQStart) {
		// Send the ack reponse right away.
		no_ack_sent_timer->Start(250);
		no_ack_sent_timer->Trigger();
	}
	/************ END ACK REQ CHECK ************/

	/************ CHECK FOR THREAD TERMINATION ************/
	if(pack->HDR.a2_Closing && pack->HDR.a6_Closing)
	{
		EQStreamState state = GetState();
		if(state == ESTABLISHED) {
			//client initiated disconnect?
			LogNetcodeDetail(_L "[EQOldStream] Received OP_SessionDisconnect. Treating like a client-initiated disconnect." __L);
			_SendDisconnect();
			SetState(CLOSED);
		} else if(state == CLOSING) {
			//we were waiting for this anyways, ignore pending messages, send the reply and be closed.
			LogNetcodeDetail(_L "[EQOldStream] Received OP_SessionDisconnect when we have a pending close, they beat us to it. Were happy though." __L);
			_SendDisconnect();
			SetState(CLOSED);
		} else {
			//we are expecting this (or have already gotten it, but dont care either way)
			LogNetcodeDetail(_L "[EQOldStream] Received expected OP_SessionDisconnect. Moving to closed state." __L);
			SetState(CLOSED);
		}
		return true;
	}
	/************ END CHECK THREAD TERMINATION ************/

	/************ Get ack sequence number ************/
	if(pack->HDR.a4_ASQ) {
		CACK.dbASQ_high = pack->dbASQ_high;
		//LogNetcodeDetail(_L "Packet Flags a1:%d dbASQ_low: %d a4:%d dbASQ_high: %d" __L, pack->HDR.a1_ARQ, pack->dbASQ_low, pack->HDR.a4_ASQ, pack->dbASQ_high);
	
		if(pack->HDR.a1_ARQ)
			CACK.dbASQ_low = pack->dbASQ_low;
	}
	/************ End get ack seq num ************/

	/************ START FRAGMENT CHECK ************/
	/************ IF - FRAGMENT ************/
	if(pack->HDR.a3_Fragment) 
	{
		FragmentGroup* fragment_group = 0;
		fragment_group = fragment_group_list.Get(pack->fraginfo.dwSeq);

		// If we dont have a fragment group with the right sequence number, create a new one
		if (fragment_group == 0)
		{
			fragment_group = new FragmentGroup(pack->fraginfo.dwSeq,pack->dwOpCode, pack->fraginfo.dwTotal);
			fragment_group_list.Add(fragment_group);
		}

		// Add this fragment to the fragment group
		fragment_group->Add(pack->fraginfo.dwCurr, pack->pExtra,pack->dwExtraSize);

		// If we have all the fragments to complete this group
		if(pack->fraginfo.dwCurr == (pack->fraginfo.dwTotal - 1) )
		{
			//Collect fragments and put them as one packet on the OutQueue
			uchar* buf = 0;
			uint32 sizep = 0;
			buf = fragment_group->AssembleData(&sizep);

			//Why does the client sometimes send fragments with 0-size packets, and why do they get completed?
			//This should help prevent a crash.
			if(sizep == 0 || !buf || fragment_group->GetOpcode() == 0)
			{
				safe_delete_array(buf);
				return true;
			}

			EQRawApplicationPacket *app = new EQRawApplicationPacket(fragment_group->GetOpcode(), buf, sizep);
			safe_delete_array(buf);
			OutQueue.push_back(app);
			fragment_group_list.Remove(pack->fraginfo.dwSeq);
			return true;
		}
		else
		{
			return true;
		}
	}
	/************ ELSE - NO FRAGMENT ************/
	else 
	{

		//seems to be happening, hardening this crap
		if(!pack || pack && !pack->pExtra && pack->dwExtraSize > 0 || pack->dwOpCode == 0)
		{
			return true;
		}


		EQRawApplicationPacket *app=MakeApplicationPacket(pack);
		//if(app->GetRawOpcode() != 62272 && (app->GetRawOpcode() != 0 || app->Size() > 2)) //ClientUpdate
		//	LogNetcodeDetail("Received old opcode - 0x%x size: %i", app->GetRawOpcode(), app->Size());
		if(app)
			OutQueue.push_back(app);
		return true;
	}
	/************ END FRAGMENT CHECK ************/

	return true;
}

void EQOldStream::CheckBufferedPackets()
{
// Should use a hash table or sorted list instead....
	int num=0; // Counting buffered packets for debug output
	uint16 nextARQ = dwLastCACK;
	nextARQ++;
	std::vector<EQOldPacket*>::iterator iterator;
	iterator = buffered_packets.begin();
	while(iterator != buffered_packets.end())
	{
		// Check if we have a packet we want already buffered
		if ((*iterator)->dwARQ == nextARQ)
		{
			//LogNetcodeDetail( _L "Processing Packet fromn Buffer Seq # [{}]" __L, nextARQ);
			ProcessPacket((*iterator), true);
			safe_delete(*iterator);
			iterator = buffered_packets.erase(iterator);
			iterator = buffered_packets.begin();
			nextARQ++;
		}
		else
		{
			iterator++;
		}
	}
}

void EQOldStream::MakeClosePacket()
{
	EQOldPacket *pack = new EQOldPacket();
	pack->HDR.a6_Closing    = 1;// Agz: Lets try to uncomment this line again
	pack->HDR.a2_Closing    = 1;// and this
	pack->HDR.a1_ARQ        = 1;// and this
	pack->acked = false;
//      pack->dwARQ             = 1;// and this, no that was not too good
	pack->dwARQ             = SACK.dwARQ++;// try this instead
	SendQueue.push_back(pack);
	return;
}


/************************************************************************/
/************ Make an EQ packet and put it to the send queue ************/
/* 
	APP->size == 0 && app->pBuffer == nullptr if no data.

	Agz: set ack_req = false if you dont want this packet to require an ack
	response from the client, this menas this packet may get lost and not
	resent. This is used by the EQ servers for HP and position updates among 
	other things. WARNING: I havent tested this yet.
*/
void EQOldStream::MakeEQPacket(EQProtocolPacket* app, bool ack_req, bool outboundAlreadyLocked)
{
	int16 restore_op = 0x0000;

	/************ PM STATE = NOT ACTIVE ************/
	if(CheckState(CLOSED) || CheckState(CLOSING) || CheckState(DISCONNECTING) || !app || app && app->GetRawOpcode() == 0)
	{
		return;
	}

	// Agz:Moved this to after finish check
	if(app == nullptr)
	{
		//cout << "EQPacketManager::MakeEQPacket app == nullptr" << endl;
		return;
	}
	bool bFragment= false; //This is set later on if fragseq should be increased at the end.
	std::unique_lock<std::mutex> lock;
	if (!outboundAlreadyLocked) {
		lock = std::unique_lock<std::mutex>(MOutboundOldQueue);
	}

	/************ IF opcode is == 0xFFFF it is a request for pure ack creation ************/
	if(app->GetRawOpcode() == 0xFFFF)
	{
		EQOldPacket *pack = new EQOldPacket();
		if (ack_req) {
			pack->HDR.a1_ARQ = 1;
			pack->dwARQ = SACK.dwARQ++;
			pack->acked = false;
			SACK.dwGSQcount = 0;
		}
		//pack->dwOpCode = 0xFFFF;
		keep_alive_timer->Start();
		SendQueue.push_back(pack);
		return;
	}
	if (app->size == 19) {
		if (app->opcode == 0x9f40) {
			// we have a mob update opcode - see if the back of the sendqueue has one to add this to
			if (!SendQueue.empty()) {
				EQOldPacket *oldpack;
				oldpack = SendQueue.back();
				if (oldpack->resend_count == 0 && oldpack->dwOpCode == 0x9f40) {
					// have matching OP_MobUpdate
					uint32 count = (oldpack->dwExtraSize - 4) / 15;
					if (count < 30) {
						count++;
						uchar *newdata = new uchar[4 + count * 15];
						uchar *olddata = oldpack->pExtra;

						memcpy((void*)newdata, (void*)olddata, oldpack->dwExtraSize);
						memcpy((void*)newdata, &count, sizeof(uint32));
						newdata += oldpack->dwExtraSize;
						app->pBuffer += 4;
						memcpy((void*)newdata, (void*)app->pBuffer, 15);
						newdata -= oldpack->dwExtraSize;
						app->pBuffer -= 4;
						oldpack->dwExtraSize = (uint16)(4 + count * 15);
						oldpack->pExtra = newdata;
						delete[] olddata;
						return;
					}
				}

			}
		}
	}

	/************ CHECK PACKET MANAGER STATE ************/
	int fragsleft = (app->size >> 9);
	
	if(fragsleft)
	{
		ack_req = true; // Fragmented packets must have ackreq set
	}

	if(CheckState(EQStreamState::ESTABLISHED))
	/************ PM STATE = ACTIVE ************/
	{
		for (int i=0; i<=fragsleft; i++)
		{
			EQOldPacket *pack = new EQOldPacket();
			//IF NON PURE ACK THEN ALWAYS INCLUDE A ACKSEQ              // Agz: Not anymore... Always include ackseq if not a fragmented packet
			if (i==0 && ack_req) // If this will be a fragmented packet, only include ackseq in first fragment
				pack->HDR.a4_ASQ = 1;                                   // This is what the eq servers does

			/************ Caculate the next ACKSEQ/acknumber ************/
			/************ Check if its a static ackseq ************/
			if( HI_BYTE(app->GetRawOpcode()) == 0x2000)
			{
				if(app->size == 15)
					pack->dbASQ_low = 0xb2;
				else
					pack->dbASQ_low = 0xa1;

			}
			/************ Normal ackseq ************/
			else
			{
				//If not pure ack and opcode != 0x20XX then
				if (ack_req) { // If the caller of this function wants us to put an ack request on this packet
					pack->HDR.a1_ARQ = 1;
					pack->acked = false;
					pack->dwARQ = SACK.dwARQ++;
					SACK.dwGSQcount = 0;
				}

				if(pack->HDR.a1_ARQ && pack->HDR.a4_ASQ) {
					pack->dbASQ_low  = SACK.dbASQ_low;
					pack->dbASQ_high = SACK.dbASQ_high;
				}
				else if(pack->HDR.a4_ASQ) {
					pack->dbASQ_high = SACK.dbASQ_high;
				}
				if(pack->HDR.a4_ASQ)
					SACK.dbASQ_low++;
			}

			/************ Check if this packet should contain op ************/
			if (app->GetRawOpcode() && i == 0) {
				pack->dwOpCode = app->GetRawOpcode();
			}
			/************ End opcode check ************/

			/************ SHOULD THIS PACKET BE SENT AS A FRAGMENT ************/
			if(fragsleft) {
				pack->HDR.a3_Fragment = 1;
			}
			/************ END FRAGMENT CHECK ************/

			if (i == 0) {
				if (LogSys.log_settings[Logs::PacketServerClient].is_category_enabled == 1) {
					EmuOpcode app_opcode = (*OpMgr)->EQToEmu(app->opcode);
					if (app_opcode != OP_SpecialMesg &&
						(!RuleB(EventLog, SkipCommonPacketLogging) ||
							(RuleB(EventLog, SkipCommonPacketLogging) && app_opcode != OP_MobHealth && app_opcode != OP_MobUpdate && app_opcode != OP_ClientUpdate))) {
						LogPacketServerClient("[{}] - [{:#06x}] Size: [{}] {}", OpcodeManager::EmuToName(app_opcode), app->opcode, app->size, DumpProtocolPacketToString(app).c_str());
					}
				}
			}

			if(app->size && app->pBuffer)
			{
				if(pack->HDR.a3_Fragment)
				{
					// If this is the last packet in the fragment group
					if(i == fragsleft) {
						// Calculate remaining bytes for this fragment
						pack->dwExtraSize = app->size-510-512*((app->size/512)-1);
					}
					else if(i == 0) {
						pack->dwExtraSize = 510; // The first packet in a fragment group has 510 bytes for data
					}
					else {
						pack->dwExtraSize = 512; // Other packets in a fragment group has 512 bytes for data
					}
					pack->fraginfo.dwCurr	= i;
					pack->fraginfo.dwSeq	= dwFragSeq;
					pack->fraginfo.dwTotal	= fragsleft + 1;
				}
				else
				{
					pack->dwExtraSize = (uint16)app->size;
				}

				pack->pExtra = new uchar[pack->dwExtraSize];
				memcpy((void*)pack->pExtra, (void*)app->pBuffer, pack->dwExtraSize);
				app->pBuffer += pack->dwExtraSize; //Increase counter
			}
			/************ End update timers ************/

			SendQueue.push_back(pack);
		}//end while

		if(fragsleft)
		{
			dwFragSeq++;
		}
		app->pBuffer -= app->size; //Restore ptr.
		app->opcode = restore_op;
	} //end if
}

void EQOldStream::CheckTimers(void)
{
	/************ Should packets be resent? ************/

	if (datarate_timer->Check(0))
	{
		datarate_timer->Start();
		dataflow -= datarate_tic;
		if (dataflow < 0)
			dataflow = 0;
	}
}

void EQOldStream::QueuePacket(const EQApplicationPacket *p, bool ack_req)
{
//	ack_req = true;	// It's broke right now, dont delete this line till fix it. =P

	if(p == nullptr)
		return;

	if(OpMgr == nullptr || *OpMgr == nullptr) {
		LogNetcode("[EQOldStream] Packet enqueued into a stream with no opcode manager, dropping.");
		delete p;
		return;
	}
	uint16 opcode = (*OpMgr)->EmuToEQ(p->emu_opcode);
	EQProtocolPacket* pack2 = new EQProtocolPacket(opcode, p->pBuffer, p->size);
	MakeEQPacket( pack2, ack_req);
	delete pack2;
}

void EQOldStream::FastQueuePacket(EQApplicationPacket **p, bool ack_req)
{
	EQApplicationPacket *pack=*p;
	*p = nullptr;		//clear caller's pointer.. effectively takes ownership

	if(pack == nullptr)
		return;

	if(OpMgr == nullptr || *OpMgr == nullptr) {
		LogNetcodeDetail("[EQOldStream] Packet enqueued into a stream with no opcode manager, dropping.");
		delete pack;
		return;
	}

	uint16 opcode = (*OpMgr)->EmuToEQ(pack->emu_opcode);

//	ack_req = true;	// It's broke right now, dont delete this line till fix it. =P

	if(p == nullptr)
		return;

	EQProtocolPacket* pack2 = new EQProtocolPacket(opcode, pack->pBuffer, pack->size);

	//if(pack->emu_opcode != OP_MobUpdate && pack->emu_opcode != OP_MobHealth && pack->emu_opcode != OP_HPUpdate)
	//	LogNetcodeDetail( _L "Sending old opcode 0x%04x" __L, opcode);
	MakeEQPacket(pack2, ack_req);
	delete pack;
	delete pack2;
}

EQApplicationPacket *EQOldStream::PopPacket()
{
	EQRawApplicationPacket *p=nullptr;
	std::unique_lock<std::mutex> inlock(MInboundOldQueue);
	if (OutQueue.size()) {
		std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
		p=*itr;
		OutQueue.erase(itr);
	}
	inlock.unlock();

	if(p)
	{
		if(p->GetRawOpcode() == 0 || p->GetRawOpcode() == 0xFFFF || p->size > 0 && !p->pBuffer)
		{
			safe_delete_array(p->pBuffer);
			safe_delete(p);
			return nullptr;
		}
	}
	//resolve the opcode if we can. do not resolve protocol packets in oldstreams
	if(p) {
		if(OpMgr != nullptr && *OpMgr != nullptr && p->GetRawOpcode() != 0 && p->GetRawOpcode() != 0xFFFF) {
			EmuOpcode emu_op = (*OpMgr)->EQToEmu(p->GetRawOpcode());
			p->SetOpcode(emu_op);
		}
	}
	return p;
}


void EQOldStream::InboundQueueClear()
{
	EQRawApplicationPacket *p=nullptr;

	LogNetcode(_L "[EQOldStream] Clearing inbound queue" __L);

	std::lock_guard<std::mutex> lock(MInboundOldQueue);
	std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
	while (itr != OutQueue.end()) {
			safe_delete(*itr);
			itr++;
	}
	OutQueue.clear();
	for(int i = 0; i < buffered_packets.size(); i++) {
		safe_delete(buffered_packets[i]);
	}
	buffered_packets.clear();
}

void EQOldStream::OutboundQueueClear()
{
	EQOldPacket *p=nullptr;

	LogNetcodeDetail(_L "[EQOldStream] Clearing outbound & resend queue" __L);

	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);

		safe_delete(p);
		packit = SendQueue.erase(packit);
	}
	SendQueue.clear();
}
void EQOldStream::ReceiveData(uchar* buf, int len)
{
	ParceEQPacket(len, buf);
}

void EQOldStream::SendPacketQueue(bool Block)
{
	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	// Get first send packet on queue and send it!
	EQOldPacket* pack = 0;
	sockaddr_in to;	
	uint32 size;
	uchar* data;
	memset((char *) &to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = remote_port;
	to.sin_addr.s_addr = remote_ip;
	int sentpacket = 0;
	uint32 time_now = Timer::GetCurrentTime();
	std::deque<EQOldPacket*>::iterator packit = SendQueue.begin();

	/************ Should a pure ack be sent? ************/
	if (GetState() == ESTABLISHED) {
		if ((no_ack_sent_timer->Check(0) || keep_alive_timer->Check()))
		{
			EQProtocolPacket app(0xFFFF, nullptr, 0);
			MakeEQPacket(&app, true, true); // outbound is already locked
			no_ack_sent_timer->Disable();
		}
	} else if (GetState() == CLOSING) {
		if (!keep_alive_timer->Enabled()) {
			keep_alive_timer->Start(450);
			keep_alive_timer->Trigger();
		}
		if (keep_alive_timer->Check()) {
			// flag resends so we can clear out queue when closing
			packit = SendQueue.begin();
			while (packit != SendQueue.end()) {
				pack = (*packit);
				if (!pack->acked)
				{
					pack->Resend = false;
					pack->LastSent = 0;
				}
				packit++;
			}
		}
	}
	packit = SendQueue.begin();
	while (packit != SendQueue.end() && !DataQueueFull()) {
		pack = (*packit);
		if(pack != nullptr && (pack->resend_count == 0 || pack->LastSent == 0 || // first time sent packets
			(!pack->acked && 
				(pack->Resend && ((time_now - pack->LastSent) > 20)) || // flagged for resend, but was just barely sent
				(!pack->Resend && (pack->resend_count) < 10 && ((time_now - pack->LastSent) > 3000))))) // packet timeout
		{
			if (pack->HDR.a2_Closing && pack->HDR.a6_Closing) //Closing bits. Terminates the connection properly.
			{
				size = pack->ReturnPacket(&data, this);
				sendto(listening_socket, (char*) data, size, 0, (sockaddr*) &to, sizeof(to));
				//LogNetcodeDetail(_L "Sending Closing Packet [{}]." __L, pack->dwARQ);
				dataflow += size;
				pack->LastSent = Timer::GetCurrentTime();
				pack->Resend = false;
				pack->resend_count++;
				safe_delete_array(data);
				if (pack->resend_count > 10) {
					safe_delete(pack);
					packit = SendQueue.erase(packit);
				}
				else {
					packit++;
				}
				continue;
			}
			else //Send a packet!
			{
				if (pack->HDR.a1_ARQ) {
					// if we are too far out on arq's, this will help prevent client from going into desync
					if (pack->dwARQ > dwLastARSP && (pack->dwARQ - dwLastARSP - 100) >= 0)
						break;
					if (pack->dwARQ < dwLastARSP && (65535 - dwLastARSP + pack->dwARQ - 100) >= 0)
						break;
					keep_alive_timer->Disable();
				}
				if (!pack->Resend)
					sentpacket++;
				size = pack->ReturnPacket(&data, this);
				pack->resend_count++;
				pack->Resend = false;

				// uncomment to simulate outgoing packet loss
				//if (pack->resend_count > 25 || rand() % 100 < 20) {
				//	if (pack->HDR.a1_ARQ)
				//		LogNetcodeDetail(_L "Sending Packet [{}]." __L, pack->dwARQ);
				sendto(listening_socket, (char*)data, size, 0, (sockaddr*)&to, sizeof(to));
				//}
				safe_delete_array(data);
				dataflow += size;
				pack->LastSent = Timer::GetCurrentTime();

				if (pack->HDR.a1_ARQ && !pack->HDR.a3_Fragment && (sentpacket > 10) && GetState() == ESTABLISHED) {
					break;
				}

				if (!pack->HDR.a1_ARQ && !pack->HDR.a3_Fragment) {
					safe_delete(pack);
					packit = SendQueue.erase(packit);
					continue;
				}
			}
		}
		packit++;
	}
	if (sentpacket > 0)
		keep_alive_timer->Start(450);
	else if(!keep_alive_timer->Enabled())
		keep_alive_timer->Start(3000);
	// ************ Processing finished ************ //
}

void EQOldStream::FlagPacketQueueForResend()
{
	if (GetState() == CLOSED)
		return;

	std::lock_guard<std::mutex> lock(MOutboundOldQueue);

	uint32 size;
	uchar* data;
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);
		if (p->HDR.a1_ARQ || p->HDR.a3_Fragment) {
			if (!p->acked) {
				p->LastSent = 0;
				p->Resend = false;
			}
			if (p->Resend) {
				p->LastSent = 0;
				p->Resend = false;
			}
			packit++;
		}
		else {
			safe_delete(p);
			packit = SendQueue.erase(packit);
		}
	}

	// ************ Connection finished ************ //
}

void EQOldStream::ClearPacketQueue()
{
	if (GetState() == CLOSED)
		return;

	std::lock_guard<std::mutex> lock(MOutboundOldQueue);

	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);

		safe_delete(p);
		packit = SendQueue.erase(packit);
	}

}

void EQOldStream::FinalizePacketQueue()
{
	if(GetState() == CLOSED)
		return;

	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	// Send out our existing queue
	EQOldPacket* p = 0;    
	sockaddr_in to;	
	memset((char *) &to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = remote_port;
	to.sin_addr.s_addr = remote_ip;

	uint32 size;
	uchar* data;
	// Set state to closing, and send off the finalized packet.
	if (GetState() != CLOSING)
		MakeClosePacket();
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		p = (*packit);
		if (p->HDR.a1_ARQ || p->HDR.a3_Fragment) {
			if (!p->acked) {
				size = p->ReturnPacket(&data, this);
				sendto(listening_socket, (char*)data, size, 0, (sockaddr*)&to, sizeof(to));
				safe_delete_array(data);
			}
			packit++;
		}
		else {
			safe_delete(p);
			packit = SendQueue.erase(packit);
			continue;
		}
	}
	// ************ Connection finished ************ //
}

bool EQOldStream::HasOutgoingData()
{
	bool flag;

	std::lock_guard<std::mutex> lock(MOutboundOldQueue);
	flag=!(SendQueue.empty());
	return flag;
}


void EQOldStream::CheckTimeout(uint32 now, uint32 timeout) {
	bool outgoing_data = HasOutgoingData();	//up here to avoid recursive locking

	EQStreamState orig_state = GetState();
	if (orig_state == CLOSING && !outgoing_data) {
		LogNetcode(_L "[EQOldStream] Out of data in closing state, disconnecting." __L);
		SetState(CLOSED);
	} else if (LastPacket && (now-LastPacket) > timeout) {
		switch(orig_state) {
		case CLOSING:
			//if we time out in the closing state, they are not acking us, just give up
			LogNetcode(_L "[EQOldStream] Timeout expired in closing state. Moving to closed state." __L);
			ClearPacketQueue();
			SetState(CLOSED);
			break;
		case DISCONNECTING:
			//we timed out waiting for them to send us the disconnect reply, just give up.
			LogNetcodeDetail(_L "[EQOldStream] Timeout expired in disconnecting state. Moving to closed state." __L);
			ClearPacketQueue();
			SetState(CLOSED);
			break;
		case CLOSED:
			LogNetcodeDetail(_L "[EQOldStream] Timeout expired in closed state??" __L);
			ClearPacketQueue();
			break;
		case ESTABLISHED:
			//we timed out during normal operation. Try to be nice about it.
			//we will almost certainly time out again waiting for the disconnect reply, but oh well.
			LogNetcodeDetail(_L "[EQOldStream] Timeout expired in established state. Closing connection." __L);
			_SendDisconnect();
			SetState(DISCONNECTING);
			break;
		default:
			break;
		}
	}
}

void EQOldStream::SetState(EQStreamState state) {
	std::lock_guard<std::mutex> lock(MState);

	if(pm_state == CLOSED)
	{
		return;
	}

	LogNetcodeDetail("[{}]:[{}]: [EQOldStream] Changing state from [{}] to [{}]", long2ip(remote_ip), ntohs(remote_port), (int)pm_state, (int)state);
	pm_state=state;
}

void EQOldStream::SetStreamType(EQStreamType type)
{
	LogNetcodeDetail("{}:{}: [EQOldStream] Changing stream type from {} to {}", long2ip(remote_ip), ntohs(remote_port), StreamTypeString(StreamType), StreamTypeString(type));
	StreamType=type;
}

const char *EQOldStream::StreamTypeString(EQStreamType type)
{
	return "OldStream";
}

//this could be expanded to check more than the fitst opcode if
//we needed more complex matching
EQStreamInterface::MatchState EQOldStream::CheckSignature(const EQStreamInterface::Signature *sig) {
	EQRawApplicationPacket *p = nullptr;
	EQStreamInterface::MatchState res = EQStreamInterface::MatchNotReady;
	std::lock_guard<std::mutex> lock(MInboundOldQueue);
	if (!OutQueue.empty()) {
		//this is already getting hackish...
		std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
		p=*itr;
		if(sig->ignore_eq_opcode != 0 && p->GetRawOpcode() == sig->ignore_eq_opcode) {
			if(OutQueue.empty()) {
				p = nullptr;
			}
		}
		if(p == nullptr) {
			//first opcode is ignored, and nothing else remains... keep waiting
		} else if(p->GetRawOpcode() == sig->first_eq_opcode) {
			//opcode matches, check length..
			if(p->size == sig->first_length) {
				LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode matched [{:#06x}] and length matched [{}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size);
				res = EQStreamInterface::MatchSuccessful;
			} else if(sig->first_length == 0) {
				LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode matched [{:#06x}] and length [({})] is ignored", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size);
				res = EQStreamInterface::MatchSuccessful;
			} else {
				//opcode matched but length did not.
				LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode matched [{:#06x}], but length [{}] did not match expected [{}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size, sig->first_length);
				res = EQStreamInterface::MatchFailed;
			}
		} else {
			//first opcode did not match..
			LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode [{:#06x}] did not match expected [{:#06x}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), p->GetRawOpcode(), sig->first_eq_opcode);
			res = EQStreamInterface::MatchFailed;
		}
	}

	return(res);
}

void EQOldStream::_SendDisconnect()
{
	FinalizePacketQueue();
}

void EQOldStream::Close() {
	// don't queue a second FIN packet here if we get called again while already closing
	EQStreamState state = GetState();
	if (state == CLOSING || state == DISCONNECTING || state == CLOSED) {
		return;
	}

	if(HasOutgoingData()) {
		//there is pending data, wait for it to go out.
		LogNetcode("[{}]:[{}]: EQOldStream requested to Close(), but there is pending data, waiting for it.", long2ip(remote_ip), ntohs(remote_port));
		std::unique_lock<std::mutex> outlock(MOutboundOldQueue);
		MakeClosePacket();
		outlock.unlock();
		keep_alive_timer->Stop();
		FlagPacketQueueForResend();
		SetState(CLOSING);
	} else {
		//otherwise, we are done, we can drop immediately.
		_SendDisconnect();
		LogNetcodeDetail("{}:{}: [EQOldStream] closing immediate due to Close()", long2ip(remote_ip).c_str(), ntohs(remote_port));
		SetState(DISCONNECTING);
	}
}

EQRawApplicationPacket *EQOldStream::MakeApplicationPacket(EQOldPacket *p)
{
	EQRawApplicationPacket *ap=nullptr;
	LogNetcodeDetail("{}:{}: [EQOldStream] Creating old application packet, length {}", long2ip(remote_ip), ntohs(remote_port), p->dwExtraSize);
	ap = p->MakeAppPacket();
	return ap;
}
