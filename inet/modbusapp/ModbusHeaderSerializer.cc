//
// Copyright (C) [Year] Your Name/Organization
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#include "ModbusHeaderSerializer.h"

#include "ModbusHeader_m.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"

namespace inet {

Register_Serializer(ModbusHeader, ModbusHeaderSerializer);

void ModbusHeaderSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& msg = staticPtrCast<const ModbusHeader>(chunk);

    stream.writeUint16Be(msg->getTransactionId());
    stream.writeUint16Be(msg->getProtocolId());
    stream.writeUint16Be(msg->getLength());

    // 序列化Modbus协议核心字段
    stream.writeByte(msg->getSlaveId());           // 设备地址（1字节）

}

const Ptr<Chunk> ModbusHeaderSerializer::deserialize(MemoryInputStream& stream) const
{
    auto msg = makeShared<ModbusHeader>();

    msg->setTransactionId(stream.readUint16Be());
    msg->setProtocolId(stream.readUint16Be());
    msg->setLength(stream.readUint16Be());


    // 反序列化Modbus协议核心字段（与序列化顺序一致）
    msg->setSlaveId(stream.readByte());             // 设备地址


    return msg;
}

} // namespace inet
