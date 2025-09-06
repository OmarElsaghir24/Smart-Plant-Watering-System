// TCP Library (includes framework only)
// Jason Losh

// Omar Elsaghir
// ID: 1001768494
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include "arp.h"
#include "mqtt.h"
#include "tcp.h"
#include "timer.h"
#include "gpio.h"
#include "uart0.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

#define MAX_TCP_PORTS 4
#define ARP_SENT 11
#define MAX_TOPIC_LENGTH 128
#define MAX_PAYLOAD_LENGTH 128
#define KEEP_ALIVE_INTERVAL 60
#define PINGRESP_TIMEOUT 3
#define BLUE_LED PORTF,2

// Structure to keep track of socket and ether data. Created for use with timer functions I implemented in timer.c.
// Was going to be used for sending messages within the callback functions.
typedef struct {
    etherHeader *ether;
    socket *s;
} TcpCallbackData;

uint16_t tcpPorts[MAX_TCP_PORTS];
uint8_t tcpPortCount = 0;
uint8_t tcpState[MAX_TCP_PORTS];
uint8_t instance = 0;
uint8_t state;
uint16_t packetId;
etherHeader globalEther;
socket *globalSocket;
//socket *s;
bool portOpen = false;
bool sendSyn = false;
bool synAck = false;
bool finRec = false;
bool endTimer = false;
bool beginTimer = false;
bool mqttAck = false;
bool tcpClose = false;
bool ackRec = false;
bool finSend = false;
bool disconnect = false;
bool finAckRec = false;
bool Ack = false;
bool ackSend = false;
bool finAckSend = false;
bool pubRec = false;
bool pubCom = false;
bool subAck = false;
bool unSubAck = false;
bool waitingForPingResp = false;
bool pingRespRec = false;
bool mqttConnStatus = false;
bool AcK = false;
bool Fin = false;
bool finWait = false;
char Buffer[8] = {'B', 'l', 'u', 'e', 'L', 'e', 'd', '\0'};
char Data[3] = {'O', 'N', '\0'};
char Data1[4] = {'O', 'F', 'F', '\0'};
char Buffer1[8] = {'m', 'e', 's', 's', 'a', 'g', 'e', '\0'};
char Data2[11] = {'H', 'e', 'l', 'l', 'o', '_', 'O', 'm', 'a', 'r', '\0'};
char str[100];

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------


// Set TCP state
void setTcpState(uint8_t instance, uint8_t state)
{
    tcpState[instance] = state;
}

// Get TCP state
uint8_t getTcpState(uint8_t instance)
{
    return tcpState[instance];
}

// Restart connection with MQTT Broker by first sending an Arp Request then enter ESTABLISHED state in FSM, then connect to MQTT Broker
void tcpOpen(void)
{
	uint8_t buffer[1518];
	etherHeader *eth = (etherHeader*)buffer;
	uint8_t MQTTIP[IP_ADD_LENGTH];
	uint8_t LOCALIP[IP_ADD_LENGTH];
	getIpAddress(LOCALIP);
	getIpMqttBrokerAddress(MQTTIP);
	sendArpRequest(eth, LOCALIP, MQTTIP);
}

// Check which state I'm currently in (For status command)
void displayState(void)
{
    if(tcpState[instance] == TCP_SYN_SENT)
    {
        putsUart0("Current State: SYN Sent\n");
    }
    if(tcpState[instance] == TCP_ESTABLISHED)
    {
        putsUart0("Current State: Established\n");
    }
    if(tcpState[instance] == TCP_FIN_WAIT_1)
    {
        putsUart0("Current State: Closed\n");
    }
    if(tcpState[instance] == TCP_FIN_WAIT_2)
    {
        putsUart0("Current State: Closed\n");
    }
    if(tcpState[instance] == TCP_CLOSE_WAIT)
    {
        putsUart0("Current State: Closed\n");
    }
    if(tcpState[instance] == TCP_LAST_ACK)
    {
        putsUart0("Current State: Closed\n");
    }
    if(tcpState[instance] == TCP_CLOSED)
    {
        putsUart0("Current State: Closed\n");
    }
}

// Check for connection status with MQTT Broker (For status command)
void mqttConnectionStatus(void) {
    if(mqttConnStatus)
    {
        putsUart0("Connected to MQTT\n");
    }
    else if(!mqttConnStatus)
    {
        putsUart0("Disconnected from MQTT\n");
    }
}

