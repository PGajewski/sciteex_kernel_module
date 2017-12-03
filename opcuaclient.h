#ifndef WRITEOPCUA_H
#define WRITEOPCUA_H
extern "C" {
# include "open62541.h"
}
#include <string>
#include <iostream>
#include <sstream>
#include <typeinfo>
#include "type_name.h"

//Serialization
#include <cereal/access.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>



extern "C" UA_Client * UA_Client_new(UA_ClientConfig config);

class OpcUaClient
{
public:

    OpcUaClient();
    bool connect();
    void connectWithUsername();
    void setModbusNode(int nodeId, std::string nodeName);
    void printNode();

    template <typename T>
    T read(std::string variableName);

    template <typename T>
    T read(int nodeID, std::string nodeName);

    template <typename T>
    void write(std::string variableName, T value);

    template <typename T>
    void write(int nodeID, std::string nodeName, T value);

    //Serialization.
    template<class Archive>
    inline void save(
        Archive & ar,
        const unsigned int version
    ) const
    {
        ar & serverName;
        ar & endpointUrl;
        ar & userName;
        ar & password;
    }

    template<class Archive>
    inline void load(
        Archive & archive
    ){
        archive(serverName,
                endpointUrl,
                userName,
                password);
    }

    //Getters
    inline std::string getServerName()
    {
        return this->serverName;
    }

    inline std::string getEndpointUrl()
    {
        return this->endpointUrl;
    }

    inline std::string getUserName()
    {
        return this->userName;
    }

    inline std::string getPassword()
    {
        return this->password;
    }

    inline std::string getClientName()
    {
        return this->serverName;
    }

    inline bool getState()
    {
        return this->isInitialized;
    }

    //Setters
    inline void setServerName(std::string serverName)
    {
        this->serverName = serverName;
    }

    inline void setEndpointUrl(std::string endpointUrl)
    {
        this->endpointUrl = endpointUrl;
    }

    inline void setUserName(std::string userName)
    {
        this->userName = userName;
    }

    inline void setPassword(std::string password)
    {
        this->password = password;
    }

    inline void setClientName(std::string clientName)
    {
        this->clientName = clientName;
    }

    inline void setState(bool state)
    {
        this->isInitialized = state;
    }

private:

    template <typename T>
    UA_Variant * parseType(T value);

    std::string serverName;
    std::string clientName;
    std::string userName;
    std::string password;
    UA_Client * client;
    UA_StatusCode statusCode;
    std::string endpointUrl;
    int modbusNodeId;
    std::string modbusNodeName;
    bool isInitialized = false;
};

template <typename T>
UA_Variant * OpcUaClient::parseType(T value)
{
    /* Write node attribute (using the highlevel API) */
    UA_Variant *myVariant = UA_Variant_new();

    /* Choose type of variable */
    std::string typeName = type_name<decltype(value)>();

    if(typeName == "bool")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_BOOLEAN]);
        return myVariant;
    }

    if(typeName == "signed char")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_SBYTE]);
        return myVariant;
    }

    if(typeName == "unsigned char")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_BYTE]);
        return myVariant;
    }

    if(typeName == "short")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_INT16]);
        return myVariant;
    }

    if(typeName == "unsigned short")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_UINT16]);
        return myVariant;
    }

    if(typeName == "int")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_INT32]);
        return myVariant;
    }

    if(typeName == "unsigned")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_UINT32]);
        return myVariant;
    }

    if(typeName == "long long")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_INT64]);
        return myVariant;
    }

    if(typeName == "unsigned long long")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_UINT64]);
        return myVariant;
    }

    if(typeName == "float")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_FLOAT]);
        return myVariant;
    }

    if(typeName == "double")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_DOUBLE]);
        return myVariant;
    }

    if(typeName == "std::string")
    {
        UA_Variant_setScalarCopy(myVariant, &value, &UA_TYPES[UA_TYPES_STRING]);
        return myVariant;
    }
    return nullptr;
}

template<typename T>
T OpcUaClient::read(int nodeID, std::string nodeName)
{
    /* Read attribute */
    T value = 0;
    std::cout << "\nReading the value of node (" << nodeID << ", " << nodeName << "):" << std::endl;
    UA_Variant *val = UA_Variant_new();
    statusCode = UA_Client_readValueAttribute(client, UA_NODEID_STRING(nodeID, const_cast<char*>(nodeName.c_str())), val);
    UA_Variant_delete(val);
    return value;
}

template <typename T>
void OpcUaClient::write(int nodeID, std::string nodeName, T value)
{
    UA_Variant *myVariant = this->parseType<T>(value);
    UA_Client_writeValueAttribute(client, UA_NODEID_STRING(nodeID, const_cast<char*>(nodeName.c_str())), myVariant);
    UA_Variant_delete(myVariant);
}

template<typename T>
T OpcUaClient::read(std::string variableName)
{
    return this->read<T>(this->modbusNodeId,std::string(this->modbusNodeName) + std::string(variableName));
}

template <typename T>
void OpcUaClient::write(std::string variableName, T value)
{
    this->write<T>(this->modbusNodeId,std::string(this->modbusNodeName) + std::string(variableName), value);
}

#endif // WRITEOPCUA_H
