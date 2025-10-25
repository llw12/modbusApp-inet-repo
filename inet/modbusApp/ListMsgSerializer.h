/*
 * ListMsgSerializer.h
 *
 *  Created on: Sep 20, 2025
 *      Author: llw
 */

#ifndef OPERATORSTATION_LISTMSGSERIALIZER_H_
#define OPERATORSTATION_LISTMSGSERIALIZER_H_

#include "inet/applications/base/ApplicationPacketSerializer.h"

namespace inet{
    class INET_API ListMsgSerializer : public ApplicationPacketSerializer
    {
      protected:
        virtual void serialize(MemoryOutputStream& stream, const Ptr<const Chunk>& chunk) const override;
        virtual const Ptr<Chunk> deserialize(MemoryInputStream& stream) const override;

      public:
        ListMsgSerializer() : ApplicationPacketSerializer() {}
    };

}



#endif