void pingRespTimeoutCallback(void *arg)
{
    uint8_t instance = *(uint8_t *)arg;  // Extract instance number
    setTcpState(instance, TCP_CLOSED);
    waitingForPingResp = false;
    //stopTimer(sendPingReqCallback);
}

// Send a ping request every time the time interval (60 seconds) expires. Accomplished using a Oneshot timer
void sendPingReqCallback(void *arg)
{
    TcpCallbackData *data = (TcpCallbackData *)arg;

    uint8_t pingReqPacket[2] = {0xC0, 0x00}; // MQTT PINGREQ
    sendTcpMessage(data->ether, data->s, PSH | ACK, pingReqPacket, sizeof(pingReqPacket));

    waitingForPingResp = true;

    // Start timeout timer for PINGRESP
    StartOneshotTimer(pingRespTimeoutCallback, &instance, PINGRESP_TIMEOUT);
}

bool isTimerRunning(void (*callback)(void *))
{
    return (callback == sendPingReqCallback && waitingForPingResp);
}

// Resends the ping request after 60 seconds
void checkKeepAlive(void *arg)
{
    etherHeader *ether = (etherHeader *)arg;

    if (tcpState[instance] == TCP_ESTABLISHED && !waitingForPingResp)
    {
        // Start one-shot timer to send PINGREQ
        StartOneshotTimer(sendPingReqCallback, ether, KEEP_ALIVE_INTERVAL);
    }
}

void callBackSentTimeout(void *arg)
{
    (void)arg;
    setTcpState(instance, TCP_SYN_SENT);
}

void callBackEstablishedTimeout(void *arg)
{
    (void)arg;
    setTcpState(instance, TCP_ESTABLISHED);
}

void callBackEstablishedCloseTimeout(void *arg)
{
    (void)arg;
    setTcpState(instance, MQTT_DISCONNECT);
}

void callBackSentStart(void *arg)
{
    TcpCallbackData *data = (TcpCallbackData *)arg;
    sendTcpMessage(data->ether, data->s, SYN, NULL, 0);
    setTcpState(instance, TCP_SYN_SENT);

}

void callBackEstablished(void *arg)
{
    TcpCallbackData *data = (TcpCallbackData *)arg;
    sendTcpMessage(data->ether, data->s, PSH | ACK, mqttBuff, mqttLength);
    setTcpState(instance, TCP_ESTABLISHED);
}

void callBackEstablishedClose(void *arg)
{
    TcpCallbackData *data = (TcpCallbackData *)arg;
    sendTcpMessage(data->ether, data->s, PSH | ACK, mqttBuff, mqttLength);
    setTcpState(instance, MQTT_DISCONNECT);
}

// Can create a flag for auto connect
// Determines whether packet is TCP packet
// Must be an IP packet
bool isTcp(etherHeader* ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    bool ok;
    uint16_t tmp16;
    uint32_t sum = 0;
    ok = (ip->protocol == PROTOCOL_TCP);
    if(ok)
    {
        // Compute TCP checksum
        // Pseudo-header checksum
        uint16_t tcpLength = ntohs(ip->length) - ipHeaderLength;
        sumIpWords(ip->sourceIp, 8, &sum);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        sum += htons(tcpLength);
        //uint16_t tcpLength = ntohs(ip->length) - ipHeaderLength;
        //sumIpWords(&tcpLength, 2, &sum);
        // Add TCP header and data to checksum
        sumIpWords(tcp, tcpLength, &sum);
        ok = (getIpChecksum(sum) == 0);
    }
    return ok;
}

// Check for a SYN message sent. Requires accessing the offsetFields variable since it keeps track of flags sent
bool isTcpSyn(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    uint16_t offsetFields = ntohs(tcp->offsetFields);
    uint8_t flags = offsetFields & 0x3F;

    return (flags & SYN) != 0;
}

// Check for an ACK message sent. Requires accessing the offsetFields variable since it keeps track of flags sent.
bool isTcpAck(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    uint16_t offsetFields = ntohs(tcp->offsetFields);
    uint8_t flags = offsetFields & 0x3F;

    return (flags & ACK) != 0;
}

