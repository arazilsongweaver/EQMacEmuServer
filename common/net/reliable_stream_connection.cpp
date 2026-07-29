#include "reliable_stream_connection.h"
#include "../event/event_loop.h"
#include "../data_verification.h"
#include "crc32.h"
#include <zlib.h>
#include <fmt/format.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

// Legacy per-pass resend throttle. Per-stream outstanding limits are enforced separately.
constexpr size_t MAX_RESEND_PACKETS_PER_PASS = 300;
constexpr size_t MAX_RESEND_BYTES_PER_PASS   = 140 * 1024;
constexpr size_t MIN_RAW_PACKET_SIZE         = 64;
constexpr int MAX_RELIABLE_SEQUENCE_DISTANCE = 30000; // UdpManager::cHardMaxOutstandingPackets in Sony's original UdpLibrary.

namespace
{
	bool IsSupportedProtocolVersion(uint32_t protocol_version)
	{
		// The EQMac UdpLibrary uses version 2.
		return protocol_version == 2 || protocol_version == 3;
	}

	bool IsValidEncodeType(EQ::Net::ReliableStreamEncodeType encode_type)
	{
		return encode_type == EQ::Net::EncodeNone || encode_type == EQ::Net::EncodeCompression || encode_type == EQ::Net::EncodeXOR;
	}

	bool IsValidCrcLength(size_t crc_length)
	{
		// Sony's UdpLibrary supports every truncated CRC length from zero through four bytes.
		return crc_length <= 4;
	}

	size_t EncodeExpansionBytes(const EQ::Net::ReliableStreamEncodeType *encode_passes)
	{
		size_t expansion = 0;
		for (int pass = 0; pass < 2; ++pass) {
			if (encode_passes[pass] == EQ::Net::EncodeCompression) {
				++expansion;
			}
		}

		return expansion;
	}

	void NormalizeOptions(EQ::Net::ReliableStreamConnectionManagerOptions &options)
	{
		options.max_packet_size = EQ::Clamp(options.max_packet_size, MIN_RAW_PACKET_SIZE, UDP_BUFFER_SIZE);
		options.hold_size = std::min(options.hold_size, options.max_packet_size);

		if (options.tic_rate_hertz <= 0.0) {
			options.tic_rate_hertz = 60.0;
		}

		if (!IsValidCrcLength(options.crc_length)) {
			options.crc_length = 0;
		}

		if (!IsSupportedProtocolVersion(options.protocol_version)) {
			options.protocol_version = 3;
		}

		for (auto &encode_pass : options.encode_passes) {
			if (!IsValidEncodeType(encode_pass)) {
				encode_pass = EQ::Net::EncodeNone;
			}
		}

		if (options.resend_delay_factor < 0.0) {
			options.resend_delay_factor = 0.0;
		}

		options.resend_delay_max = std::max<size_t>(1, options.resend_delay_max);
		options.resend_delay_min = std::min(options.resend_delay_min, options.resend_delay_max);

		for (int stream = 0; stream < 4; ++stream) {
			options.max_instanding_packets[stream] = EQ::Clamp<size_t>(options.max_instanding_packets[stream], 1, MAX_RELIABLE_SEQUENCE_DISTANCE);
			options.max_outstanding_packets[stream] = EQ::Clamp<size_t>(options.max_outstanding_packets[stream], 1, MAX_RELIABLE_SEQUENCE_DISTANCE);
			options.max_outstanding_bytes[stream] = std::max<size_t>(1, options.max_outstanding_bytes[stream]);
		}
	}
}

// buffer pools
SendBufferPool send_buffer_pool;

EQ::Net::ReliableStreamConnectionManager::ReliableStreamConnectionManager()
{
	m_attached = nullptr;
	NormalizeOptions(m_options);
	memset(&m_timer, 0, sizeof(uv_timer_t));
	memset(&m_socket, 0, sizeof(uv_udp_t));

	Attach(EQ::EventLoop::Get().Handle());
}

EQ::Net::ReliableStreamConnectionManager::ReliableStreamConnectionManager(const ReliableStreamConnectionManagerOptions &opts)
{
	m_attached = nullptr;
	m_options = opts;
	NormalizeOptions(m_options);
	memset(&m_timer, 0, sizeof(uv_timer_t));
	memset(&m_socket, 0, sizeof(uv_udp_t));

	Attach(EQ::EventLoop::Get().Handle());
}

EQ::Net::ReliableStreamConnectionManager::~ReliableStreamConnectionManager()
{
	Detach();
}

void EQ::Net::ReliableStreamConnectionManager::Attach(uv_loop_t *loop)
{
	if (!m_attached) {
		uv_timer_init(loop, &m_timer);
		m_timer.data = this;

		auto update_rate = std::max<uint64_t>(1, (uint64_t)(1000.0 / m_options.tic_rate_hertz));

		uv_timer_start(&m_timer, [](uv_timer_t *handle) {
			ReliableStreamConnectionManager *c = (ReliableStreamConnectionManager*)handle->data;
			c->UpdateDataBudget();
			c->Process();
			c->ProcessResend();
		}, update_rate, update_rate);

		uv_udp_init(loop, &m_socket);
		m_socket.data = this;
		struct sockaddr_in recv_addr;
		uv_ip4_addr("0.0.0.0", m_options.port, &recv_addr);
		int rc = uv_udp_bind(&m_socket, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);

		rc = uv_udp_recv_start(
			&m_socket,
			[](uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
				if (suggested_size > 65536) {
					buf->base = new char[suggested_size];
					buf->len  = suggested_size;
					return;
				}

				static thread_local char temp_buf[65536];
				buf->base = temp_buf;
				buf->len  = 65536;
			},
			[](uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
			if (nread >= 0 && addr != nullptr) {
				ReliableStreamConnectionManager *c = (ReliableStreamConnectionManager*)handle->data;
				char endpoint[16];
				uv_ip4_name((const sockaddr_in*)addr, endpoint, 16);
				auto port = ntohs(((const sockaddr_in*)addr)->sin_port);
				c->ProcessPacket(endpoint, port, buf->base, nread);
			}

			if (buf->len > 65536) {
				delete[] buf->base;
			}
		});

		m_attached = loop;
	}
}

void EQ::Net::ReliableStreamConnectionManager::Detach()
{
	if (m_attached) {
		uv_udp_recv_stop(&m_socket);
		uv_timer_stop(&m_timer);
		m_attached = nullptr;
	}
}

void EQ::Net::ReliableStreamConnectionManager::Connect(const std::string &addr, int port)
{
	//todo dns resolution

	auto connection = std::shared_ptr<ReliableStreamConnection>(new ReliableStreamConnection(this, addr, port));
	connection->m_self = connection;

	if (m_on_new_connection) {
		m_on_new_connection(connection);
	}

	m_connections.emplace(std::make_pair(std::make_pair(addr, port), connection));
}

void EQ::Net::ReliableStreamConnectionManager::Process()
{
	auto now = Clock::now();
	auto iter = m_connections.begin();
	while (iter != m_connections.end()) {
		auto connection = iter->second;
		auto status = connection->m_status;

		if (status == StatusDisconnecting) {
			auto time_since_close = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection->m_close_time);
			if (time_since_close.count() > m_options.connection_close_time) {
				connection->ChangeStatus(StatusDisconnected);
				iter = m_connections.erase(iter);
				continue;
			}
		}

		if (status == StatusConnecting) {
			auto time_since_last_recv = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection->m_last_recv);
			if ((size_t)time_since_last_recv.count() > m_options.connect_stale_ms) {
				connection->Close();
				iter++;
				continue;
			}
		}
		else if (status == StatusConnected) {
			auto time_since_last_recv = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection->m_last_recv);
			if ((size_t)time_since_last_recv.count() > m_options.stale_connection_ms) {
				connection->Close();
				iter++;
				continue;
			}
		}

		switch (status)
		{
			case StatusConnecting: {
				auto time_since_last_send = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection->m_last_send);
				if ((size_t)time_since_last_send.count() > m_options.connect_delay_ms) {
					connection->SendConnect();
				}
				break;
			}
			case StatusConnected: {
				if (m_options.keepalive_delay_ms != 0) {
					auto time_since_last_send = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection->m_last_send);
					if ((size_t)time_since_last_send.count() > m_options.keepalive_delay_ms) {
						connection->SendKeepAlive();
					}
				}
				connection->Process();
				break;
			}
			case StatusDisconnecting: {
				break;
			}
			default:
				break;
		}

		iter++;
	}
}

void EQ::Net::ReliableStreamConnectionManager::UpdateDataBudget()
{
	auto outgoing_data_rate = m_options.outgoing_data_rate;
	if (outgoing_data_rate <= 0.0) {
		return;
	}

	auto update_rate = std::max<uint64_t>(1, (uint64_t)(1000.0 / m_options.tic_rate_hertz));
	auto budget_add = update_rate * outgoing_data_rate / 1000.0;

	auto iter = m_connections.begin();
	while (iter != m_connections.end()) {
		auto &connection = iter->second;
		connection->UpdateDataBudget(budget_add);

		iter++;
	}
}

