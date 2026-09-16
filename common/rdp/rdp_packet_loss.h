// solar: this is for simulating packet loss in the transport on a live client.

#ifndef RDP_PACKET_LOSS_H
#define RDP_PACKET_LOSS_H

#include "../random.h"
#include "../types.h"

#include "rdplib.h"

class RDPPacketLoss
{
public:
	struct Options
	{
		double percentage = 0.0;
		rdplib_packet_drop_direction_t direction = RDPLIB_PACKET_DROP_BOTH;
	};

	explicit RDPPacketLoss(const Options &options);
	Options GetOptions() const;

	// This is called under the connection lock from the sending thread or the RDP I/O thread.  The packet is borrowed and is only valid during the call.
	static int ShouldDropPacket(void *context, rdplib_packet_drop_direction_t direction, const uint8 *packet, uint32 packet_bytes);

private:
	const Options m_options;
	EQ::Random m_random;
};

#endif
