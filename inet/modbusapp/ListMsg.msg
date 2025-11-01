import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.chunk.Chunk;
import inet.applications.base.ApplicationPacket;

namespace inet;

class ListMsg extends FieldsChunk
{
    chunkLength = B(5);
    uint32_t sequenceNumber;
	bool ifList = true;
}