// Check if a Connect Acknowledgment (CONNACK) is sent. Will be sent after sending a CONNECT message to the broker.
bool isMqttConnAck(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Fixing TCP Header Length calculation
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT CONNACK message has a control header of 0x20 and remaining length of 2
    if (mqtt->controlHdr == 0x20 && mqtt->remainingLength == 2)
    {
        uint8_t returnCode = mqtt->variableHeader[1]; // Connection return code
        if (returnCode == 0x00)
        {
            return true;  // Connection Accepted
        }
    }

    return false;  // No CONNACK received or connection failed
}

// Check for a disconnect message. Initiated by the client
bool isMqttDisconnect(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Fixing TCP Header Length calculation
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT CONNACK message has a control header of 0x20 and remaining length of 2
    if (mqtt->controlHdr == 0xE0 && mqtt->remainingLength == 0)
    {
        //uint8_t returnCode = mqtt->variableHeader[1]; // Connection return code
        //if (returnCode == 0x00)
        //{
            return true;  // Connection Accepted
        //}
    }

    return false;  // No CONNACK received or connection failed
}

// Check if a Publish Receive message is sent
bool isMqttPubRec(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Calculate TCP Header Length properly
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT PUBREC message has a control header of 0x50 and a remaining length of 2
    if (mqtt->controlHdr == 0x50 && mqtt->remainingLength == 2)
    {
        packetId = (mqtt->variableHeader[0] << 8) | mqtt->variableHeader[1];  // Extract Packet ID
        return true;  // PUBREC received
    }

    return false;  // No PUBREC received
}

// Check if a Publish message is sent
bool isMqttPublish(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Fixing TCP Header Length calculation
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;
    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT PUBLISH message should have control header with bits 7-4 as 0x3 (0x30 to 0x3F based on QoS and retain flags)
    if (mqtt->controlHdr == 0x30)
    {
        // Extract topic length
        uint16_t topicLength;
        memcpy(&topicLength, mqtt->variableHeader, sizeof(uint16_t));
        topicLength = ntohs(topicLength); // Convert from network byte order

        if (topicLength > MAX_TOPIC_LENGTH) return false; // Prevent overflow

        // Extract topic string
        char topic[MAX_TOPIC_LENGTH + 1];
        memcpy(topic, mqtt->variableHeader + 2, topicLength);
        topic[topicLength] = '\0'; // Null-terminate

        // Compute payload position
        uint8_t *payloadStart = mqtt->variableHeader + 2 + topicLength;
        uint16_t payloadLength = mqtt->remainingLength - (2 + topicLength);

        // Compare extracted topic with relevant topic
        if(strcmp(topic, Buffer) == 0)
        {
            snprintf(str, sizeof(str), "Topic: %s\n", topic);
            putsUart0(str);

            if (payloadLength > 0)  // Ensure there's data
           {
               char payload[MAX_PAYLOAD_LENGTH + 1]; // Define max payload size
               if (payloadLength > MAX_PAYLOAD_LENGTH) payloadLength = MAX_PAYLOAD_LENGTH;
               memcpy(payload, payloadStart, payloadLength);
               payload[payloadLength] = '\0'; // Null-terminate

               snprintf(str, sizeof(str), "Payload: %s\n", payload);
               putsUart0(str);
               if(strcmp(payload, Data) == 0) {
                   setPinValue(BLUE_LED, 1);
               }
               else if(strcmp(payload, Data1) == 0) {
                   setPinValue(BLUE_LED, 0);
               }
           }
            //return true;  // PUBLISH message with relevant topic
        }
        else if(strcmp(topic, Buffer1) == 0)
        {
            snprintf(str, sizeof(str), "Topic: %s\n", topic);
            putsUart0(str);

            if (payloadLength > 0)  // Ensure there's data
            {
               char payload[MAX_PAYLOAD_LENGTH + 1]; // Define max payload size
               if (payloadLength > MAX_PAYLOAD_LENGTH) payloadLength = MAX_PAYLOAD_LENGTH;
               memcpy(payload, payloadStart, payloadLength);
               payload[payloadLength] = '\0'; // Null-terminate

               snprintf(str, sizeof(str), "Payload: %s\n", payload);
               putsUart0(str);
            }
        }
        return true;
    }

    return false;  // Not a relevant PUBLISH message
}

// Check if a Ping Response is sent after sending a Ping Request
bool isMqttPingResp(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Calculate TCP Header Length properly
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT PINGRESP message has a control header of 0xD0 and a remaining length of 0
    if (mqtt->controlHdr == 0xD0 && mqtt->remainingLength == 0)
    {
        //packetId = (mqtt->variableHeader[0] << 8) | mqtt->variableHeader[1];  // Extract Packet ID
        return true;  // PINGRESP received
    }

    return false;  // No PINGRESP received
}

