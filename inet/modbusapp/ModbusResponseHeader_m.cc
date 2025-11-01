//
// Generated file, do not edit! Created by opp_msgtool 6.1 from inet/applications/modbusapp/ModbusResponseHeader.msg.
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
#include "ModbusResponseHeader_m.h"

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

Register_Class(ModbusResponseHeader)

ModbusResponseHeader::ModbusResponseHeader() : ::inet::FieldsChunk()
{
    this->setChunkLength(B(12));

}

ModbusResponseHeader::ModbusResponseHeader(const ModbusResponseHeader& other) : ::inet::FieldsChunk(other)
{
    copy(other);
}

ModbusResponseHeader::~ModbusResponseHeader()
{
}

ModbusResponseHeader& ModbusResponseHeader::operator=(const ModbusResponseHeader& other)
{
    if (this == &other) return *this;
    ::inet::FieldsChunk::operator=(other);
    copy(other);
    return *this;
}

void ModbusResponseHeader::copy(const ModbusResponseHeader& other)
{
    this->transactionId = other.transactionId;
    this->protocolId = other.protocolId;
    this->length = other.length;
    this->slaveId = other.slaveId;
    this->functionCode = other.functionCode;
}

void ModbusResponseHeader::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::inet::FieldsChunk::parsimPack(b);
    doParsimPacking(b,this->transactionId);
    doParsimPacking(b,this->protocolId);
    doParsimPacking(b,this->length);
    doParsimPacking(b,this->slaveId);
    doParsimPacking(b,this->functionCode);
}

void ModbusResponseHeader::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::inet::FieldsChunk::parsimUnpack(b);
    doParsimUnpacking(b,this->transactionId);
    doParsimUnpacking(b,this->protocolId);
    doParsimUnpacking(b,this->length);
    doParsimUnpacking(b,this->slaveId);
    doParsimUnpacking(b,this->functionCode);
}

uint16_t ModbusResponseHeader::getTransactionId() const
{
    return this->transactionId;
}

void ModbusResponseHeader::setTransactionId(uint16_t transactionId)
{
    handleChange();
    this->transactionId = transactionId;
}

uint16_t ModbusResponseHeader::getProtocolId() const
{
    return this->protocolId;
}

void ModbusResponseHeader::setProtocolId(uint16_t protocolId)
{
    handleChange();
    this->protocolId = protocolId;
}

uint16_t ModbusResponseHeader::getLength() const
{
    return this->length;
}

void ModbusResponseHeader::setLength(uint16_t length)
{
    handleChange();
    this->length = length;
}

uint8_t ModbusResponseHeader::getSlaveId() const
{
    return this->slaveId;
}

void ModbusResponseHeader::setSlaveId(uint8_t slaveId)
{
    handleChange();
    this->slaveId = slaveId;
}

uint8_t ModbusResponseHeader::getFunctionCode() const
{
    return this->functionCode;
}

void ModbusResponseHeader::setFunctionCode(uint8_t functionCode)
{
    handleChange();
    this->functionCode = functionCode;
}

class ModbusResponseHeaderDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_transactionId,
        FIELD_protocolId,
        FIELD_length,
        FIELD_slaveId,
        FIELD_functionCode,
    };
  public:
    ModbusResponseHeaderDescriptor();
    virtual ~ModbusResponseHeaderDescriptor();

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

Register_ClassDescriptor(ModbusResponseHeaderDescriptor)

ModbusResponseHeaderDescriptor::ModbusResponseHeaderDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(inet::ModbusResponseHeader)), "inet::FieldsChunk")
{
    propertyNames = nullptr;
}

ModbusResponseHeaderDescriptor::~ModbusResponseHeaderDescriptor()
{
    delete[] propertyNames;
}

bool ModbusResponseHeaderDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<ModbusResponseHeader *>(obj)!=nullptr;
}

