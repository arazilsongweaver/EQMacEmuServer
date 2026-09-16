#pragma once

#include "../eq_packet.h"
#include "../opcodemgr.h"
#include "reliable_stream_connection.h"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <utility>

namespace EQ
{
	namespace Net
	{
		struct EQStreamManagerOptions
		{
			EQStreamManagerOptions() = default;

			EQStreamManagerOptions(int port, bool encoded, bool compressed)
			{
				if (compressed) {
					reliable_stream_options.encode_passes[0] = EncodeCompression;
				}
				else if (encoded) {
					reliable_stream_options.encode_passes[0] = EncodeXOR;
				}

				reliable_stream_options.port = port;
			}

			int opcode_size = 2;
			ReliableStreamConnectionManagerOptions reliable_stream_options;
		};

		class EQStream;

		class EQStreamManager
		{
		public:
			explicit EQStreamManager(const EQStreamManagerOptions &options);

			void OnNewConnection(std::function<void(std::shared_ptr<EQStream>)> func) { m_on_new_connection = func; }

		private:
			void ReliableStreamNewConnection(std::shared_ptr<ReliableStreamConnection> connection);
			void ReliableStreamConnectionStateChange(std::shared_ptr<ReliableStreamConnection> connection, DbProtocolStatus, DbProtocolStatus to);
			void ReliableStreamPacketRecv(std::shared_ptr<ReliableStreamConnection> connection, const Packet &p);

			int m_opcode_size;
			ReliableStreamConnectionManager m_reliable_stream;
			std::function<void(std::shared_ptr<EQStream>)> m_on_new_connection;
			std::map<std::shared_ptr<ReliableStreamConnection>, std::shared_ptr<EQStream>> m_streams;
		};

		class EQStream
		{
		public:
			EQStream(int opcode_size, std::shared_ptr<ReliableStreamConnection> connection);

			void QueuePacket(const EQApplicationPacket *p, bool ack_req = true);
			EQApplicationPacket *PopPacket();
			void Close();

			uint32 GetRemoteIP() const;
			uint16 GetRemotePort() const { return m_connection->RemotePort(); }
			bool IsClosed() const { return m_connection->GetStatus() == StatusDisconnected; }

			void SetOpcodeManager(const std::shared_ptr<OpcodeManager> &opm) { m_opcode_manager = opm; }
			OpcodeManager *GetOpcodeManager() const { return m_opcode_manager.get(); }

		private:
			int m_opcode_size;
			std::shared_ptr<ReliableStreamConnection> m_connection;
			std::shared_ptr<OpcodeManager> m_opcode_manager;
			std::deque<std::unique_ptr<Packet>> m_packet_queue;

			friend class EQStreamManager;
		};
	}
}