// Check if I received a Publish Complete message
bool isMqttPubCom(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Calculate TCP Header Length properly
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT PUBREC message has a control header of 0x50 and a remaining length of 2
    if (mqtt->controlHdr == 0x70 && mqtt->remainingLength == 2)
    {
        //packetId = (mqtt->variableHeader[0] << 8) | mqtt->variableHeader[1];  // Extract Packet ID
        return true;  // PUBREC received
    }

    return false;  // No PUBREC received
}

// Check if I received a SUBACK message
bool isMqttSubAck(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Calculate TCP Header Length properly
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT SUBACK message has a control header of 0x90 and a remaining length of 3 (for one topic)
    if (mqtt->controlHdr == 0x90 && mqtt->remainingLength >= 3)
    {
        // Extract Packet ID (Big Endian)
        uint16_t packetId = (mqtt->variableHeader[0] << 8) | mqtt->variableHeader[1];

        // Extract Return Code (should be 0x00, 0x01, 0x02, or 0x80)
        uint8_t returnCode = mqtt->variableHeader[2];

        // Verify if a valid return code is received
        if (returnCode == 0x00 || returnCode == 0x01 || returnCode == 0x02 || returnCode == 0x80)
        {
            return true;  // Valid SUBACK received
        }
    }

    return false;  // No valid SUBACK received
}

// Check if I received an UNSUBACK message
bool isMqttUnSubAck(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    // Calculate TCP Header Length properly
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;

    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    // MQTT SUBACK message has a control header of 0x90 and a remaining length of 3 (for one topic)
    if (mqtt->controlHdr == 0xB0 && mqtt->remainingLength == 2)
    {
        // Extract Packet ID (Big Endian)
        uint16_t packetId = (mqtt->variableHeader[0] << 8) | mqtt->variableHeader[1];

        return true;
    }

    return false;  // No valid SUBACK received
}

// Send an MQTT Publish Release message
void sendMqttPubRel(etherHeader *ether, socket *s, uint16_t packetId)
{
    uint8_t mqttPacket[4];

    mqttPacket[0] = 0x62;              // PUBREL Control Header
    mqttPacket[1] = 0x02;              // Remaining Length
    mqttPacket[2] = (packetId >> 8);   // Packet ID MSB
    mqttPacket[3] = (packetId & 0xFF); // Packet ID LSB

    sendTcpMessage(ether, s, PSH | ACK, mqttPacket, sizeof(mqttPacket));
}

// Check if a FIN flag is sent
bool isTcpFin(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    uint16_t offsetFields = ntohs(tcp->offsetFields);
    uint8_t flags = offsetFields & 0x3F;

    return (flags & FIN) != 0;
}

// Detect for a PSH flag
bool isTcpPsh(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    uint16_t offsetFields = ntohs(tcp->offsetFields);
    uint8_t flags = offsetFields & 0x3F;

    return (flags & PSH) != 0;
}