const char **ModbusResponseHeaderDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *ModbusResponseHeaderDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int ModbusResponseHeaderDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 5+base->getFieldCount() : 5;
}

unsigned int ModbusResponseHeaderDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_transactionId
        FD_ISEDITABLE,    // FIELD_protocolId
        FD_ISEDITABLE,    // FIELD_length
        FD_ISEDITABLE,    // FIELD_slaveId
        FD_ISEDITABLE,    // FIELD_functionCode
    };
    return (field >= 0 && field < 5) ? fieldTypeFlags[field] : 0;
}

const char *ModbusResponseHeaderDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "transactionId",
        "protocolId",
        "length",
        "slaveId",
        "functionCode",
    };
    return (field >= 0 && field < 5) ? fieldNames[field] : nullptr;
}

int ModbusResponseHeaderDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "transactionId") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "protocolId") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "length") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "slaveId") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "functionCode") == 0) return baseIndex + 4;
    return base ? base->findField(fieldName) : -1;
}

const char *ModbusResponseHeaderDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "uint16_t",    // FIELD_transactionId
        "uint16_t",    // FIELD_protocolId
        "uint16_t",    // FIELD_length
        "uint8_t",    // FIELD_slaveId
        "uint8_t",    // FIELD_functionCode
    };
    return (field >= 0 && field < 5) ? fieldTypeStrings[field] : nullptr;
}

const char **ModbusResponseHeaderDescriptor::getFieldPropertyNames(int field) const
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

const char *ModbusResponseHeaderDescriptor::getFieldProperty(int field, const char *propertyName) const
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

int ModbusResponseHeaderDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void ModbusResponseHeaderDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'ModbusResponseHeader'", field);
    }
}

const char *ModbusResponseHeaderDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string ModbusResponseHeaderDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        case FIELD_transactionId: return ulong2string(pp->getTransactionId());
        case FIELD_protocolId: return ulong2string(pp->getProtocolId());
        case FIELD_length: return ulong2string(pp->getLength());
        case FIELD_slaveId: return ulong2string(pp->getSlaveId());
        case FIELD_functionCode: return ulong2string(pp->getFunctionCode());
        default: return "";
    }
}

void ModbusResponseHeaderDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        case FIELD_transactionId: pp->setTransactionId(string2ulong(value)); break;
        case FIELD_protocolId: pp->setProtocolId(string2ulong(value)); break;
        case FIELD_length: pp->setLength(string2ulong(value)); break;
        case FIELD_slaveId: pp->setSlaveId(string2ulong(value)); break;
        case FIELD_functionCode: pp->setFunctionCode(string2ulong(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ModbusResponseHeader'", field);
    }
}

omnetpp::cValue ModbusResponseHeaderDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        case FIELD_transactionId: return (omnetpp::intval_t)(pp->getTransactionId());
        case FIELD_protocolId: return (omnetpp::intval_t)(pp->getProtocolId());
        case FIELD_length: return (omnetpp::intval_t)(pp->getLength());
        case FIELD_slaveId: return (omnetpp::intval_t)(pp->getSlaveId());
        case FIELD_functionCode: return (omnetpp::intval_t)(pp->getFunctionCode());
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'ModbusResponseHeader' as cValue -- field index out of range?", field);
    }
}

void ModbusResponseHeaderDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        case FIELD_transactionId: pp->setTransactionId(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_protocolId: pp->setProtocolId(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_length: pp->setLength(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_slaveId: pp->setSlaveId(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_functionCode: pp->setFunctionCode(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ModbusResponseHeader'", field);
    }
}

const char *ModbusResponseHeaderDescriptor::getFieldStructName(int field) const
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

omnetpp::any_ptr ModbusResponseHeaderDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void ModbusResponseHeaderDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    ModbusResponseHeader *pp = omnetpp::fromAnyPtr<ModbusResponseHeader>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ModbusResponseHeader'", field);
    }
}

}  // namespace inet

namespace omnetpp {

}  // namespace omnetpp

