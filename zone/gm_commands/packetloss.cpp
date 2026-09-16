#include "../client.h"

static const char *PacketLossDirectionName(rdplib_packet_drop_direction_t direction)
{
	switch (direction)
	{
	case RDPLIB_PACKET_DROP_INBOUND:
		return "inbound";
	case RDPLIB_PACKET_DROP_OUTBOUND:
		return "outbound";
	case RDPLIB_PACKET_DROP_BOTH:
		return "both directions";
	default:
		return "unknown direction";
	}
}

void command_packetloss(Client *c, const Seperator *sep)
{
	if (c->GetTarget() == nullptr || !c->GetTarget()->IsClient())
	{
		c->Message(Chat::White, "You must target a client.");
		return;
	}

	Client *client = c->GetTarget()->CastToClient();

	if (sep->argnum < 1)
	{
		RDPPacketLoss::Options options;
		if (!client->GetSimulatedPacketLossOptions(&options))
		{
			c->Message(Chat::White, "Simulated packet loss is not active for %s.", client->GetName());
			return;
		}

		c->Message(Chat::White, "Simulated packet loss for %s is %.2f%% in %s.", client->GetName(), options.percentage, PacketLossDirectionName(options.direction));
		return;
	}

	if (!sep->IsNumber(1) || sep->argnum > 2)
	{
		c->Message(Chat::White, "Usage: #packetloss [percentage] [inbound|outbound|both]");
		return;
	}

	RDPPacketLoss::Options options;
	options.percentage = atof(sep->arg[1]);
	if (options.percentage < 0.0 || options.percentage > 100.0)
	{
		c->Message(Chat::White, "Packet loss percentage must be from 0 through 100.");
		return;
	}

	if (sep->argnum == 2)
	{
		if (strcasecmp(sep->arg[2], "inbound") == 0)
			options.direction = RDPLIB_PACKET_DROP_INBOUND;
		else if (strcasecmp(sep->arg[2], "outbound") == 0)
			options.direction = RDPLIB_PACKET_DROP_OUTBOUND;
		else if (strcasecmp(sep->arg[2], "both") != 0)
		{
			c->Message(Chat::White, "Direction must be inbound, outbound, or both.");
			return;
		}
	}

	int result = client->SimulatePacketLoss(options);
	if (result != RDPLIB_OK)
	{
		c->Message(Chat::White, "Unable to change simulated packet loss.  RDP result %d.", result);
		return;
	}

	if (options.percentage == 0.0)
		c->Message(Chat::White, "Simulated packet loss is not active for %s.", client->GetName());
	else
		c->Message(Chat::White, "Simulated packet loss for %s set to %.2f%% in %s.", client->GetName(), options.percentage, PacketLossDirectionName(options.direction));
}