// Send any messages needed to Broker (For connecting, sending ping request, publishing, subscribing, unsubscribing, and disconnecting
void sendTcpPendingMessages(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;
    mqttFrame *mqtt = (mqttFrame*)((uint8_t*)tcp + tcpHeaderLength);

    socket *s = getSocket(instance);
    s->remotePort = 1883;
    s->localPort = 50234;
    static TcpCallbackData tcpData;
    tcpData.ether = ether;
    tcpData.s = s;

     if(sendSyn)
     {
        s->sequenceNumber = random32();
        s->acknowledgementNumber = 0;
        sendTcpMessage(ether, s, SYN, NULL, 0);
        sendSyn = false;
        setTcpState(instance, TCP_SYN_SENT);
        // Allocate memory for callback data
        //static TcpCallbackData tcpData;
        //tcpData.ether = ether;
        //tcpData.s = s;
        // Start one shot timer with data
        //startOneshotTimer(callBackSentStart, (void*)&tcpData, 5);

    }
    if(tcpState[instance] == TCP_SYN_SENT && synAck)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        synAck = false;
        setTcpState(instance, TCP_ESTABLISHED);
        //startPeriodicTimer(callBackEstablished, 10);
        //synAck = false;
    }
    if(tcpState[instance] == TCP_ESTABLISHED && pshNeeded)
    {
        sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);
        pshNeeded = false;
        //static TcpCallbackData tcpData;
        //tcpData.ether = ether;
        //tcpData.s = s;
        setTcpState(instance, TCP_ESTABLISHED);
        //startOneshotTimer(callBackEstablished, (void*)&tcpData, 5);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && mqttAck)
    {
           sendTcpMessage(ether, s, ACK, NULL, 0);
           mqttAck = false;
           setTcpState(instance, TCP_ESTABLISHED);
    }
    if (tcpState[instance] == TCP_ESTABLISHED && !waitingForPingResp)
    {
        // Start one-shot timer to send PINGREQ
        StartOneshotTimer(sendPingReqCallback, (void*)&tcpData, KEEP_ALIVE_INTERVAL);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && pingRespRec)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        pingRespRec = false;
        setTcpState(instance, TCP_ESTABLISHED);
    }

    if(tcpState[instance] == TCP_ESTABLISHED && publish)
    {
        sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);
        publish = false;
        //static TcpCallbackData tcpData;
        //tcpData.ether = ether;
        //tcpData.s = s;
        setTcpState(instance, TCP_ESTABLISHED);
        //startOneshotTimer(callBackEstablished, (void*)&tcpData, 5);
    }

    if(tcpState[instance] == TCP_ESTABLISHED && pubCom)
    {
        pubCom = false;
        setTcpState(instance, TCP_ESTABLISHED);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && subscribe)
    {
        sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);
        subscribe = false;
        //static TcpCallbackData tcpData;
        //tcpData.ether = ether;
        //tcpData.s = s;
        setTcpState(instance, TCP_ESTABLISHED);
        //startOneshotTimer(callBackEstablished, (void*)&tcpData, 5);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && subAck)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        subAck = false;
        setTcpState(instance, TCP_ESTABLISHED);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && pubRec)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        pubRec = false;
        setTcpState(instance, TCP_ESTABLISHED);
    }

    if(tcpState[instance] == TCP_ESTABLISHED && unsubscribe)
    {
        sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);
        unsubscribe = false;
        setTcpState(instance, TCP_ESTABLISHED);
        //startOneshotTimer(callBackEstablished, (void*)&tcpData, 5);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && unSubAck)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        unSubAck = false;
        setTcpState(instance, TCP_ESTABLISHED);
    }
    if(tcpState[instance] == TCP_ESTABLISHED && closeConn)
    {
        sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);
        s->sequenceNumber += 2;
        sendTcpMessage(ether, s, FIN | ACK, NULL, 0);
        closeConn = false;
        stopTimer(pingRespTimeoutCallback);
        setTcpState(instance, TCP_FIN_WAIT_1);
        //startOneshotTimer(callBackEstablishedClose, (void*)&tcpData, 5);
    }
    if(tcpState[instance] == TCP_FIN_WAIT_1 && Fin)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        Fin = false;
        setTcpState(instance, TCP_CLOSE_WAIT);
    }

    if(tcpState[instance] == TCP_CLOSE_WAIT && finSend)
    {
		//sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);
		sendTcpMessage(ether, s, FIN | ACK, NULL, 0);
		finSend = false;
		setTcpState(instance, TCP_LAST_ACK);
    }
    if(tcpState[instance] == TCP_LAST_ACK && finAckSend)
    {
        sendTcpMessage(ether, s, ACK, NULL, 0);
        finAckSend = false;
        deleteSocket(s);
        setTcpState(instance, TCP_CLOSED);
    }

    if(tcpState[instance] == TCP_CLOSING && ackRec){
        //sendTcpMessage(ether, s, FIN | ACK, NULL, 0);
    	deleteSocket(s);
        ackRec = false;
        setTcpState(instance, TCP_CLOSED);
    }

}