void EQ::Net::ReliableStreamConnectionManager::ProcessResend()
{
	auto iter = m_connections.begin();
	while (iter != m_connections.end()) {
		auto &connection = iter->second;
		auto status = connection->m_status;

		switch (status)
		{
			case StatusConnected:
				connection->ProcessResend();
				break;
			default:
				break;
		}

		iter++;
	}
}

void EQ::Net::ReliableStreamConnectionManager::ProcessPacket(const std::string &endpoint, int port, const char *data, size_t size)
{
	if (m_options.simulated_in_packet_loss && m_options.simulated_in_packet_loss >= m_rand.Int(0, 100)) {
		return;
	}

	if (size < ReliableStreamHeader::size()) {
		if (m_on_error_message) {
			m_on_error_message(fmt::format("Packet of size {0} which is less than {1}", size, ReliableStreamHeader::size()));
		}
		return;
	}

	try {
		auto connection = FindConnectionByEndpoint(endpoint, port);
		if (connection) {
			StaticPacket p((void*)data, size);
			connection->ProcessPacket(p);
		}
		else {
			if (data[0] == 0 && data[1] == OP_SessionRequest) {
				if (size < ReliableStreamConnect::size() || size > m_options.max_packet_size || m_options.max_packet_size < MIN_RAW_PACKET_SIZE) {
					return;
				}

				StaticPacket p((void*)data, size);
				auto request = p.GetSerialize<ReliableStreamConnect>(0);
				auto protocol_version = NetworkToHost(request.protocol_version);
				auto requested_max_packet_size = NetworkToHost(request.max_packet_size);
				if (!IsSupportedProtocolVersion(protocol_version) || requested_max_packet_size < MIN_RAW_PACKET_SIZE || size > requested_max_packet_size) {
					return;
				}

				if (m_options.max_connection_count != 0 && m_connections.size() >= m_options.max_connection_count) {
					return;
				}

				connection = std::shared_ptr<ReliableStreamConnection>(new ReliableStreamConnection(this, request, endpoint, port));
				connection->m_self = connection;

				if (m_on_new_connection) {
					m_on_new_connection(connection);
				}
				m_connections.emplace(std::make_pair(std::make_pair(endpoint, port), connection));
				connection->ProcessPacket(p);
			}
			else if (data[1] != OP_UnreachableConnection && data[1] != OP_SessionDisconnect) {
				SendUnreachableConnection(endpoint, port);
			}
		}
	}
	catch (std::exception &ex) {
		if (m_on_error_message) {
			m_on_error_message(fmt::format("Error processing packet: {0}", ex.what()));
		}
	}
}

std::shared_ptr<EQ::Net::ReliableStreamConnection> EQ::Net::ReliableStreamConnectionManager::FindConnectionByEndpoint(std::string addr, int port)
{
	auto p = std::make_pair(addr, port);
	auto iter = m_connections.find(p);
	if (iter != m_connections.end()) {
		return iter->second;
	}

	return nullptr;
}

void EQ::Net::ReliableStreamConnectionManager::SendUnreachableConnection(const std::string &addr, int port)
{
	// UdpLibrary calls this an UnreachableConnection packet. It has no
	// connection state, so it is deliberately only the unencoded header.
	ReliableStreamHeader header;
	header.zero = 0;
	header.opcode = OP_UnreachableConnection;

	DynamicPacket out;
	out.PutSerialize(0, header);

	uv_udp_send_t *send_req = new uv_udp_send_t;
	sockaddr_in send_addr;
	uv_ip4_addr(addr.c_str(), port, &send_addr);
	uv_buf_t send_buffers[1];

	char *data = new char[out.Length()];
	memcpy(data, out.Data(), out.Length());
	send_buffers[0] = uv_buf_init(data, out.Length());
	send_req->data = send_buffers[0].base;
	int ret = uv_udp_send(send_req, &m_socket, send_buffers, 1, (sockaddr*)&send_addr,
		[](uv_udp_send_t* req, int status) {
		delete[](char*)req->data;
		delete req;
	});
	if (ret < 0) {
		delete[] data;
		delete send_req;
	}
}

