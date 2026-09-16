#include "../../client.h"
#include "../../../common/rdp/rdp_stream.h"

void ShowNetworkStats(Client* c, const Seperator* sep)
{
	if (c == nullptr)
		return;

	Client *subject = c;
	if (c->GetTarget() != nullptr && c->GetTarget()->IsClient())
		subject = c->GetTarget()->CastToClient();

	rdplib_connection_perf_stats_t statistics{};
	if (!subject->GetNetworkStatistics(statistics))
	{
		c->Message(Chat::White, "RDP statistics are not available for %s.", subject->GetCleanName());
		return;
	}

	c->Message(Chat::White, "RDP statistics for %s:", subject->GetCleanName());
	c->Message(Chat::White, "RTT: mean %u ms, deviation %u ms, last ping %u ms", statistics.rtt_mean_ms, statistics.rtt_deviation_ms, statistics.last_ping_sample_ms);
	c->Message(Chat::White, "Queued reliable data: %u bytes, transmit stall: %u ms", statistics.queued_reliable_bytes, statistics.transmit_stall_time_ms);
	c->Message(Chat::White, "Last receive clock: %u ms, packet sequence: %u", statistics.last_packet_receive_time_ms, statistics.last_received_packet_sequence);
}

