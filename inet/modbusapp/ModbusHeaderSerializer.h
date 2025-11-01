//
// Copyright (C) [Year] Your Name/Organization
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_MODBUSMSGSERIALIZER_H
#define __INET_MODBUSMSGSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

namespace inet {

/**
 * Converts between ModbusMsg and binary (network byte order) Modbus packet.
 */
class INET_API ModbusHeaderSerializer : public FieldsChunkSerializer
{
  public:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;


    ModbusHeaderSerializer() : FieldsChunkSerializer() {}
};

} // namespace inet

#endif
