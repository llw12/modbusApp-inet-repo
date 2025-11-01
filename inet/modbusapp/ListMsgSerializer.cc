/*
 * ListMsgSerializer.cc
 *
 *  Created on: Sep 20, 2025
 *      Author: llw
 */

#include "ListMsgSerializer.h"

#include "ListMsg_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"

namespace inet {

Register_Serializer(ListMsg, ListMsgSerializer);

void ListMsgSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{

    const auto& msg = staticPtrCast<const ListMsg>(chunk);
    stream.writeUint32Be(msg->getSequenceNumber());
    stream.writeByte(msg->getIfList());
}

const Ptr<Chunk> ListMsgSerializer::deserialize(MemoryInputStream& stream) const
{
    auto msg = makeShared<ListMsg>();
    msg->setSequenceNumber(stream.readUint32Be());
    msg->setIfList(stream.readByte() ? true : false);
    return msg;
}

} // namespace inet