//new connection made as server
EQ::Net::ReliableStreamConnection::ReliableStreamConnection(ReliableStreamConnectionManager *owner, const ReliableStreamConnect &connect, const std::string &endpoint, int port)
{
	m_owner = owner;
	m_last_send = Clock::now();
	m_last_recv = Clock::now();
	m_status = StatusConnected;
	m_endpoint = endpoint;
	m_port = port;
	m_connect_code = NetworkToHost(connect.connect_code);
	m_encode_key = m_owner->m_rand.Int(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
	m_max_packet_size = (uint32_t)std::min(owner->m_options.max_packet_size, (size_t)NetworkToHost(connect.max_packet_size));
	m_crc_bytes = (uint32_t)owner->m_options.crc_length;
	m_encode_passes[0] = owner->m_options.encode_passes[0];
	m_encode_passes[1] = owner->m_options.encode_passes[1];
	m_hold_time = Clock::now();
	m_buffered_packets_length = 0;
	m_rolling_ping = 800; // UdpReliableChannel starts conservatively until it has measured RTT samples.
	m_combined.reset(new char[m_max_packet_size]);
	m_combined[0] = 0;
	m_combined[1] = OP_Combined;
	m_last_session_stats = Clock::now();
	m_outgoing_budget = owner->m_options.outgoing_data_rate;
	m_silent_disconnect = false;
	InitializeReliableStreams();

	LogNetClient("New session [{}] with encode key [{}]", m_connect_code, HostToNetwork(m_encode_key));
}

//new connection made as client
EQ::Net::ReliableStreamConnection::ReliableStreamConnection(ReliableStreamConnectionManager *owner, const std::string &endpoint, int port)
{
	m_owner = owner;
	m_last_send = Clock::now();
	m_last_recv = Clock::now();
	m_status = StatusConnecting;
	m_endpoint = endpoint;
	m_port = port;
	m_connect_code = m_owner->m_rand.Int(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
	m_encode_key = 0;
	m_max_packet_size = (uint32_t)owner->m_options.max_packet_size;
	m_crc_bytes = 0;
	m_encode_passes[0] = EncodeNone;
	m_encode_passes[1] = EncodeNone;
	m_hold_time = Clock::now();
	m_buffered_packets_length = 0;
	m_rolling_ping = 800;
	m_combined.reset(new char[m_max_packet_size]);
	m_combined[0] = 0;
	m_combined[1] = OP_Combined;
	m_last_session_stats = Clock::now();
	m_outgoing_budget = owner->m_options.outgoing_data_rate;
	m_silent_disconnect = false;
	InitializeReliableStreams();
}

EQ::Net::ReliableStreamConnection::~ReliableStreamConnection()
{
}

void EQ::Net::ReliableStreamConnection::InitializeReliableStreams()
{
	size_t encode_expansion = EncodeExpansionBytes(m_encode_passes);
	size_t max_reliable_data_size = m_max_packet_size - m_crc_bytes - ReliableStreamReliableHeader::size() - encode_expansion;

	for (int stream_id = 0; stream_id < 4; ++stream_id) {
		auto stream = &m_streams[stream_id];
		stream->max_reliable_data_size = max_reliable_data_size;
		stream->congestion_window_minimum = std::max(max_reliable_data_size, m_owner->m_options.congestion_window_minimum[stream_id]);
		stream->congestion_window_start = std::min(4 * max_reliable_data_size, std::max(2 * max_reliable_data_size, static_cast<size_t>(4380)));
		stream->congestion_window_start = std::max(stream->congestion_window_start, stream->congestion_window_minimum);
		stream->congestion_slow_start_threshold = std::min(
			m_owner->m_options.max_outstanding_packets[stream_id] * max_reliable_data_size,
			m_owner->m_options.max_outstanding_bytes[stream_id]);
		stream->congestion_window_size = stream->congestion_window_start;
		stream->congestion_window_largest = stream->congestion_window_start;
		stream->maxed_out_current_window = false;
	}
}

void EQ::Net::ReliableStreamConnection::ResetCongestionWindowIfIdle(ReliableStream *stream)
{
	if (stream->sent_packets.empty() && stream->pending_packets.empty()) {
		stream->congestion_window_size = stream->congestion_window_start;
		stream->congestion_slow_start_threshold = stream->congestion_window_largest;
		stream->maxed_out_current_window = false;
	}
}

void EQ::Net::ReliableStreamConnection::DiscardTransportQueues()
{
	m_buffered_packets.clear();
	m_buffered_packets_length = 0;

	for (auto &stream : m_streams) {
		stream.packet_queue.clear();
		stream.ResetFragment();

		while (!stream.pending_packets.empty()) {
			stream.pending_packets.pop();
		}

		stream.sent_packets.clear();
		stream.pending_bytes = 0;
		stream.outstanding_bytes = 0;
		stream.sequence_out_pending = stream.sequence_out;
		stream.maxed_out_current_window = false;
	}
}

size_t EQ::Net::ReliableStreamConnection::TotalPendingReliableBytes() const
{
	size_t total = 0;
	for (const auto &stream : m_streams) {
		total += stream.pending_bytes + stream.outstanding_bytes;
	}

	return total;
}

void EQ::Net::ReliableStreamConnection::Close()
{
	auto status = m_status;
	if (status == StatusDisconnected || status == StatusDisconnecting) {
		return;
	}

	m_close_time = Clock::now();
	if (status == StatusConnecting) {
		m_silent_disconnect = true;
	}

	ChangeStatus(StatusDisconnecting);
	DiscardTransportQueues();

	if (status == StatusConnected && !m_silent_disconnect) {
		SendDisconnect();
	}
}

void EQ::Net::ReliableStreamConnection::QueuePacket(Packet &p)
{
	QueuePacket(p, 0, true);
}

void EQ::Net::ReliableStreamConnection::QueuePacket(Packet &p, int stream)
{
	QueuePacket(p, stream, true);
}

void EQ::Net::ReliableStreamConnection::QueuePacket(Packet &p, int stream, bool reliable)
{
	if (p.Length() == 0 || stream < 0 || stream >= 4 || m_status != StatusConnected) {
		return;
	}

	if (*(char*)p.Data() == 0) {
		DynamicPacket packet;
		packet.PutUInt8(0, 0);
		packet.PutPacket(1, p);
		InternalQueuePacket(packet, stream, reliable);
		return;
	}

	InternalQueuePacket(p, stream, reliable);
}

EQ::Net::ReliableStreamConnectionStats EQ::Net::ReliableStreamConnection::GetStats()
{
	EQ::Net::ReliableStreamConnectionStats ret = m_stats;
	ret.datarate_remaining = m_outgoing_budget;
	ret.avg_ping = m_rolling_ping;

	return ret;
}

void EQ::Net::ReliableStreamConnection::ResetStats()
{
	m_stats.Reset();
}

void EQ::Net::ReliableStreamConnection::Process()
{
	try {
		if (!m_buffered_packets.empty()) {
			auto now = Clock::now();
			auto time_since_hold = (size_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - m_hold_time).count();
			if (time_since_hold >= m_owner->m_options.hold_length_ms) {
				FlushBuffer();
			}
		}

		ProcessQueue();
	}
	catch (std::exception &ex) {
		if (m_owner->m_on_error_message) {
			m_owner->m_on_error_message(fmt::format("Error processing connection: {0}", ex.what()));
		}
	}
}

void EQ::Net::ReliableStreamConnection::ProcessPacket(Packet &p)
{
	if (p.Length() < ReliableStreamHeader::size() || p.Length() > m_max_packet_size) {
		return;
	}

	m_stats.recv_packets++;
	m_stats.recv_bytes += p.Length();

	bool packet_can_be_encoded = PacketCanBeEncoded(p);
	if (m_status == StatusConnecting && packet_can_be_encoded) {
		// Encryption, compression, and CRC settings are not known until OP_SessionResponse arrives.
		return;
	}

	if (packet_can_be_encoded) {
		size_t clear_header_size = p.GetInt8(0) == 0 ? ReliableStreamHeader::size() : 1;
		if (p.Length() < clear_header_size + m_crc_bytes) {
			return;
		}

		if (!ValidateCRC(p)) {
			if (m_owner->m_on_error_message) {
				m_owner->m_on_error_message(fmt::format("Tossed packet that failed CRC of type {0:#x}", p.Length() >= 2 ? p.GetInt8(1) : 0));
			}

			m_stats.bytes_after_decode += p.Length();
			return;
		}

		if (m_encode_passes[0] == EncodeCompression || m_encode_passes[1] == EncodeCompression)
		{
			EQ::Net::DynamicPacket temp;
			temp.PutPacket(0, p);
			temp.Resize(temp.Length() - m_crc_bytes);

			for (int i = 1; i >= 0; --i) {
				switch (m_encode_passes[i]) {
					case EncodeCompression:
						if (temp.GetInt8(0) == 0) {
							if (!Decompress(temp, ReliableStreamHeader::size(), temp.Length() - ReliableStreamHeader::size())) {
								return;
							}
						}
						else if (!Decompress(temp, 1, temp.Length() - 1)) {
							return;
						}
						break;
					case EncodeXOR:
						if (temp.GetInt8(0) == 0)
							Decode(temp, ReliableStreamHeader::size(), temp.Length() - ReliableStreamHeader::size());
						else
							Decode(temp, 1, temp.Length() - 1);
						break;
					default:
						break;
				}
			}

			m_last_recv = Clock::now();
			m_stats.bytes_after_decode += temp.Length();
			ProcessDecodedPacket(StaticPacket(temp.Data(), temp.Length()));
		}
		else {
			EQ::Net::StaticPacket temp(p.Data(), p.Length() - m_crc_bytes);

			for (int i = 1; i >= 0; --i) {
				switch (m_encode_passes[i]) {
					case EncodeXOR:
						if (temp.GetInt8(0) == 0)
							Decode(temp, ReliableStreamHeader::size(), temp.Length() - ReliableStreamHeader::size());
						else
							Decode(temp, 1, temp.Length() - 1);
						break;
					default:
						break;
				}
			}

			m_last_recv = Clock::now();
			m_stats.bytes_after_decode += temp.Length();
			ProcessDecodedPacket(StaticPacket(temp.Data(), temp.Length()));
		}
	}
	else {
		m_last_recv = Clock::now();
		m_stats.bytes_after_decode += p.Length();
		ProcessDecodedPacket(p);
	}
}

void EQ::Net::ReliableStreamConnection::ProcessQueue()
{
	for (int i = 0; i < 4; ++i) {
		auto stream = &m_streams[i];
		for (;;) {

			auto iter = stream->packet_queue.find(stream->sequence_in);
			if (iter == stream->packet_queue.end()) {
				break;
			}

			DynamicPacket packet(std::move(iter->second));
			stream->packet_queue.erase(iter);
			ProcessDecodedPacket(packet);
		}
	}
}

void EQ::Net::ReliableStreamConnection::RemoveFromQueue(int stream, uint16_t seq)
{
	auto s = &m_streams[stream];
	s->packet_queue.erase(seq);
}

void EQ::Net::ReliableStreamConnection::AddToQueue(int stream, uint16_t seq, const Packet &p)
{
	auto s = &m_streams[stream];
	auto iter = s->packet_queue.find(seq);
	if (iter == s->packet_queue.end()) {
		DynamicPacket out;
		out.PutPacket(0, p);
		s->packet_queue.emplace(seq, std::move(out));
	}
}

void EQ::Net::ReliableStreamConnection::RejectInvalidFragment(ReliableStream *stream, const char *reason)
{
	if (m_owner->m_on_error_message) {
		m_owner->m_on_error_message(fmt::format("Invalid fragment: {}", reason));
	}

	stream->ResetFragment();
	Close();
}

void EQ::Net::ReliableStreamConnection::ProcessDecodedPacket(const Packet &p)
{
	if (p.Length() == 0) {
		return;
	}

	if (p.GetInt8(0) == 0) {
		if (p.Length() < 2) {
			return;
		}

			switch (p.GetInt8(1)) {
			case OP_KeepAlive:
			case OP_OutboundPing:
				break;

			case OP_UnreachableConnection:
				// Port remapping is not enabled here, so use UdpLibrary's normal fallback and end the stale connection.
				Close();
				break;

			case OP_RequestRemap:
				// If this reached an existing connection, its endpoint mapping is already correct.
				break;

			case OP_Combined: {
				if (m_status == StatusDisconnecting) {
					if (!m_silent_disconnect) {
						SendDisconnect();
					}
					return;
				}

				char *current = (char*)p.Data() + 2;
				char *end = (char*)p.Data() + p.Length();
				while (current < end) {
					uint8_t subpacket_length = *(uint8_t*)current;
					current += 1;

					if (subpacket_length == 0 || subpacket_length > static_cast<size_t>(end - current)) {
						return;
					}

					ProcessDecodedPacket(StaticPacket(current, subpacket_length));
					current += subpacket_length;
				}
				break;
			}

			case OP_AppCombined:
			{
				if (m_status == StatusDisconnecting) {
					if (!m_silent_disconnect) {
						SendDisconnect();
					}
					return;
				}

				uint8_t *current = (uint8_t*)p.Data() + 2;
				uint8_t *end = (uint8_t*)p.Data() + p.Length();

				while (current < end) {
					uint32_t subpacket_length = 0;
					size_t remaining = static_cast<size_t>(end - current);

					if (*current == 0xFF) {
						if (remaining < 3) {
							throw std::out_of_range("OP_AppCombined has an incomplete length field");
						}

						if (current[1] == 0xFF && current[2] == 0xFF) {
							if (remaining < 7) {
								throw std::out_of_range("OP_AppCombined has an incomplete extended length field");
							}

							subpacket_length = (static_cast<uint32_t>(current[3]) << 24) | (static_cast<uint32_t>(current[4]) << 16) | (static_cast<uint32_t>(current[5]) << 8) | static_cast<uint32_t>(current[6]);
							current += 7;
						}
						else {
							subpacket_length = (static_cast<uint32_t>(current[1]) << 8) | static_cast<uint32_t>(current[2]);
							current += 3;
						}
					}
					else {
						subpacket_length = static_cast<uint32_t>(current[0]);
						current += 1;
					}

					remaining = static_cast<size_t>(end - current);
					if (subpacket_length == 0 || subpacket_length > remaining) {
						throw std::out_of_range("OP_AppCombined subpacket exceeds the remaining packet data");
					}

					ProcessDecodedPacket(StaticPacket(current, subpacket_length));
					current += subpacket_length;
				}

				break;
			}

			case OP_SessionRequest:
			{
				if (p.Length() < ReliableStreamConnect::size()) {
					return;
				}

				if (m_status == StatusConnected) {
					auto request = p.GetSerialize<ReliableStreamConnect>(0);
					auto protocol_version = NetworkToHost(request.protocol_version);
					auto requested_max_packet_size = NetworkToHost(request.max_packet_size);

					if (!IsSupportedProtocolVersion(protocol_version) || requested_max_packet_size < MIN_RAW_PACKET_SIZE || p.Length() > requested_max_packet_size) {
						return;
					}

					if (NetworkToHost(request.connect_code) != m_connect_code) {
						Close();
						return;
					}

					ReliableStreamConnectReply reply;
					reply.zero = 0;
					reply.opcode = OP_SessionResponse;
					reply.connect_code = HostToNetwork(m_connect_code);
					reply.encode_key = HostToNetwork(m_encode_key);
					reply.crc_bytes = m_crc_bytes;
					reply.max_packet_size = HostToNetwork(m_max_packet_size);
					reply.encode_pass1 = m_encode_passes[0];
					reply.encode_pass2 = m_encode_passes[1];
					DynamicPacket p;
					p.PutSerialize(0, reply);
					InternalSend(p);

					LogNetClient("[OP_SessionRequest] Session [{}] started with encode key [{}]", m_connect_code, HostToNetwork(m_encode_key));
				}

				break;
			}

			case OP_SessionResponse:
			{
				if (p.Length() < ReliableStreamConnectReply::size()) {
					return;
				}

				if (m_status == StatusConnecting) {
					auto reply = p.GetSerialize<ReliableStreamConnectReply>(0);
					auto connect_code = NetworkToHost(reply.connect_code);
					auto max_packet_size = NetworkToHost(reply.max_packet_size);

					bool valid_crc = IsValidCrcLength(reply.crc_bytes);
					bool valid_encode_passes = IsValidEncodeType(static_cast<ReliableStreamEncodeType>(reply.encode_pass1)) && IsValidEncodeType(static_cast<ReliableStreamEncodeType>(reply.encode_pass2));
					if (m_connect_code == connect_code && max_packet_size >= MIN_RAW_PACKET_SIZE && valid_crc && valid_encode_passes) {
						m_encode_key = NetworkToHost(reply.encode_key);
						m_crc_bytes = reply.crc_bytes;
						m_encode_passes[0] = (ReliableStreamEncodeType)reply.encode_pass1;
						m_encode_passes[1] = (ReliableStreamEncodeType)reply.encode_pass2;
						m_max_packet_size = static_cast<uint32_t>(std::min(m_owner->m_options.max_packet_size, static_cast<size_t>(max_packet_size)));
						InitializeReliableStreams();
						ChangeStatus(StatusConnected);
						for (int stream_id = 0; stream_id < 4; ++stream_id) {
							SendPendingPackets(stream_id);
						}

						LogNetClient(
							"[OP_SessionResponse] Session [{}] refresh with encode key [{}]",
							m_connect_code,
							HostToNetwork(m_encode_key)
						);
					}
				}
				break;
			}

			case OP_Packet:
			case OP_Packet2:
			case OP_Packet3:
			case OP_Packet4:
			{
				if (p.Length() <= ReliableStreamReliableHeader::size()) {
					return;
				}

				if (m_status == StatusDisconnecting) {
					if (!m_silent_disconnect) {
						SendDisconnect();
					}
					return;
				}

				auto header = p.GetSerialize<ReliableStreamReliableHeader>(0);
				auto sequence = NetworkToHost(header.sequence);
				auto stream_id = header.opcode - OP_Packet;
				auto stream = &m_streams[stream_id];

				auto order = CompareSequence(stream->sequence_in, sequence);
				if (order == SequenceFuture) {
					auto sequence_distance = static_cast<uint16_t>(sequence - stream->sequence_in);
					if (sequence_distance >= m_owner->m_options.max_instanding_packets[stream_id]) {
						return;
					}

					AddToQueue(stream_id, sequence, p);
					SendOutOfOrderAck(stream_id, sequence);
				}
				else if (order == SequencePast) {
					SendAck(stream_id, stream->sequence_in - 1);
				}
				else {
					if (stream->fragment_total_bytes != 0) {
						RejectInvalidFragment(stream, "received a normal reliable packet while reassembly was in progress");
						return;
					}

					RemoveFromQueue(stream_id, sequence);
					SendAck(stream_id, stream->sequence_in);
					stream->sequence_in++;
					StaticPacket next((char*)p.Data() + ReliableStreamReliableHeader::size(), p.Length() - ReliableStreamReliableHeader::size());
					ProcessDecodedPacket(next);
				}

				break;
			}

			case OP_Fragment:
			case OP_Fragment2:
			case OP_Fragment3:
			case OP_Fragment4:
			{
				if (p.Length() <= ReliableStreamReliableHeader::size()) {
					return;
				}

				if (m_status == StatusDisconnecting) {
					if (!m_silent_disconnect) {
						SendDisconnect();
					}
					return;
				}

				auto header = p.GetSerialize<ReliableStreamReliableHeader>(0);
				auto sequence = NetworkToHost(header.sequence);
				auto stream_id = header.opcode - OP_Fragment;
				auto stream = &m_streams[stream_id];

				auto order = CompareSequence(stream->sequence_in, sequence);

				if (order == SequenceFuture) {
					auto sequence_distance = static_cast<uint16_t>(sequence - stream->sequence_in);
					if (sequence_distance >= m_owner->m_options.max_instanding_packets[stream_id]) {
						return;
					}

					AddToQueue(stream_id, sequence, p);
					SendOutOfOrderAck(stream_id, sequence);
				}
				else if (order == SequencePast) {
					SendAck(stream_id, stream->sequence_in - 1);
				}
				else {
					bool first_fragment = stream->fragment_total_bytes == 0;
					size_t payload_offset = ReliableStreamReliableHeader::size();
					uint32_t total_bytes = stream->fragment_total_bytes;

					if (first_fragment) {
						if (p.Length() < ReliableStreamReliableFragmentHeader::size()) {
							RejectInvalidFragment(stream, "first fragment is shorter than its header");
							return;
						}

						auto fragheader = p.GetSerialize<ReliableStreamReliableFragmentHeader>(0);
						total_bytes = NetworkToHost(fragheader.total_size);
						payload_offset = ReliableStreamReliableFragmentHeader::size();

						if (total_bytes == 0 || total_bytes > m_owner->m_options.max_reassembled_packet_size) {
							RejectInvalidFragment(stream, "declared packet size is invalid");
							return;
						}
					}
					if (stream->fragment_current_bytes > total_bytes) {
						RejectInvalidFragment(stream, "reassembly position exceeds the declared packet size");
						return;
					}

					size_t payload_size = p.Length() - payload_offset;
					size_t remaining = total_bytes - stream->fragment_current_bytes;
					if (payload_size > remaining) {
						RejectInvalidFragment(stream, "fragment data exceeds the declared packet size");
						return;
					}

					if (first_fragment) {
						stream->fragment_total_bytes = total_bytes;
						stream->fragment_packet.Reserve(total_bytes);
					}

					stream->fragment_packet.PutData(stream->fragment_current_bytes, (char*)p.Data() + payload_offset, payload_size);
					stream->fragment_current_bytes += static_cast<uint32_t>(payload_size);

					RemoveFromQueue(stream_id, sequence);
					SendAck(stream_id, stream->sequence_in);
					stream->sequence_in++;

					if (stream->fragment_current_bytes == stream->fragment_total_bytes) {
						DynamicPacket completed(std::move(stream->fragment_packet));
						stream->ResetFragment();
						ProcessDecodedPacket(completed);
					}
				}

				break;
			}

			case OP_Ack:
			case OP_Ack2:
			case OP_Ack3:
			case OP_Ack4:
			{
				if (p.Length() < ReliableStreamReliableHeader::size()) {
					return;
				}

				auto header = p.GetSerialize<ReliableStreamReliableHeader>(0);
				auto sequence = NetworkToHost(header.sequence);
				auto stream_id = header.opcode - OP_Ack;
				Ack(stream_id, sequence);
				break;
			}

			case OP_OutOfOrderAck:
			case OP_OutOfOrderAck2:
			case OP_OutOfOrderAck3:
			case OP_OutOfOrderAck4:
			{
				if (p.Length() < ReliableStreamReliableHeader::size()) {
					return;
				}

				auto header = p.GetSerialize<ReliableStreamReliableHeader>(0);
				auto sequence = NetworkToHost(header.sequence);
				auto stream_id = header.opcode - OP_OutOfOrderAck;
				OutOfOrderAck(stream_id, sequence);
				break;
			}

			case OP_SessionDisconnect:
			{
				if (p.Length() < ReliableStreamDisconnect::size()) {
					return;
				}

				auto disconnect = p.GetSerialize<ReliableStreamDisconnect>(0);
				if (NetworkToHost(disconnect.connect_code) != m_connect_code) {
					return;
				}

				m_silent_disconnect = true;
				if (m_status != StatusDisconnecting) {
					m_close_time = Clock::now();
					ChangeStatus(StatusDisconnecting);
				}
				DiscardTransportQueues();

				LogNetClient(
					"[OP_SessionDisconnect] Session [{}] disconnect with encode key [{}]",
					m_connect_code,
					HostToNetwork(m_encode_key)
				);
				break;
			}

			case OP_Padding:
			{
				auto self = m_self.lock();
				if (m_owner->m_on_packet_recv && self) {
					m_owner->m_on_packet_recv(self, StaticPacket((char*)p.Data() + 1, p.Length() - 1));
				}
				break;
			}
			case OP_SessionStatRequest:
			{
				if (p.Length() < ReliableStreamSessionStatRequest::size()) {
					return;
				}

				auto request = p.GetSerialize<ReliableStreamSessionStatRequest>(0);
				m_stats.sync_remote_sent_packets = EQ::Net::NetworkToHost(request.packets_sent);
				m_stats.sync_remote_recv_packets = EQ::Net::NetworkToHost(request.packets_recv);
				m_stats.sync_sent_packets = m_stats.sent_packets;
				m_stats.sync_recv_packets = m_stats.recv_packets;

				ReliableStreamSessionStatResponse response;
				response.zero = 0;
				response.opcode = OP_SessionStatResponse;
				response.timestamp = request.timestamp;
				response.our_timestamp = EQ::Net::HostToNetwork(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
				response.client_sent = request.packets_sent;
				response.client_recv = request.packets_recv;
				response.server_sent = EQ::Net::HostToNetwork(m_stats.sent_packets);
				response.server_recv = EQ::Net::HostToNetwork(m_stats.recv_packets);
				DynamicPacket out;
				out.PutSerialize(0, response);
				InternalSend(out);
				break;
			}
			case OP_SessionStatResponse: {
				if (p.Length() < ReliableStreamSessionStatResponse::size()) {
					return;
				}

				auto response = p.GetSerialize<ReliableStreamSessionStatResponse>(0);
				m_stats.sync_remote_sent_packets = EQ::Net::NetworkToHost(response.server_sent);
				m_stats.sync_remote_recv_packets = EQ::Net::NetworkToHost(response.server_recv);
				m_stats.sync_sent_packets = m_stats.sent_packets;
				m_stats.sync_recv_packets = m_stats.recv_packets;
				break;
			}
			default:
				if (m_owner->m_on_error_message) {
					m_owner->m_on_error_message(fmt::format("Unhandled opcode {0:#x}", p.GetInt8(1)));
				}
				break;
		}
	}
	else {
		auto self = m_self.lock();
		if (m_owner->m_on_packet_recv && self) {
			m_owner->m_on_packet_recv(self, p);
		}
	}
}

bool EQ::Net::ReliableStreamConnection::ValidateCRC(Packet &p)
{
	if (m_crc_bytes == 0U) {
		return true;
	}

	if (p.Length() < (size_t)m_crc_bytes) {
		LogNetClient("Session [{}] ignored packet (crc bytes invalid on session)", m_connect_code);
		return false;
	}

	char *data = (char*)p.Data();
	uint32_t calculated = 0;
	uint32_t actual = 0;
	switch (m_crc_bytes) {
		case 1:
			actual = static_cast<uint8_t>(data[p.Length() - 1]);
			calculated = static_cast<uint32_t>(Crc32(data, static_cast<int>(p.Length() - 1), m_encode_key)) & 0xff;
			break;
		case 2: {
			uint16_t wire_crc = 0;
			memcpy(&wire_crc, &data[p.Length() - (size_t)m_crc_bytes], sizeof(wire_crc));
			actual = NetworkToHost(wire_crc);
			calculated = static_cast<uint32_t>(Crc32(data, (int)(p.Length() - (size_t)m_crc_bytes), m_encode_key)) & 0xffff;
			break;
		}
		case 3: {
			auto crc = reinterpret_cast<const uint8_t *>(&data[p.Length() - 3]);
			actual = (static_cast<uint32_t>(crc[0]) << 16) | (static_cast<uint32_t>(crc[1]) << 8) | static_cast<uint32_t>(crc[2]);
			calculated = static_cast<uint32_t>(Crc32(data, static_cast<int>(p.Length() - 3), m_encode_key)) & 0xffffff;
			break;
		}
		case 4: {
			uint32_t wire_crc = 0;
			memcpy(&wire_crc, &data[p.Length() - (size_t)m_crc_bytes], sizeof(wire_crc));
			actual = NetworkToHost(wire_crc);
			calculated = static_cast<uint32_t>(Crc32(data, (int)(p.Length() - (size_t)m_crc_bytes), m_encode_key));
			break;
		}
		default:
			return false;
	}

	if (actual == calculated) {
		return true;
	}

	return false;
}

void EQ::Net::ReliableStreamConnection::AppendCRC(Packet &p)
{
	if (m_crc_bytes == 0U) {
		return;
	}

	uint32_t calculated = static_cast<uint32_t>(Crc32(p.Data(), static_cast<int>(p.Length()), m_encode_key));
	switch (m_crc_bytes) {
		case 1:
			p.PutUInt8(p.Length(), static_cast<uint8_t>(calculated & 0xff));
			break;
		case 2:
			p.PutUInt16(p.Length(), EQ::Net::HostToNetwork(static_cast<uint16_t>(calculated & 0xffff)));
			break;
		case 3: {
			uint8_t crc[3] = {
				static_cast<uint8_t>((calculated >> 16) & 0xff),
				static_cast<uint8_t>((calculated >> 8) & 0xff),
				static_cast<uint8_t>(calculated & 0xff)
			};
			p.PutData(p.Length(), crc, sizeof(crc));
			break;
		}
		case 4:
			p.PutUInt32(p.Length(), EQ::Net::HostToNetwork(calculated));
			break;
	}
}

void EQ::Net::ReliableStreamConnection::ChangeStatus(DbProtocolStatus new_status)
{
	if (new_status == m_status) {
		return;
	}

	if (m_owner->m_on_connection_state_change) {
		if (auto self = m_self.lock()) {
			m_owner->m_on_connection_state_change(self, m_status, new_status);
		}
	}

	m_status = new_status;
}

bool EQ::Net::ReliableStreamConnection::PacketCanBeEncoded(Packet &p) const
{
	if (p.Length() < 2) {
		return false;
	}

	auto zero = p.GetInt8(0);
	if (zero != 0) {
		return true;
	}

	auto opcode = p.GetInt8(1);
	if (opcode == OP_SessionRequest || opcode == OP_SessionResponse || opcode == OP_UnreachableConnection || opcode == OP_RequestRemap) {
		return false;
	}

	return true;
}

void EQ::Net::ReliableStreamConnection::Decode(Packet &p, size_t offset, size_t length)
{
	uint32_t key = m_encode_key;
	char *buffer = (char*)p.Data() + offset;

	size_t i = 0;
	for (i = 0; i + 4 <= length; i += 4)
	{
		uint32_t cipher_text = 0;
		memcpy(&cipher_text, &buffer[i], sizeof(cipher_text));
		uint32_t plain_text = cipher_text ^ key;
		key = cipher_text;
		memcpy(&buffer[i], &plain_text, sizeof(plain_text));
	}

	unsigned char KC = key & 0xFF;
	for (; i < length; i++)
	{
		buffer[i] = buffer[i] ^ KC;
	}
}

void EQ::Net::ReliableStreamConnection::Encode(Packet &p, size_t offset, size_t length)
{
	uint32_t key = m_encode_key;
	char *buffer = (char*)p.Data() + offset;

	size_t i = 0;
	for (i = 0; i + 4 <= length; i += 4)
	{
		uint32_t plain_text = 0;
		memcpy(&plain_text, &buffer[i], sizeof(plain_text));
		uint32_t cipher_text = plain_text ^ key;
		key = cipher_text;
		memcpy(&buffer[i], &cipher_text, sizeof(cipher_text));
	}

	unsigned char KC = key & 0xFF;
	for (; i < length; i++)
	{
		buffer[i] = buffer[i] ^ KC;
	}
}

static uint32_t Inflate(const uint8_t* in, uint32_t in_len, uint8_t* out, uint32_t out_len) {
	if (!in || !out || out_len == 0) {
		return 0;
	}

	z_stream zstream;
	memset(&zstream, 0, sizeof(zstream));
	int zerror = 0;
	int i;

	zstream.next_in = const_cast<unsigned char *>(in);
	zstream.avail_in = in_len;
	zstream.next_out = out;
	zstream.avail_out = out_len;
	zstream.opaque = Z_NULL;

	i = inflateInit2(&zstream, 15);
	if (i != Z_OK) {
		return 0;
	}

	zerror = inflate(&zstream, Z_FINISH);
	uint32_t total_out = zerror == Z_STREAM_END ? static_cast<uint32_t>(zstream.total_out) : 0;
	inflateEnd(&zstream);
	return total_out;
}

static uint32_t Deflate(const uint8_t* in, uint32_t in_len, uint8_t* out, uint32_t out_len) {
	if (!in || !out || out_len == 0) {
		return 0;
	}

	z_stream zstream;
	memset(&zstream, 0, sizeof(zstream));
	int zerror;

	zstream.next_in = const_cast<unsigned char *>(in);
	zstream.avail_in = in_len;
	zstream.opaque = Z_NULL;

	if (deflateInit(&zstream, Z_BEST_SPEED) != Z_OK) {
		return 0;
	}
	zstream.next_out = out;
	zstream.avail_out = out_len;

	zerror = deflate(&zstream, Z_FINISH);

	uint32_t total_out = zerror == Z_STREAM_END ? static_cast<uint32_t>(zstream.total_out) : 0;
	deflateEnd(&zstream);
	return total_out;
}

bool EQ::Net::ReliableStreamConnection::Decompress(Packet &p, size_t offset, size_t length)
{
	if (offset > p.Length() || length != p.Length() - offset || length == 0 || offset >= m_max_packet_size) {
		return false;
	}

	size_t output_capacity = m_max_packet_size - offset;
	std::vector<uint8_t> new_buffer(output_capacity);
	uint8_t *buffer = (uint8_t*)p.Data() + offset;
	uint32_t new_length = 0;

	if (buffer[0] == 0x5a) {
		if (length < 2) {
			return false;
		}

		new_length = Inflate(buffer + 1, static_cast<uint32_t>(length - 1), new_buffer.data(), static_cast<uint32_t>(output_capacity));
		if (new_length == 0) {
			return false;
		}
	}
	else if (buffer[0] == 0xa5) {
		if (length - 1 > output_capacity) {
			return false;
		}

		memcpy(new_buffer.data(), buffer + 1, length - 1);
		new_length = static_cast<uint32_t>(length - 1);
	}
	else {
		return false;
	}

	p.Resize(offset);
	p.PutData(offset, new_buffer.data(), new_length);
	return true;
}

bool EQ::Net::ReliableStreamConnection::Compress(Packet &p, size_t offset, size_t length)
{
	if (offset > p.Length() || length != p.Length() - offset) {
		return false;
	}

	size_t encoded_capacity = m_max_packet_size - m_crc_bytes;
	if (offset > encoded_capacity || length + 1 > encoded_capacity - offset) {
		return false;
	}

	std::vector<uint8_t> new_buffer(length + 1);
	uint8_t *buffer = (uint8_t*)p.Data() + offset;
	uint32_t new_length = 0;
	bool send_uncompressed = true;

	if (length > 30) {
		uint32_t compressed_length = Deflate(buffer, static_cast<uint32_t>(length), new_buffer.data() + 1, static_cast<uint32_t>(length));
		if (compressed_length != 0 && compressed_length < length) {
			new_buffer[0] = 0x5a;
			new_length = compressed_length + 1;
			send_uncompressed = false;
		}
	}
	if (send_uncompressed) {
		memcpy(new_buffer.data() + 1, buffer, length);
		new_buffer[0] = 0xa5;
		new_length = static_cast<uint32_t>(length + 1);
	}

	p.Resize(offset);
	p.PutData(offset, new_buffer.data(), new_length);
	return true;
}

void EQ::Net::ReliableStreamConnection::QueueReliablePacket(int stream_id, const Packet &p, size_t data_length)
{
	if (stream_id < 0 || stream_id >= 4 || p.Length() < ReliableStreamReliableHeader::size() || m_status != StatusConnected) {
		return;
	}

	auto stream = &m_streams[stream_id];
	ReliableStreamPendingPacket pending;
	pending.packet.PutPacket(0, p);
	pending.data_length = data_length;
	stream->pending_bytes += data_length;
	stream->pending_packets.push(std::move(pending));

	if (m_owner->m_options.reliable_overflow_bytes != 0 && TotalPendingReliableBytes() >= m_owner->m_options.reliable_overflow_bytes) {
		LogNetClient("Closing session [{}]: reliable queue reached [{}] bytes", m_connect_code, TotalPendingReliableBytes());
		Close();
		return;
	}

	SendPendingPackets(stream_id);
}

int64_t EQ::Net::ReliableStreamConnection::GetReliableOutgoingId(const ReliableStream *stream, uint16_t reliable_stamp) const
{
	// Sony sends only the low 16 bits and reconstructs the full reliable ID using the next outgoing ID.
	int64_t reliable_id = static_cast<int64_t>(reliable_stamp) |
		(stream->sequence_out & ~static_cast<int64_t>(0xffff));

	if (reliable_id > stream->sequence_out) {
		reliable_id -= 0x10000;
	}

	return reliable_id;
}

void EQ::Net::ReliableStreamConnection::AdvanceOutgoingWindow(ReliableStream *stream)
{
	while (stream->sequence_out_pending < stream->sequence_out &&
		stream->sent_packets.find(stream->sequence_out_pending) == stream->sent_packets.end()) {
		stream->sequence_out_pending++;
	}
}

void EQ::Net::ReliableStreamConnection::SendPendingPackets(int stream_id)
{
	if (stream_id < 0 || stream_id >= 4 || m_status != StatusConnected) {
		return;
	}

	auto stream = &m_streams[stream_id];
	auto max_outstanding_packets = m_owner->m_options.max_outstanding_packets[stream_id];
	auto max_outstanding_bytes = std::min(m_owner->m_options.max_outstanding_bytes[stream_id], stream->congestion_window_size);
	auto now = Clock::now();

	while (!stream->pending_packets.empty() &&
		static_cast<size_t>(stream->sequence_out - stream->sequence_out_pending) < max_outstanding_packets &&
		stream->outstanding_bytes < max_outstanding_bytes) {
		ReliableStreamPendingPacket pending(std::move(stream->pending_packets.front()));
		stream->pending_packets.pop();
		stream->pending_bytes -= std::min(stream->pending_bytes, pending.data_length);

		int64_t reliable_id = stream->sequence_out++;
		auto wire_sequence = static_cast<uint16_t>(reliable_id & 0xffff);
		pending.packet.PutUInt16(ReliableStreamHeader::size(), HostToNetwork(wire_sequence));

		ReliableStreamSentPacket sent;
		sent.packet.PutPacket(0, pending.packet);
		sent.last_sent = now;
		sent.first_sent = now;
		sent.times_resent = 0;
		sent.data_length = pending.data_length;
		stream->outstanding_bytes += pending.data_length;

		auto inserted = stream->sent_packets.emplace(reliable_id, std::move(sent));
		InternalBufferedSend(inserted.first->second.packet);
	}

	stream->maxed_out_current_window = stream->outstanding_bytes >= max_outstanding_bytes;
}

void EQ::Net::ReliableStreamConnection::ProcessResend()
{
	for (int i = 0; i < 4; ++i) {
		ProcessResend(i);
	}
}

void EQ::Net::ReliableStreamConnection::ProcessResend(int stream)
{
	if (stream < 0 || stream >= 4 || m_status == DbProtocolStatus::StatusDisconnected || m_silent_disconnect) {
		return;
	}

	auto s = &m_streams[stream];
	if (s->sent_packets.empty()) {
		ResetCongestionWindowIfIdle(s);
		return;
	}

	m_resend_packets_sent = 0;
	m_resend_bytes_sent = 0;

	auto now = Clock::now();
	auto &oldest_packet = s->sent_packets.begin()->second;
	auto oldest_age = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldest_packet.first_sent).count();
	if (m_owner->m_options.resend_timeout != 0 && oldest_age >= static_cast<int64_t>(m_owner->m_options.resend_timeout)) {
		Close();
		return;
	}

	size_t resend_delay = EQ::Clamp(
		static_cast<size_t>((m_rolling_ping * m_owner->m_options.resend_delay_factor) + m_owner->m_options.resend_delay_ms),
		m_owner->m_options.resend_delay_min,
		m_owner->m_options.resend_delay_max);
	auto resend_timestamp = now - std::chrono::milliseconds(resend_delay);
	auto oldest_resend_timestamp = std::max(resend_timestamp, s->last_timestamp_acknowledged);
	size_t resend_outstanding_bytes = 0;
	for (const auto &entry : s->sent_packets) {
		if (entry.second.last_sent >= oldest_resend_timestamp) {
			resend_outstanding_bytes += entry.second.data_length;
		}
	}
	size_t resend_window_bytes = std::min(m_owner->m_options.max_outstanding_bytes[stream], s->congestion_window_size);

	for (auto &e: s->sent_packets) {
		auto &sp = e.second;
		if (sp.last_sent >= oldest_resend_timestamp) {
			continue;
		}

		if (m_resend_packets_sent >= MAX_RESEND_PACKETS_PER_PASS ||
			m_resend_bytes_sent >= MAX_RESEND_BYTES_PER_PASS) {
			LogNetClient(
				"Stopping resend because we hit thresholds m_resend_packets_sent [{}] max [{}] m_resend_bytes_sent [{}] max [{}]",
				m_resend_packets_sent,
				MAX_RESEND_PACKETS_PER_PASS,
				m_resend_bytes_sent,
				MAX_RESEND_BYTES_PER_PASS
			);
			break;
		}
		if (resend_outstanding_bytes >= resend_window_bytes) {
			break;
		}

		auto &p  = sp.packet;
		bool accelerated = sp.last_sent < s->last_timestamp_acknowledged;
		if (accelerated) {
			s->congestion_window_size = std::max(s->congestion_window_minimum, s->congestion_window_size * 2 / 3);
			s->congestion_slow_start_threshold = s->congestion_window_size;
		}
		else {
			s->congestion_slow_start_threshold = std::max(2 * s->max_reliable_data_size, resend_outstanding_bytes / 2);
			s->congestion_window_size = s->congestion_window_start;
			m_rolling_ping = std::min(m_owner->m_options.resend_delay_max, m_rolling_ping + 100);
		}
		resend_window_bytes = std::min(m_owner->m_options.max_outstanding_bytes[stream], s->congestion_window_size);

		if (p.Length() >= ReliableStreamHeader::size()) {
			if (p.GetInt8(0) == 0 && p.GetInt8(1) >= OP_Fragment && p.GetInt8(1) <= OP_Fragment4) {
				m_stats.resent_fragments++;
			}
			else {
				m_stats.resent_full++;
			}
		}
		else {
			m_stats.resent_full++;
		}
		m_stats.resent_packets++;

		// Resend the packet
		InternalBufferedSend(p);

		m_resend_packets_sent++;
		m_resend_bytes_sent += p.Length();
		sp.last_sent = now;
		sp.times_resent++;
		resend_outstanding_bytes += sp.data_length;
	}

	s->maxed_out_current_window = resend_outstanding_bytes >= std::min(m_owner->m_options.max_outstanding_bytes[stream], s->congestion_window_size);
}

void EQ::Net::ReliableStreamConnection::Ack(int stream, uint16_t seq)
{
	if (stream < 0 || stream >= 4) {
		return;
	}

	auto now = Clock::now();
	auto s = &m_streams[stream];
	auto reliable_id = GetReliableOutgoingId(s, seq);
	bool packets_acked = false;

	if (s->sequence_out_pending > reliable_id) {
		m_rolling_ping = std::min(m_owner->m_options.resend_delay_max, m_rolling_ping + 400);
		return;
	}
	if (reliable_id >= s->sequence_out) {
		return;
	}

	auto iter = s->sent_packets.begin();
	while (iter != s->sent_packets.end() && iter->first <= reliable_id) {
		auto &packet = iter->second;
		if (s->maxed_out_current_window) {
			if (s->congestion_window_size < s->congestion_slow_start_threshold) {
				s->congestion_window_size += s->max_reliable_data_size;
			}
			else {
				s->congestion_window_size += std::max<size_t>(1, s->max_reliable_data_size * s->max_reliable_data_size / s->congestion_window_size);
			}

			s->congestion_window_largest = std::max(s->congestion_window_largest, s->congestion_window_size);
		}

		if (packet.times_resent == 0) {
			uint64_t round_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - packet.first_sent).count());
			m_stats.max_ping = std::max(m_stats.max_ping, round_time);
			m_stats.min_ping = std::min(m_stats.min_ping, round_time);
			m_stats.last_ping = round_time;
			m_rolling_ping = (m_rolling_ping * 3 + round_time) / 4;
		}

		s->last_timestamp_acknowledged = packet.first_sent;
		s->outstanding_bytes -= std::min(s->outstanding_bytes, packet.data_length);

		iter = s->sent_packets.erase(iter);
		packets_acked = true;
	}

	if (packets_acked) {
		AdvanceOutgoingWindow(s);
		SendPendingPackets(stream);
		ResetCongestionWindowIfIdle(s);
	}
}

