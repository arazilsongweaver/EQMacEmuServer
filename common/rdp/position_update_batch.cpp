#include "position_update_batch.h"

#include <cstring>

PositionUpdateBatch::PositionUpdateBatch()
	: m_updates{},
	  m_count(0)
{
}

bool PositionUpdateBatch::Empty() const
{
	return m_count == 0;
}

bool PositionUpdateBatch::Full() const
{
	return m_count == MaximumUpdates;
}

bool PositionUpdateBatch::Append(const SpawnPositionUpdate_Struct &update)
{
	if (Full())
		return false;

	m_updates[m_count++] = update;
	return true;
}

uint32 PositionUpdateBatch::Write(uint8 *destination, uint16 opcode) const
{
	if (destination == nullptr || Empty())
		return 0;

	// EQ application opcodes are little-endian.
	destination[0] = static_cast<uint8>(opcode);
	destination[1] = static_cast<uint8>(opcode >> 8);
	std::memcpy(destination + 2, &m_count, sizeof(m_count));
	std::memcpy(destination + 2 + sizeof(m_count), m_updates, m_count * sizeof(m_updates[0]));

	return 2 + sizeof(m_count) + m_count * sizeof(m_updates[0]);
}

void PositionUpdateBatch::Clear()
{
	m_count = 0;
}