// Process any messages sent from the Broker. Also being used for handling implementation of entire closing stage of TCP
// state machine, which involves completing the four-way handshake.
void processTcpResponse(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);
    uint8_t tcpHeaderLength = ((tcp->offsetFields >> 4) & 0xF) * 4;
    mqttFrame *mqtt = (mqttFrame*)(uint8_t*)tcp + sizeof(tcpHeader);
    uint8_t Data = ntohs(ip->length) - ipHeaderLength - tcpHeaderLength;

    socket *s = getSocket(instance);

    if(tcpState[instance] == TCP_SYN_SENT && (isTcpSyn(ether) && isTcpAck(ether)))
    {
         //endTimer = stopTimer(callBackSentTimeout);
        //if(isTcpSyn(ether) && isTcpAck(ether)) {
         //stopTimer(callBackSentTimeout);
         synAck = true;
         getSocketInfoFromTcpPacket(ether, s);
         s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
         s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + 1;
    }

   if(tcpState[instance] == TCP_ESTABLISHED && isMqttConnAck(ether))
   {
       //stopTimer(callBackEstablishedTimeout);
       getSocketInfoFromTcpPacket(ether, s);
       //s->sequenceNumber = ntohl(tcp->sequenceNumber) + Data;
       s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
       s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + Data;   // Update with sequence number + datasize from CONNACK packet instead of sequence number + 1
       mqttAck = true;
       mqttConnStatus = true;
   }
   if (tcpState[instance] && waitingForPingResp && isMqttPingResp(ether))
   {
           waitingForPingResp = false;
           getSocketInfoFromTcpPacket(ether, s);
           s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
           s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + Data;
           //pingRespRec = true;
           if (!restartTimer(sendPingReqCallback)) {
               // Fallback: Start a new timer if restart fails
               StartOneshotTimer(checkKeepAlive, ether, KEEP_ALIVE_INTERVAL);
           }
           pingRespRec = true;
           mqttConnStatus = true;
   }

   if(tcpState[instance] == TCP_ESTABLISHED && isTcpAck(ether))
   {
       //stopTimer(callBackEstablishedTimeout);
       getSocketInfoFromTcpPacket(ether, s);
       s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
       s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + Data;
       pubCom = true;
       mqttConnStatus = true;

   }
   if(tcpState[instance] == TCP_ESTABLISHED && isMqttSubAck(ether))
   {
       //stopTimer(callBackEstablishedTimeout);
       getSocketInfoFromTcpPacket(ether, s);
       s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
       s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + Data;
       subAck = true;
       mqttConnStatus = true;
   }
   if(tcpState[instance] == TCP_ESTABLISHED && isMqttPublish(ether))
   {
       getSocketInfoFromTcpPacket(ether, s);
       s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
       s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + Data;
       pubRec = true;
       mqttConnStatus = true;
   }
   if(tcpState[instance] == TCP_ESTABLISHED && isMqttUnSubAck(ether))
   {
       //stopTimer(callBackEstablishedTimeout);
       getSocketInfoFromTcpPacket(ether, s);
       s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
       s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + Data;
       unSubAck = true;
       mqttConnStatus = true;
   }
   if(tcpState[instance] == TCP_FIN_WAIT_1 && (isTcpFin(ether) && isTcpAck(ether)))
   {
          //stopTimer(callBackEstablishedCloseTimeout);
         getSocketInfoFromTcpPacket(ether, s);
          s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
          s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + 1;
          Fin = true;
          mqttConnStatus = false;
      }
      if(tcpState[instance] == TCP_CLOSE_WAIT)
      {
          finSend = true;
          mqttConnStatus = false;
      }
      if(tcpState[instance] == TCP_LAST_ACK && (isTcpFin(ether) && isTcpAck(ether)))
      {
          getSocketInfoFromTcpPacket(ether, s);
          s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
          s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + 1;
          finAckSend = true;
          mqttConnStatus = false;
      }
      if(tcpState[instance] == TCP_CLOSING)
      {
          //getSocketInfoFromTcpPacket(ether, s);
          //s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
          //s->acknowledgementNumber = ntohl(tcp->sequenceNumber) + 1;
          ackRec = true;
          mqttConnStatus = false;
      }
}

// After sending an ARP request and receive an ARP response, I perform some checks of my own ensure that the IP and Hardware addresses
// being used are the correct ones for the broker and my own. I would then create a new socket to use for communication with broker and keep
// track of packet message information. After this, I finally set a flag to indicate I am ready to send a SYN message to start connecting to
// the broker.
void processTcpArpResponse(etherHeader *ether)
{
    arpPacket *arp = (arpPacket*)ether->data;
    socket *s;
    uint8_t i;
    uint8_t localHwAddress[HW_ADD_LENGTH];
    uint8_t ipMqttAddress[IP_ADD_LENGTH];
    // Filter out messages not from mqtt broker.
    getEtherMacAddress(localHwAddress);
    getIpMqttBrokerAddress(ipMqttAddress);
    for(i = 0; i < HW_ADD_LENGTH; i++)
    {
        if(arp->destAddress[i] != localHwAddress[i])
        {
            return;
        }
     }

    for(i = 0; i < IP_ADD_LENGTH; i++)
    {
        if(arp->sourceIp[i] != ipMqttAddress[i])
        {
             return;
        }
    }

    s = newSocket();
    getSocketInfoFromArpResponse(ether, s);
    sendSyn = true;
}

