#include "../client.h"

void command_zone(Client *c, const Seperator *sep)
{
	if (!sep->argnum)
	{
		c->Message(Chat::White, "Usage: #zone [Zone ID|Zone Short Name] [X] [Y] [Z]");
		return;
	}

	const std::string zone_identifier = sep->arg[1];
	uint32 zone_id = 0;
	if (zone_identifier == "0")
	{
		zone_id = c->GetZoneID();
	}
	else if (!zone_identifier.empty() && Strings::IsNumber(zone_identifier) && zone_identifier[0] != '-')
	{
		zone_id = Strings::ToUnsignedInt(zone_identifier);
	}
	else
	{
		zone_id = ZoneID(zone_identifier);
	}

	const char *zone_short_name = ZoneName(zone_id);
	if (!zone_id || !zone_short_name)
	{
		c->Message(
			Chat::White,
			fmt::format(
				"No zones were found matching '{}'.",
				zone_identifier
			).c_str()
		);
		return;
	}

	const int min_status = database.GetMinStatus(zone_id);
	if (c->Admin() < min_status)
	{
		c->Message(Chat::White, "Your status is not high enough to go to this zone.");
		return;
	}

	const bool has_x = sep->arg[2][0] != '\0';
	const bool has_y = sep->arg[3][0] != '\0';
	const bool has_z = sep->arg[4][0] != '\0';
	const bool has_coordinates = has_x && has_y && has_z;
	if ((has_x || has_y || has_z) && !has_coordinates)
	{
		c->Message(Chat::White, "Usage: #zone [Zone ID|Zone Short Name] [X] [Y] [Z]");
		return;
	}

	auto zone_data = GetZone(zone_id);
	if (!zone_data)
	{
		c->Message(Chat::White, "Unable to find the destination zone data.");
		return;
	}

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float heading = 0.0f;
	if (has_coordinates)
	{
		if (!Strings::IsFloat(sep->arg[2]) || !Strings::IsFloat(sep->arg[3]) || !Strings::IsFloat(sep->arg[4]))
		{
			c->Message(Chat::White, "Usage: #zone [Zone ID|Zone Short Name] [X] [Y] [Z]");
			return;
		}

		x = Strings::ToFloat(sep->arg[2]);
		y = Strings::ToFloat(sep->arg[3]);
		z = Strings::ToFloat(sep->arg[4]);
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
		{
			c->Message(Chat::White, "Usage: #zone [Zone ID|Zone Short Name] [X] [Y] [Z]");
			return;
		}

		heading = zone_data->safe_heading;
	}
	else
	{
		x = zone_data->safe_x;
		y = zone_data->safe_y;
		z = zone_data->safe_z;
		heading = zone_data->safe_heading;

		if (zone_identifier == "0")
		{
			c->Message(Chat::White, "Sending you to the safe coordinates of this zone.");
		}
	}

	c->MovePC(
		zone_id,
		x,
		y,
		z,
		heading,
		0,
		ZoneSolicited
	);
}

