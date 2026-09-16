#ifndef RDP_CONNECTION_H
#define RDP_CONNECTION_H

#include "../types.h"

#include "rdplib.h"

#include <chrono>

class RDPEndpoint;

// Owns one message returned by rdplib.  Its data remains valid until this object is destroyed or reused.
// A populated message also keeps the rdplib runtime busy, so the runtime must outlive it.
class RDPMessage
{
public:
	RDPMessage();
	~RDPMessage();

	RDPMessage(const RDPMessage &) = delete;
	RDPMessage &operator=(const RDPMessage &) = delete;

	const uint8 *Data() const;
	uint32 Size() const;
	uint8 StreamNumber() const;
	uint16 Flags() const;

private:
	friend class RDPConnection;

	void Reset(rdplib_message_t *message = nullptr);

	rdplib_message_t *m_message;
};

// Owns one application connection handle returned by rdplib.
// Its endpoint and runtime must outlive it.
class RDPConnection
{
public:
	enum ReceiveResult
	{
		NoData,
		MessageReceived,
		PeerClosed,
		ConnectionLost
	};

	struct Snapshot
	{
		uint8 remote_address[4] = {};
		uint16 remote_port = 0;
		// Application-observed lifetime beginning when the rdplib handle was wrapped.
		uint64 duration_ms = 0;
		ReceiveResult terminal_result = NoData;
		int terminal_send_result = RDPLIB_OK;
		uint32 disconnect_reason = 0;
		rdplib_disconnect_info_t disconnect_info = {};
		rdplib_connection_counters_t counters = {};
		rdplib_connection_perf_stats_t performance = {};
	};

	~RDPConnection();

	RDPConnection(const RDPConnection &) = delete;
	RDPConnection &operator=(const RDPConnection &) = delete;

	int EnableKeepalive();
	int EnableKeepalive(uint32 interval_ms);
	int SetDataRate(uint32 bytes_per_second);
	int SetSendBufferSize(uint32 bytes);

	// The data is borrowed for this call.  rdplib copies it before returning.
	int Send(const void *data, uint32 bytes, uint32 stream, uint32 flags);

	// This does not block.  MessageReceived gives ownership of the message contents to RDPMessage.
	ReceiveResult Receive(RDPMessage *message, uint32 *disconnect_reason = nullptr);

	// borrowed until Receive or Close, do not release
	const rdplib_message_t *PeekMessage() const;

	// Releases the application connection handle and returns immediately.
	void Close(uint32 linger_timeout_ms);

	int GetRemoteAddress(uint8 address[4], uint16 &port) const;
	int GetCounters(rdplib_connection_counters_t &counters) const;
	int GetStatistics(rdplib_connection_perf_stats_t &statistics) const;
	int GetDisconnectInfo(rdplib_disconnect_info_t &information) const;
	// The individual rdplib records are captured sequentially while the connection is still usable.
	int GetSnapshot(Snapshot &snapshot) const;
	int SetPacketDropCallback(rdplib_packet_drop_callback_t callback, void *context = nullptr);

private:
	friend class RDPEndpoint;

	explicit RDPConnection(rdplib_connection_t *connection);
	void DiscardMessages();

	rdplib_connection_t *m_connection;
	ReceiveResult m_terminal_result;
	int m_terminal_send_result;
	uint32 m_disconnect_reason;
	std::chrono::steady_clock::time_point m_started_at;
};

#endif