void EQ::Net::ReliableStreamConnection::OutOfOrderAck(int stream, uint16_t seq)
{
	if (stream < 0 || stream >= 4) {
		return;
	}

	auto now = Clock::now();
	auto s = &m_streams[stream];
	auto reliable_id = GetReliableOutgoingId(s, seq);
	auto iter = s->sent_packets.find(reliable_id);

	if (iter != s->sent_packets.end()) {
		auto &packet = iter->second;
		if (s->maxed_out_current_window) {
			if (s->congestion_window_size < s->congestion_slow_start_threshold) {
				s->congestion_window_size += s->max_reliable_data_size;
			}
			else {
				s->congestion_window_size += std::max<size_t>(1, s->max_reliable_data_size * s->max_reliable_data_size / s->congestion_window_size);
			}

			s->congestion_window_largest = std::max(s->congestion_window_largest, s->congestion_window_size);
		}

		if (packet.times_resent == 0) {
			uint64_t round_time = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - packet.first_sent).count());
			m_stats.max_ping = std::max(m_stats.max_ping, round_time);
			m_stats.min_ping = std::min(m_stats.min_ping, round_time);
			m_stats.last_ping = round_time;
			m_rolling_ping = (m_rolling_ping * 3 + round_time) / 4;
		}

		s->last_timestamp_acknowledged = packet.first_sent;
		s->outstanding_bytes -= std::min(s->outstanding_bytes, packet.data_length);

		s->sent_packets.erase(iter);
		AdvanceOutgoingWindow(s);
		SendPendingPackets(stream);
		ResetCongestionWindowIfIdle(s);
	}
}

