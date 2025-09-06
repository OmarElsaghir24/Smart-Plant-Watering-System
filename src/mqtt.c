// MQTT Library (includes framework only)
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
#include "mqtt.h"
#include "arp.h"
#include "eth0.h"
#include "tcp.h"
#include "timer.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Connect to MQTT Broker. I first construct the message by first setting the control header (message type),
void connectMqtt()
{
    uint8_t m = 0;
    mqttFrame *mqtt = (mqttFrame*)mqttBuff;
    mqtt->controlHdr = 0x10;           // Connect

    mqtt->data[m++] = 0x00;
    mqtt->data[m++] = 0x04;  //Protocol name length
    mqtt->data[m++] = 'M';   // Protocol Name
    mqtt->data[m++] = 'Q';
    mqtt->data[m++] = 'T';
    mqtt->data[m++] = 'T';
    mqtt->data[m++] = 0x04;   // MQTT version number
    mqtt->data[m++] = 0x02;   // Clean session
    mqtt->data[m++] = 0x00;   // QoS Exactly once
    mqtt->data[m++] = 0x3C;  // Keep-Alive time
    mqtt->data[m++] = 0x00;
    mqtt->data[m++] = 0x08;  // Length of client id
    mqtt->data[m++] = 'O';   // Entire client id
    mqtt->data[m++] = 'm';
    mqtt->data[m++] = 'a';
    mqtt->data[m++] = 'r';
    mqtt->data[m++] = '2';
    mqtt->data[m++] = '0';
    mqtt->data[m++] = '0';
    mqtt->data[m++] = '1';

    mqtt->remainingLength = m;     // Set remaining length
    mqttLength = sizeof(mqttFrame) + m;        // Length of total message
    pshNeeded = true;                         // Set flag for sending connect message
}

// Disconnect from MQTT broker. Initiated by client
void disconnectMqtt()
{
    uint8_t m = 0;
    mqttFrame *mqtt = (mqttFrame*)mqttBuff;
    mqtt->controlHdr = 0xE0;         // Disconnect message
    mqtt->remainingLength = m;       // Set remaining length
    mqttLength = sizeof(mqttFrame) + m;    // Length of total message
    closeConn = true;                // Set flag for sending disconnect message
}

// Publish messages (Includes topic and data related to topic) to broker
//void publishMqtt(char strTopic[], char strData[])
void publishMqtt(etherHeader *ether, socket *s, char strTopic[], char strData[])
{
    uint8_t m = 0;
    uint8_t i;
    mqttFrame *mqtt = (mqttFrame*)mqttBuff;
    mqtt->controlHdr = 0x30;             // Publish message
    //mqtt->controlHdr = 0x34;  // QoS 2
    uint16_t topicLength = strlen(strTopic);
    uint16_t dataLen = strlen(strData);

    // Ensure topic length is stored correctly in Big Endian format
    mqtt->variableHeader[m++] = (topicLength >> 8) & 0xFF;  // MSB first
    mqtt->variableHeader[m++] = topicLength & 0xFF;         // LSB second

    // Copy topic string
    for (i = 0; i < topicLength; i++) {
        mqtt->variableHeader[m++] = strTopic[i];
    }

    // Copy data payload
    for (i = 0; i < dataLen; i++) {
        mqtt->data[m++] = strData[i];
    }

    // Set the Remaining Length field correctly
    mqtt->remainingLength = m;

    // Set total MQTT packet length
    mqttLength = 2 + m;  // Control header (1) + Remaining Length (1) + payload

    //sendTcpMessage(ether, s, PSH | ACK, mqttBuff, mqttLength);

    publish = true;         // Set flag for sending publish message
}

// Send a message to the Broker to subscribe to a topic
void subscribeMqtt(char strTopic[])
{
    uint8_t m = 0;
    uint8_t i;
    mqttFrame *mqtt = (mqttFrame*)mqttBuff;
    uint16_t topicLen = strlen(strTopic);

    // Set Control Header for Subscribe
    mqtt->controlHdr = 0x82;

    uint16_t packetId = 1;
    mqtt->variableHeader[m++] = (packetId >> 8) & 0xFF;  // MSB
    mqtt->variableHeader[m++] = packetId & 0xFF;         // LSB

    // Ensure topic length is stored correctly in Big Endian format
    mqtt->variableHeader[m++] = (topicLen >> 8) & 0xFF;  // MSB first
    mqtt->variableHeader[m++] = topicLen & 0xFF;         // LSB second

    // Copy topic string
    for (i = 0; i < topicLen; i++) {
        mqtt->variableHeader[m++] = strTopic[i];
    }

    mqtt->variableHeader[m++] = 0x00;    // QoS 0

    // Set the Remaining Length field correctly
    mqtt->remainingLength = m;

    // Set total MQTT packet length
   mqttLength = 2 + m;  // Control header (1) + Remaining Length (1) + payload

   subscribe = true;       // Set flag for sending subscribe
}

// Unsubscribe from a topic that you're subscribed to
void unsubscribeMqtt(char strTopic[])
{
    uint8_t m = 0;
    uint8_t i;
    mqttFrame *mqtt = (mqttFrame*)mqttBuff;
    uint16_t topicLen = strlen(strTopic);

    // Set Control Header for Unsubscribe
    mqtt->controlHdr = 0xA2;

    // Set Packet Identifier (Big Endian)
    mqtt->variableHeader[m++] = 0x00;  // Packet ID MSB
    mqtt->variableHeader[m++] = 0x01;  // Packet ID LSB

    // Ensure topic length is stored correctly in Big Endian format
    mqtt->data[m++] = (topicLen >> 8) & 0xFF;  // MSB first
    mqtt->data[m++] = topicLen & 0xFF;         // LSB second

    // Copy topic string
    for (i = 0; i < topicLen; i++) {
        mqtt->data[m++] = strTopic[i];
    }

    // Set the Remaining Length field correctly
    mqtt->remainingLength = m;

    // Set total MQTT packet length
   mqttLength = 2 + m;  // Control header (1) + Remaining Length (1) + payload

   unsubscribe = true;   // Set flag for sending unsubscribe message
}