// A setter function to set the list of ports being used.
void setTcpPortList(uint16_t ports[], uint8_t count)
{
    uint8_t i;

    if(count > MAX_TCP_PORTS)
    {
        count = MAX_TCP_PORTS;
    }

    for(i = 0; i < count; i++)
    {
        tcpPorts[i] = ports[i];
    }

    tcpPortCount = count;
}

// This function checks if the destination port (the random port that I set) is open before
// proceeding with communication with the broker.
bool isTcpPortOpen(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*)((uint8_t*)ip + ipHeaderLength);

    uint8_t i;
    uint16_t destPort = ntohs(tcp->destPort); // Convert to host byte order
    //uint16_t srcPort = ntohs(tcp->sourcePort); // Convert to host byte order

    for(i = 0; i < tcpPortCount; i++)
    {
        if(destPort == tcpPorts[i])
        {
            return true;
        }
    }
    return false;
}

void sendTcpResponse(etherHeader *ether, socket* s, uint16_t flags)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint16_t tcpLengthNet;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s->remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s->remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP Header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s->localPort);
    tcp->destPort = htons(s->remotePort);
    tcp->sequenceNumber = htonl(s->sequenceNumber);
    tcp->acknowledgementNumber = htonl(s->acknowledgementNumber);
    tcp->offsetFields = htons((5 << 12) | flags & 0x3F); // Data Offset (5) & Flags
    tcp->windowSize = htons(1024); // Example window size
    tcp->checksum = 0;
    tcp->urgentPointer = 0;
    // adjust lengths
    tcpLength = sizeof(tcpHeader);
    ip->length = htons(ipHeaderLength + tcpLength);
    // 32-bit sum over ip header
    calcIpChecksum(ip);
    tcpLengthNet = htons(tcpLength);
    // 32-bit sum over pseudo-header
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sumIpWords(&tcpLengthNet, 2, &sum);
    sumIpWords(tcp, ntohs(tcpLengthNet), &sum);  // TCP header + payload. Payload is the data being processed, but no data is being handled in this function, so disregard the payload for this function
    tcp->checksum = getIpChecksum(sum);

    // send packet with size = ether + tcp hdr + ip header + tcp_size
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

// Send TCP message
void sendTcpMessage(etherHeader *ether, socket *s, uint16_t flags, uint8_t data[], uint16_t dataSize)
{
    uint8_t i;
    uint16_t j;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint16_t tcpLengthNet;
    uint8_t *copyData;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
       ether->destAddress[i] = s->remoteHwAddress[i];
       ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // IP header
    ipHeader* ip = (ipHeader*)ether->data;
    ip->rev = 0x4;
    ip->size = 0x5;
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128;
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s->remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }
    uint8_t ipHeaderLength = ip->size * 4;

    // TCP Header
    tcpHeader* tcp = (tcpHeader*)((uint8_t*)ip + (ip->size * 4));
    tcp->sourcePort = htons(s->localPort);
    tcp->destPort = htons(s->remotePort);
    tcp->sequenceNumber = htonl(s->sequenceNumber);
    tcp->acknowledgementNumber = htonl(s->acknowledgementNumber);
    tcp->offsetFields = htons((5 << 12) | (flags & 0x3F)); // Data Offset (5) & Flags
    tcp->windowSize = htons(1024); // Example window size
    tcp->checksum = 0;
    tcp->urgentPointer = 0;
    // adjust lengths
    tcpLength = sizeof(tcpHeader) + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);
    // 32-bit sum over ip header
    calcIpChecksum(ip);
    tcpLengthNet = htons(tcpLength);
    // copy data
    copyData = tcp->data;
    for (j = 0; j < dataSize; j++)
        copyData[j] = data[j];
    // 32-bit sum over pseudo-header
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    sumIpWords(&tcpLengthNet, 2, &sum);
    sumIpWords(tcp, ntohs(tcpLengthNet), &sum);  // TCP header + payload
    tcp->checksum = getIpChecksum(sum);

    // send packet with size = ether + tcp hdr + ip header + tcp_size
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);

}