void EQ::Net::ReliableStreamConnection::UpdateDataBudget(double budget_add)
{
	auto outgoing_data_rate = m_owner->m_options.outgoing_data_rate;
	m_outgoing_budget = EQ::ClampUpper(m_outgoing_budget + budget_add, outgoing_data_rate);
}

void EQ::Net::ReliableStreamConnection::SendAck(int stream_id, uint16_t seq)
{
	ReliableStreamReliableHeader ack;
	ack.zero = 0;
	ack.opcode = OP_Ack + stream_id;
	ack.sequence = HostToNetwork(seq);

	DynamicPacket p;
	p.PutSerialize(0, ack);

	InternalBufferedSend(p);
}

void EQ::Net::ReliableStreamConnection::SendOutOfOrderAck(int stream_id, uint16_t seq)
{
	ReliableStreamReliableHeader ack;
	ack.zero = 0;
	ack.opcode = OP_OutOfOrderAck + stream_id;
	ack.sequence = HostToNetwork(seq);

	DynamicPacket p;
	p.PutSerialize(0, ack);

	InternalBufferedSend(p);
}

void EQ::Net::ReliableStreamConnection::SendDisconnect()
{
	ReliableStreamDisconnect disconnect;
	disconnect.zero = 0;
	disconnect.opcode = OP_SessionDisconnect;
	disconnect.connect_code = HostToNetwork(m_connect_code);
	DynamicPacket out;
	out.PutSerialize(0, disconnect);
	InternalSend(out);
}

