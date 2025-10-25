import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.chunk.Chunk;

namespace inet;


class OperatorRequest extends FieldsChunk
{
    string targetHostName;
    uint16_t transactionId;                                       
    uint16_t protocolId;   
    uint16_t length;                                               
    uint8_t slaveId;                                                
    uint8_t functionCode;      
    uint16_t startAddress;                            
    uint16_t quantity; 
    
}