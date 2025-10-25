//
// Copyright (C) [Year] Your Name/Organization
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//

#ifndef __INET_OPERATORREQUESTSERIALIZER_H
#define __INET_OPERATORREQUESTSERIALIZER_H

#include "inet/common/packet/serializer/FieldsChunkSerializer.h"

namespace inet {

/**
 * Converts between OperatorRequest and binary (network byte order) packet.
 */
class INET_API OperatorRequestSerializer : public FieldsChunkSerializer
{
  protected:
    virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
    virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

  public:
    OperatorRequestSerializer() : FieldsChunkSerializer() {}
};

} // namespace inet

#endif // __INET_OPERATORREQUESTSERIALIZER_H