void EQ::Net::ReliableStreamConnection::InternalBufferedSend(Packet &p)
{
	if (p.Length() == 0) {
		return;
	}

	size_t encode_expansion = EncodeExpansionBytes(m_encode_passes);
	size_t encoded_overhead = PacketCanBeEncoded(p) ? m_crc_bytes + encode_expansion : 0;
	if (p.Length() + encoded_overhead > m_max_packet_size) {
		return;
	}

	if (p.Length() > 0xFFU) {
		FlushBuffer();
		InternalSend(p);
		return;
	}

	if (p.Length() >= ReliableStreamReliableHeader::size() && p.GetInt8(0) == 0 && p.GetInt8(1) >= OP_Ack && p.GetInt8(1) <= OP_Ack4) {
		int stream_id = p.GetInt8(1) - OP_Ack;
		if (m_owner->m_options.ack_deduping[stream_id]) {
			for (auto iter = m_buffered_packets.rbegin(); iter != m_buffered_packets.rend(); ++iter) {
				auto &buffered = *iter;
				if (buffered.Length() < ReliableStreamReliableHeader::size() || buffered.GetInt8(0) != 0) {
					continue;
				}

				if (buffered.GetInt8(1) == OP_OutOfOrderAck + stream_id) {
					break;
				}

				if (buffered.GetInt8(1) == OP_Ack + stream_id) {
					m_buffered_packets_length -= buffered.Length();
					buffered.Clear();
					buffered.PutPacket(0, p);
					m_buffered_packets_length += buffered.Length();
					return;
				}
			}
		}
	}

	//we could add this packet to a combined
	size_t raw_size = ReliableStreamHeader::size() + (size_t)m_crc_bytes + encode_expansion + m_buffered_packets_length + m_buffered_packets.size() + 1 + p.Length();
	if (raw_size > m_max_packet_size) {
		FlushBuffer();
	}

	if (m_buffered_packets.empty()) {
		m_hold_time = Clock::now();
	}

	DynamicPacket copy;
	copy.PutPacket(0, p);
	m_buffered_packets.push_back(std::move(copy));
	m_buffered_packets_length += p.Length();

	if (m_owner->m_options.hold_length_ms == 0 || m_buffered_packets_length + m_buffered_packets.size() > m_owner->m_options.hold_size) {
		FlushBuffer();
	}
}

