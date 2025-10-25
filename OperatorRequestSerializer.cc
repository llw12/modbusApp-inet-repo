//
// Copyright (C) [Year] Your Name/Organization
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#include "inet/applications/tcpapp/OperatorRequestSerializer.h"
#include "inet/common/packet/serializer/ChunkSerializerRegistry.h"
#include "inet/applications/tcpapp/OperatorRequest_m.h"

namespace inet {

Register_Serializer(OperatorRequest, OperatorRequestSerializer);

void OperatorRequestSerializer::serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const
{
    const auto& msg = staticPtrCast<const OperatorRequest>(chunk);

    // 序列化字符串字段（先写长度，再写内容）
    const char* targetHost = msg->getTargetHostName();
    uint16_t hostNameLen = strlen(targetHost);
    stream.writeUint16Be(hostNameLen); // 字符串长度（2字节）
    for(int i = 0; i < hostNameLen; i++){
        stream.writeByte(targetHost[i]);
    }

    // 序列化其他字段（与ModbusHeader类似）
    stream.writeUint16Be(msg->getTransactionId());
    stream.writeUint16Be(msg->getProtocolId());
    stream.writeUint16Be(msg->getLength());
    stream.writeByte(msg->getSlaveId());
    stream.writeByte(msg->getFunctionCode());
    stream.writeUint16Be(msg->getStartAddress());
    stream.writeUint16Be(msg->getQuantity());
}

const Ptr<Chunk> OperatorRequestSerializer::deserialize(MemoryInputStream& stream) const
{
    auto msg = makeShared<OperatorRequest>();

    // 反序列化字符串字段
    uint16_t hostNameLen = stream.readUint16Be(); // 读取字符串长度
    char* targetHost = new char[hostNameLen + 1]; // +1 用于终止符
    for(int i = 0; i < hostNameLen; i++){
        targetHost[i] = stream.readByte();
    }

    targetHost[hostNameLen] = '\0'; // 添加字符串终止符
    msg->setTargetHostName(targetHost);
    delete[] targetHost; // 释放临时内存

    // 反序列化其他字段（与ModbusHeader类似）
    msg->setTransactionId(stream.readUint16Be());
    msg->setProtocolId(stream.readUint16Be());
    msg->setLength(stream.readUint16Be());
    msg->setSlaveId(stream.readByte());
    msg->setFunctionCode(stream.readByte());
    msg->setStartAddress(stream.readUint16Be());
    msg->setQuantity(stream.readUint16Be());

    return msg;
}

} // namespace inet
