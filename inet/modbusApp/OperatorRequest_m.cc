//
// Generated file, do not edit! Created by opp_msgtool 6.1 from inet/applications/tcpapp/OperatorRequest.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "OperatorRequest_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace inet {

Register_Class(OperatorRequest)

OperatorRequest::OperatorRequest() : ::inet::FieldsChunk()
{
}

OperatorRequest::OperatorRequest(const OperatorRequest& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

OperatorRequest::~OperatorRequest()
{
}

OperatorRequest& OperatorRequest::operator=(const OperatorRequest& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void OperatorRequest::copy(const OperatorRequest& other)
{
    this->targetHostName = other.targetHostName;
    this->transactionId = other.transactionId;
    this->protocolId = other.protocolId;
    this->length = other.length;
    this->slaveId = other.slaveId;
    this->functionCode = other.functionCode;
    this->startAddress = other.startAddress;
    this->quantity = other.quantity;
}

void OperatorRequest::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->targetHostName);
    doParsimPacking(b,this->transactionId);
    doParsimPacking(b,this->protocolId);
    doParsimPacking(b,this->length);
    doParsimPacking(b,this->slaveId);
    doParsimPacking(b,this->functionCode);
    doParsimPacking(b,this->startAddress);
    doParsimPacking(b,this->quantity);
}

void OperatorRequest::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->targetHostName);
    doParsimUnpacking(b,this->transactionId);
    doParsimUnpacking(b,this->protocolId);
    doParsimUnpacking(b,this->length);
    doParsimUnpacking(b,this->slaveId);
    doParsimUnpacking(b,this->functionCode);
    doParsimUnpacking(b,this->startAddress);
    doParsimUnpacking(b,this->quantity);
}

const char * OperatorRequest::getTargetHostName() const
{
    return this->targetHostName.c_str();
}

void OperatorRequest::setTargetHostName(const char * targetHostName)
{
    handleChange();
    this->targetHostName = targetHostName;
}

uint16_t OperatorRequest::getTransactionId() const
{
    return this->transactionId;
}

void OperatorRequest::setTransactionId(uint16_t transactionId)
{
    handleChange();
    this->transactionId = transactionId;
}

uint16_t OperatorRequest::getProtocolId() const
{
    return this->protocolId;
}

void OperatorRequest::setProtocolId(uint16_t protocolId)
{
    handleChange();
    this->protocolId = protocolId;
}

uint16_t OperatorRequest::getLength() const
{
    return this->length;
}

void OperatorRequest::setLength(uint16_t length)
{
    handleChange();
    this->length = length;
}

uint8_t OperatorRequest::getSlaveId() const
{
    return this->slaveId;
}

void OperatorRequest::setSlaveId(uint8_t slaveId)
{
    handleChange();
    this->slaveId = slaveId;
}

uint8_t OperatorRequest::getFunctionCode() const
{
    return this->functionCode;
}

void OperatorRequest::setFunctionCode(uint8_t functionCode)
{
    handleChange();
    this->functionCode = functionCode;
}

uint16_t OperatorRequest::getStartAddress() const
{
    return this->startAddress;
}

void OperatorRequest::setStartAddress(uint16_t startAddress)
{
    handleChange();
    this->startAddress = startAddress;
}

uint16_t OperatorRequest::getQuantity() const
{
    return this->quantity;
}

void OperatorRequest::setQuantity(uint16_t quantity)
{
    handleChange();
    this->quantity = quantity;
}

class OperatorRequestDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_targetHostName,
        FIELD_transactionId,
        FIELD_protocolId,
        FIELD_length,
        FIELD_slaveId,
        FIELD_functionCode,
        FIELD_startAddress,
        FIELD_quantity,
    };
  public:
    OperatorRequestDescriptor();
    virtual ~OperatorRequestDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(OperatorRequestDescriptor)

OperatorRequestDescriptor::OperatorRequestDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::OperatorRequest)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

OperatorRequestDescriptor::~OperatorRequestDescriptor()
{
    delete[] propertyNames;
}

bool OperatorRequestDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<OperatorRequest *>(obj)!=nullptr;
}

const char **OperatorRequestDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *OperatorRequestDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int OperatorRequestDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 8+base->getFieldCount() : 8;
}