void EQ::Net::ReliableStreamConnection::SendConnect()
{
	ReliableStreamConnect connect;
	connect.zero = 0;
	connect.opcode = OP_SessionRequest;
	connect.protocol_version = HostToNetwork(m_owner->m_options.protocol_version);
	connect.connect_code = (uint32_t)HostToNetwork(m_connect_code);
	connect.max_packet_size = HostToNetwork((uint32_t)m_owner->m_options.max_packet_size);

	DynamicPacket p;
	p.PutSerialize(0, connect);

	InternalSend(p);
}

void EQ::Net::ReliableStreamConnection::SendKeepAlive()
{
	ReliableStreamHeader keep_alive;
	keep_alive.zero = 0;
	keep_alive.opcode = OP_KeepAlive;

	DynamicPacket p;
	p.PutSerialize(0, keep_alive);

	InternalSend(p);
}

void EQ::Net::ReliableStreamConnection::InternalSend(Packet &p) {
	if (m_owner->m_options.outgoing_data_rate > 0.0) {
		auto new_budget = m_outgoing_budget - (p.Length() / 1024.0);
		if (new_budget <= 0.0) {
			m_stats.dropped_datarate_packets++;
			return;
		} else {
			m_outgoing_budget = new_budget;
		}
	}

	m_last_send = Clock::now();

	auto pooled_opt = send_buffer_pool.acquire();
	if (!pooled_opt) {
		m_stats.dropped_datarate_packets++;
		return;
	}

	auto [send_req, data, ctx] = *pooled_opt;
	ctx->pool = &send_buffer_pool; // set pool pointer

	sockaddr_in send_addr{};
	uv_ip4_addr(m_endpoint.c_str(), m_port, &send_addr);
	uv_buf_t send_buffers[1];

	if (PacketCanBeEncoded(p)) {
		m_stats.bytes_before_encode += p.Length();

		DynamicPacket out;
		out.PutPacket(0, p);

		for (auto &m_encode_passe: m_encode_passes) {
			switch (m_encode_passe) {
				case EncodeCompression:
					if (out.GetInt8(0) == 0) {
						if (!Compress(out, ReliableStreamHeader::size(), out.Length() - ReliableStreamHeader::size())) {
							send_buffer_pool.release(ctx);
							return;
						}
					} else {
						if (!Compress(out, 1, out.Length() - 1)) {
							send_buffer_pool.release(ctx);
							return;
						}
					}
					break;
				case EncodeXOR:
					if (out.GetInt8(0) == 0) {
						Encode(out, ReliableStreamHeader::size(), out.Length() - ReliableStreamHeader::size());
					} else {
						Encode(out, 1, out.Length() - 1);
					}
					break;
				default:
					break;
			}
		}

		AppendCRC(out);
		if (out.Length() > m_max_packet_size || out.Length() > UDP_BUFFER_SIZE) {
			send_buffer_pool.release(ctx);
			return;
		}

		memcpy(data, out.Data(), out.Length());
		send_buffers[0] = uv_buf_init(data, out.Length());
	} else {
		if (p.Length() > m_max_packet_size || p.Length() > UDP_BUFFER_SIZE) {
			send_buffer_pool.release(ctx);
			return;
		}

		memcpy(data, p.Data(), p.Length());
		send_buffers[0] = uv_buf_init(data, p.Length());
	}

	m_stats.sent_bytes += send_buffers[0].len;
	m_stats.sent_packets++;

	if (m_owner->m_options.simulated_out_packet_loss &&
		m_owner->m_options.simulated_out_packet_loss >= m_owner->m_rand.Int(0, 100)) {
		send_buffer_pool.release(ctx);
		return;
	}

	int send_result = uv_udp_send(
		send_req, &m_owner->m_socket, send_buffers, 1, (sockaddr *)&send_addr,
		[](uv_udp_send_t *req, int status) {
			auto *ctx = reinterpret_cast<EmbeddedContext *>(req->data);
			if (!ctx) {
				std::cerr << "Error: send_req->data is null in callback!" << std::endl;
				return;
			}

			if (status < 0) {
				std::cerr << "uv_udp_send failed: " << uv_strerror(status) << std::endl;
			}

			ctx->pool->release(ctx);
		}
	);

	if (send_result < 0) {
		std::cerr << "uv_udp_send() failed: " << uv_strerror(send_result) << std::endl;
		send_buffer_pool.release(ctx);
	}
}

