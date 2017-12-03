#include "opcuaclient.h"

/*
----------------------Additional functions to create node display.
*/
static void
handler_TheAnswerChanged(UA_UInt32 monId, UA_DataValue *value, void *context) {
    printf("The Answer has changed!\n");
}

static UA_StatusCode
nodeIter(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void *handle) {
    if(isInverse)
        return UA_STATUSCODE_GOOD;
    UA_NodeId *parent = (UA_NodeId *)handle;
    printf("%d, %d --- %d ---> NodeId %d, %d\n",
           parent->namespaceIndex, parent->identifier.numeric,
           referenceTypeId.identifier.numeric, childId.namespaceIndex,
           childId.identifier.numeric);
    return UA_STATUSCODE_GOOD;
}
//---------------------End additional functions.

OpcUaClient::OpcUaClient()
{
    client = UA_Client_new(UA_ClientConfig_standard);
}

bool OpcUaClient::connect()
{
    statusCode = UA_Client_connect(client, endpointUrl.c_str());
    if(statusCode != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return false;
    }else return true;
}

void OpcUaClient::printNode()
{
    /* Same thing, this time using the node iterator... */
    std::cout << "Connected to endpoint:";
    std::cout << this->endpointUrl << std::endl;
    UA_NodeId *parent = UA_NodeId_new();
    *parent = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_Client_forEachChildNodeCall(client, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                   nodeIter, (void *) parent);
    UA_NodeId_delete(parent);
}


void OpcUaClient::setModbusNode(int nodeId, std::string nodeName)
{
    this->modbusNodeId = nodeId;
    this->modbusNodeName = nodeName;
}

