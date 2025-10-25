import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.chunk.Chunk;

namespace inet;

class ModbusResponseHeader extends FieldsChunk
{
    chunkLength = B(12);
    uint16_t transactionId;                                       
    uint16_t protocolId;   
    uint16_t length;                                               
    uint8_t slaveId;                                                
    uint8_t functionCode;      
}