void EQ::Net::ReliableStreamConnection::InternalQueuePacket(Packet &p, int stream_id, bool reliable)
{
	if (stream_id < 0 || stream_id >= 4 || p.Length() == 0 || m_status != StatusConnected) {
		return;
	}

	if (!reliable) {
		if (m_status != StatusConnected) {
			return;
		}

		size_t encode_expansion = EncodeExpansionBytes(m_encode_passes);
		auto max_raw_size = m_max_packet_size - m_crc_bytes - encode_expansion;
		if (p.Length() > max_raw_size) {
			InternalQueuePacket(p, stream_id, true);
			return;
		}

		InternalBufferedSend(p);
		return;
	}

	auto stream = &m_streams[stream_id];
	auto max_raw_size = stream->max_reliable_data_size;
	size_t length = p.Length();
	if (length > max_raw_size) {
		if (length > std::numeric_limits<uint32_t>::max() || length > m_owner->m_options.max_reassembled_packet_size) {
			return;
		}

		ReliableStreamReliableFragmentHeader first_header;
		first_header.reliable.zero = 0;
		first_header.reliable.opcode = OP_Fragment + stream_id;
		first_header.reliable.sequence = 0;
		first_header.total_size = (uint32_t)HostToNetwork((uint32_t)length);

		size_t used = 0;
		size_t sublen = max_raw_size - sizeof(uint32_t);
		DynamicPacket first_packet;
		first_packet.PutSerialize(0, first_header);
		first_packet.PutData(ReliableStreamReliableFragmentHeader::size(), (char*)p.Data() + used, sublen);
		used += sublen;

		QueueReliablePacket(stream_id, first_packet, sublen);

		while (used < length) {
			auto left = length - used;
			DynamicPacket packet;
			ReliableStreamReliableHeader header;
			header.zero = 0;
			header.opcode = OP_Fragment + stream_id;
			header.sequence = 0;
			packet.PutSerialize(0, header);

			if (left > max_raw_size) {
				packet.PutData(ReliableStreamReliableHeader::size(), (char*)p.Data() + used, max_raw_size);
				used += max_raw_size;
			}
			else {
				packet.PutData(ReliableStreamReliableHeader::size(), (char*)p.Data() + used, left);
				used += left;
			}

			QueueReliablePacket(stream_id, packet, std::min(left, max_raw_size));
		}
	}
	else {
		DynamicPacket packet;
		ReliableStreamReliableHeader header;
		header.zero = 0;
		header.opcode = OP_Packet + stream_id;
		header.sequence = 0;
		packet.PutSerialize(0, header);
		packet.PutPacket(ReliableStreamReliableHeader::size(), p);

		QueueReliablePacket(stream_id, packet, length);
	}
}

void EQ::Net::ReliableStreamConnection::FlushBuffer()
{
	if (m_buffered_packets.empty()) {
		return;
	}

	if (m_buffered_packets.size() > 1) {
		StaticPacket out(m_combined.get(), m_max_packet_size);
		size_t length = 2;
		for (auto &p : m_buffered_packets) {
			out.PutUInt8(length, (uint8_t)p.Length());
			out.PutPacket(length + 1, p);
			length += (1 + p.Length());
		}

		out.Resize(length);
		InternalSend(out);
	}
	else {
		auto &front = m_buffered_packets.front();
		InternalSend(front);
	}

	m_buffered_packets.clear();
	m_buffered_packets_length = 0;
}

EQ::Net::SequenceOrder EQ::Net::ReliableStreamConnection::CompareSequence(uint16_t expected, uint16_t actual) const
{
	int diff = (int)actual - (int)expected;

	if (diff == 0) {
		return SequenceCurrent;
	}

	if (diff > 0) {
		if (diff > MAX_RELIABLE_SEQUENCE_DISTANCE) {
			return SequencePast;
		}

		return SequenceFuture;
	}

	if (diff < -MAX_RELIABLE_SEQUENCE_DISTANCE) {
		return SequenceFuture;
	}

	return SequencePast;
}
