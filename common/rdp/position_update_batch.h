#ifndef POSITION_UPDATE_BATCH_H
#define POSITION_UPDATE_BATCH_H

#include "../eq_packet_structs.h"
#include "../types.h"

class PositionUpdateBatch
{
public:
	static constexpr uint32 MaximumUpdates = 33;
	static constexpr uint32 MaximumMessageBytes = 2 + sizeof(uint32) + MaximumUpdates * sizeof(SpawnPositionUpdate_Struct);

	PositionUpdateBatch();

	bool Empty() const;
	bool Full() const;

	bool Append(const SpawnPositionUpdate_Struct &update);
	uint32 Write(uint8 *destination, uint16 opcode) const;
	void Clear();

private:
	SpawnPositionUpdate_Struct m_updates[MaximumUpdates];
	uint32 m_count;
};

static_assert(sizeof(SpawnPositionUpdate_Struct) == 15, "unexpected position update size");
static_assert(PositionUpdateBatch::MaximumMessageBytes == 501, "unexpected position update batch size");

#endif
