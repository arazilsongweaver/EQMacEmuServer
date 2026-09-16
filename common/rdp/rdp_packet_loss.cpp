// solar: this is for simulating packet loss in the transport on a live client.

#include "rdp_packet_loss.h"

RDPPacketLoss::RDPPacketLoss(const Options &options) :
	m_options(options)
{
}

RDPPacketLoss::Options RDPPacketLoss::GetOptions() const
{
	return m_options;
}

int RDPPacketLoss::ShouldDropPacket(void *context, rdplib_packet_drop_direction_t direction, const uint8 *packet, uint32 packet_bytes)
{
	RDPPacketLoss *packet_loss = static_cast<RDPPacketLoss *>(context);
	if (packet_loss == nullptr)
		return 0;

	(void)packet;
	(void)packet_bytes;

	if ((static_cast<uint32>(packet_loss->m_options.direction) & static_cast<uint32>(direction)) == 0)
		return 0;

	return packet_loss->m_random.Real(0.0, 100.0) < packet_loss->m_options.percentage;
}
