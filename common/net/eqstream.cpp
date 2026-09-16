#include "eqstream.h"
#include "../eqemu_logsys.h"

EQ::Net::EQStreamManager::EQStreamManager(const EQStreamManagerOptions &options)
	: m_opcode_size(options.opcode_size),
	  m_reliable_stream(options.reliable_stream_options)
{
	m_reliable_stream.OnNewConnection(std::bind(&EQStreamManager::ReliableStreamNewConnection, this, std::placeholders::_1));
	m_reliable_stream.OnConnectionStateChange(std::bind(&EQStreamManager::ReliableStreamConnectionStateChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_reliable_stream.OnPacketRecv(std::bind(&EQStreamManager::ReliableStreamPacketRecv, this, std::placeholders::_1, std::placeholders::_2));
}

void EQ::Net::EQStreamManager::ReliableStreamNewConnection(std::shared_ptr<ReliableStreamConnection> connection)
{
	auto stream = std::make_shared<EQStream>(m_opcode_size, connection);
	m_streams.emplace(std::make_pair(connection, stream));
	if (m_on_new_connection) {
		m_on_new_connection(stream);
	}
}

void EQ::Net::EQStreamManager::ReliableStreamConnectionStateChange(std::shared_ptr<ReliableStreamConnection> connection, DbProtocolStatus, DbProtocolStatus to)
{
	auto iter = m_streams.find(connection);
	if (iter != m_streams.end()) {
		if (to == StatusDisconnected) {
			m_streams.erase(iter);
		}
	}
}

void EQ::Net::EQStreamManager::ReliableStreamPacketRecv(std::shared_ptr<ReliableStreamConnection> connection, const Packet &p)
{
	auto iter = m_streams.find(connection);
	if (iter != m_streams.end()) {
		if (m_opcode_size <= 0 || p.Length() < static_cast<size_t>(m_opcode_size)) {
			return;
		}

		auto &stream = iter->second;
		auto packet = std::make_unique<DynamicPacket>();
		packet->PutPacket(0, p);
		stream->m_packet_queue.push_back(std::move(packet));
	}
}

EQ::Net::EQStream::EQStream(int opcode_size, std::shared_ptr<ReliableStreamConnection> connection)
	: m_opcode_size(opcode_size),
	  m_connection(std::move(connection))
{
}

void EQ::Net::EQStream::QueuePacket(const EQApplicationPacket *p, bool ack_req)
{
	if (!m_opcode_manager) {
		LogNetcode("Cannot queue application packet without an opcode manager");
		return;
	}

	LogPacketServerClient(
		"[{}] [{:#06x}] Size [{}] {}",
		OpcodeManager::EmuToName(p->GetOpcode()),
		m_opcode_manager->EmuToEQ(p->GetOpcode()),
		p->Size(),
		(LogSys.IsLogEnabled(Logs::Detail, Logs::PacketServerClient) ? DumpPacketToString(p) : "")
	);

	uint16 opcode = p->GetOpcodeBypass();
	if (opcode == 0) {
		opcode = m_opcode_manager->EmuToEQ(p->GetOpcode());
	}

	DynamicPacket out;
	switch (m_opcode_size) {
	case 1:
		out.PutUInt8(0, opcode);
		out.PutData(1, p->pBuffer, p->size);
		break;
	case 2:
		out.PutUInt16(0, opcode);
		out.PutData(2, p->pBuffer, p->size);
		break;
	default:
		LogNetcode("Cannot queue application packet with opcode size [{}]", m_opcode_size);
		return;
	}

	if (ack_req) {
		m_connection->QueuePacket(out);
	}
	else {
		m_connection->QueuePacket(out, 0, false);
	}
}

EQApplicationPacket *EQ::Net::EQStream::PopPacket()
{
	if (m_packet_queue.empty() || !m_opcode_manager) {
		return nullptr;
	}

	auto &packet = m_packet_queue.front();
	uint16 opcode = 0;
	switch (m_opcode_size) {
	case 1:
		opcode = packet->GetUInt8(0);
		break;
	case 2:
		opcode = packet->GetUInt16(0);
		break;
	default:
		return nullptr;
	}

	EmuOpcode emu_opcode = m_opcode_manager->EQToEmu(opcode);
	auto app = new EQApplicationPacket(
		emu_opcode,
		(unsigned char *)packet->Data() + m_opcode_size,
		packet->Length() - m_opcode_size
	);
	app->SetProtocolOpcode(opcode);
	m_packet_queue.pop_front();
	return app;
}

void EQ::Net::EQStream::Close()
{
	m_connection->Close();
}

uint32 EQ::Net::EQStream::GetRemoteIP() const
{
	return inet_addr(m_connection->RemoteEndpoint().c_str());
}
