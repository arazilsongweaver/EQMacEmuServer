#include "rdp_endpoint.h"

#include "../eqemu_logsys.h"
#include "rdp_connection.h"

#include <cassert>
#include <new>

static constexpr uint32 ServerSocketBufferSize = 4u * 1024u * 1024u;

static void ReleaseConnection(rdplib_connection_t *connection)
{
	if (connection == nullptr)
		return;

	rdplib_message_t *message;
	while ((message = rdplib_connection_pop_message(connection)) != nullptr)
		rdplib_message_release(message);

	(void)rdplib_connection_begin_close(connection, 0);
	rdplib_connection_release(connection);
}

RDPEndpoint::RDPEndpoint()
	: m_endpoint(nullptr)
{
}

RDPEndpoint::~RDPEndpoint()
{
	int result = Close();
	assert(result == RDPLIB_OK);
	(void)result;
}

int RDPEndpoint::Open(RDPRuntime &runtime, uint16 local_port, uint32 expected_connections, uint32 flags)
{
	return Open(runtime, local_port, nullptr, nullptr, expected_connections, flags);
}

int RDPEndpoint::Open(RDPRuntime &runtime, uint16 local_port, rdplib_arrival_ready_callback_t arrival_ready_callback,
	void *arrival_ready_context, uint32 expected_connections, uint32 flags)
{
	if (m_endpoint != nullptr)
		return RDPLIB_ERROR_BUSY;
	if (!runtime.IsOpen())
		return RDPLIB_ERROR_INVALID_ARGUMENT;

	rdplib_endpoint_options_t options{};
	options.structure_size = sizeof(options);
	options.arrival_ready_callback = arrival_ready_callback;
	options.arrival_ready_context = arrival_ready_context;

	// use default buffer sizes for client usage
	if (local_port != 0)
	{
		// request larger buffer sizes for server usage
		options.receive_socket_buffer_bytes = ServerSocketBufferSize;
		options.send_socket_buffer_bytes = ServerSocketBufferSize;
	}

	int result = rdplib_endpoint_create_ex(runtime.m_runtime, &m_endpoint, local_port, expected_connections, flags, &options);
	if (result != RDPLIB_OK || local_port == 0)
		return result;

	// receive buffer, SO_RCVBUF
	uint32 receive_socket_buffer_bytes;
	result = rdplib_endpoint_get_socket_receive_buffer_size(m_endpoint, &receive_socket_buffer_bytes);
	if (result != RDPLIB_OK)
	{
		(void)Close();
		return result;
	}
	if (receive_socket_buffer_bytes < ServerSocketBufferSize)
		LogWarning("RDP receive socket buffer requested [{}] bytes, but the operating system reported [{}] bytes. If running on Linux, check net.core.rmem_max and net.core.wmem_max", ServerSocketBufferSize, receive_socket_buffer_bytes);

	// send buffer, SO_SNDBUF
	uint32 send_socket_buffer_bytes;
	result = rdplib_endpoint_get_socket_send_buffer_size(m_endpoint, &send_socket_buffer_bytes);
	if (result != RDPLIB_OK)
	{
		(void)Close();
		return result;
	}
	if (send_socket_buffer_bytes < ServerSocketBufferSize)
		LogWarning("RDP send socket buffer requested [{}] bytes, but the operating system reported [{}] bytes. If running on Linux, check net.core.rmem_max and net.core.wmem_max", ServerSocketBufferSize, send_socket_buffer_bytes);

	return RDPLIB_OK;
}

int RDPEndpoint::Close()
{
	if (m_endpoint == nullptr)
		return RDPLIB_OK;

	int result = rdplib_endpoint_destroy(m_endpoint);
	if (result != RDPLIB_OK)
		return result;

	m_endpoint = nullptr;
	return RDPLIB_OK;
}

int RDPEndpoint::Process(int32 timeout_ms)
{
	if (m_endpoint == nullptr)
		return RDPLIB_ERROR_INVALID_ARGUMENT;

	int result = rdplib_endpoint_process(m_endpoint, timeout_ms);
	DiscardConnectionless();
	return result;
}

RDPConnection *RDPEndpoint::Accept(int *result)
{
	if (result != nullptr)
		*result = RDPLIB_OK;
	if (m_endpoint == nullptr)
	{
		if (result != nullptr)
			*result = RDPLIB_ERROR_NOT_USABLE;
		return nullptr;
	}

	rdplib_connection_t *connection = rdplib_endpoint_accept(m_endpoint);
	if (connection == nullptr)
		return nullptr;

	RDPConnection *wrapper = new (std::nothrow) RDPConnection(connection);
	if (wrapper == nullptr)
	{
		ReleaseConnection(connection);
		if (result != nullptr)
			*result = RDPLIB_ERROR_OUT_OF_MEMORY;
	}
	return wrapper;
}

RDPConnection *RDPEndpoint::Connect(const char *host, uint16 port, int *result)
{
	if (result != nullptr)
		*result = RDPLIB_OK;
	if (m_endpoint == nullptr || host == nullptr)
	{
		if (result != nullptr)
			*result = RDPLIB_ERROR_INVALID_ARGUMENT;
		return nullptr;
	}

	rdplib_connection_t *connection = nullptr;
	int connect_result = rdplib_connect(m_endpoint, &connection, host, port);
	if (connect_result != RDPLIB_OK)
	{
		if (result != nullptr)
			*result = connect_result;
		return nullptr;
	}

	RDPConnection *wrapper = new (std::nothrow) RDPConnection(connection);
	if (wrapper == nullptr)
	{
		ReleaseConnection(connection);
		if (result != nullptr)
			*result = RDPLIB_ERROR_OUT_OF_MEMORY;
	}
	return wrapper;
}

uint16 RDPEndpoint::LocalPort() const
{
	return m_endpoint != nullptr ? rdplib_endpoint_local_port(m_endpoint) : 0;
}

// This is something the rdp library supports but the game doesn't use this.
// The client also drains and discards any arriving connectionless messages.
void RDPEndpoint::DiscardConnectionless()
{
	if (m_endpoint == nullptr)
		return;

	rdplib_message_t *message;
	while ((message = rdplib_endpoint_pop_connectionless(m_endpoint)) != nullptr)
		rdplib_message_release(message);
}