unsigned int OperatorRequestDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_targetHostName
        FD_ISEDITABLE,    // FIELD_transactionId
        FD_ISEDITABLE,    // FIELD_protocolId
        FD_ISEDITABLE,    // FIELD_length
        FD_ISEDITABLE,    // FIELD_slaveId
        FD_ISEDITABLE,    // FIELD_functionCode
        FD_ISEDITABLE,    // FIELD_startAddress
        FD_ISEDITABLE,    // FIELD_quantity
    };
    return (field >= 0 && field < 8) ? fieldTypeFlags[field] : 0;
}

const char *OperatorRequestDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "targetHostName",
        "transactionId",
        "protocolId",
        "length",
        "slaveId",
        "functionCode",
        "startAddress",
        "quantity",
    };
    return (field >= 0 && field < 8) ? fieldNames[field] : nullptr;
}

int OperatorRequestDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "targetHostName") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "transactionId") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "protocolId") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "length") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "slaveId") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "functionCode") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "startAddress") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "quantity") == 0) return baseIndex + 7;
    return base ? base->findField(fieldName) : -1;
}

const char *OperatorRequestDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_targetHostName
        "uint16_t",    // FIELD_transactionId
        "uint16_t",    // FIELD_protocolId
        "uint16_t",    // FIELD_length
        "uint8_t",    // FIELD_slaveId
        "uint8_t",    // FIELD_functionCode
        "uint16_t",    // FIELD_startAddress
        "uint16_t",    // FIELD_quantity
    };
    return (field >= 0 && field < 8) ? fieldTypeStrings[field] : nullptr;
}

const char **OperatorRequestDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *OperatorRequestDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int OperatorRequestDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void OperatorRequestDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'OperatorRequest'", field);
    }
}

const char *OperatorRequestDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string OperatorRequestDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        case FIELD_targetHostName: return oppstring2string(pp->getTargetHostName());
        case FIELD_transactionId: return ulong2string(pp->getTransactionId());
        case FIELD_protocolId: return ulong2string(pp->getProtocolId());
        case FIELD_length: return ulong2string(pp->getLength());
        case FIELD_slaveId: return ulong2string(pp->getSlaveId());
        case FIELD_functionCode: return ulong2string(pp->getFunctionCode());
        case FIELD_startAddress: return ulong2string(pp->getStartAddress());
        case FIELD_quantity: return ulong2string(pp->getQuantity());
        default: return "";
    }
}

void OperatorRequestDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        case FIELD_targetHostName: pp->setTargetHostName((value)); break;
        case FIELD_transactionId: pp->setTransactionId(string2ulong(value)); break;
        case FIELD_protocolId: pp->setProtocolId(string2ulong(value)); break;
        case FIELD_length: pp->setLength(string2ulong(value)); break;
        case FIELD_slaveId: pp->setSlaveId(string2ulong(value)); break;
        case FIELD_functionCode: pp->setFunctionCode(string2ulong(value)); break;
        case FIELD_startAddress: pp->setStartAddress(string2ulong(value)); break;
        case FIELD_quantity: pp->setQuantity(string2ulong(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OperatorRequest'", field);
    }
}

omnetpp::cValue OperatorRequestDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        case FIELD_targetHostName: return pp->getTargetHostName();
        case FIELD_transactionId: return (omnetpp::intval_t)(pp->getTransactionId());
        case FIELD_protocolId: return (omnetpp::intval_t)(pp->getProtocolId());
        case FIELD_length: return (omnetpp::intval_t)(pp->getLength());
        case FIELD_slaveId: return (omnetpp::intval_t)(pp->getSlaveId());
        case FIELD_functionCode: return (omnetpp::intval_t)(pp->getFunctionCode());
        case FIELD_startAddress: return (omnetpp::intval_t)(pp->getStartAddress());
        case FIELD_quantity: return (omnetpp::intval_t)(pp->getQuantity());
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'OperatorRequest' as cValue -- field index out of range?", field);
    }
}

void OperatorRequestDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        case FIELD_targetHostName: pp->setTargetHostName(value.stringValue()); break;
        case FIELD_transactionId: pp->setTransactionId(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_protocolId: pp->setProtocolId(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_length: pp->setLength(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_slaveId: pp->setSlaveId(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_functionCode: pp->setFunctionCode(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_startAddress: pp->setStartAddress(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_quantity: pp->setQuantity(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OperatorRequest'", field);
    }
}

const char *OperatorRequestDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr OperatorRequestDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void OperatorRequestDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    OperatorRequest *pp = omnetpp::fromAnyPtr<OperatorRequest>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OperatorRequest'", field);
    }
}

}  // namespace inet

namespace omnetpp {

}  // namespace omnetpp

