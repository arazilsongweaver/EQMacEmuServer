#ifndef RDP_RUNTIME_H
#define RDP_RUNTIME_H

#include "../types.h"

#include "rdplib.h"

class RDPEndpoint;

// owns the process rdplib runtime
class RDPRuntime
{
public:
	RDPRuntime();
	~RDPRuntime();

	RDPRuntime(const RDPRuntime &) = delete;
	RDPRuntime &operator=(const RDPRuntime &) = delete;

	// fast_allocator_bytes is an initial allocation, not a limit.  client uses 1MB
	int Open(uint32 fast_allocator_bytes = 4 * 1024 * 1024);

	// every endpoint must be closed first
	int Close();

	bool IsOpen() const
	{
		return m_runtime != nullptr;
	}

private:
	friend class RDPEndpoint;
	rdplib_runtime_t *m_runtime;
};

#endif
