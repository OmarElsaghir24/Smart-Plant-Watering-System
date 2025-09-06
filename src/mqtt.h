// MQTT Library (includes framework only)
// Jason Losh

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

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"
#include "socket.h"
#include "eth0.h"

uint8_t mqttBuff[512];
uint8_t mqttLength;
bool pshNeeded;
bool closeConn;
bool publish;
bool subscribe;
bool unsubscribe;
bool sendPing;
char mqttTopic[8];
char mqttData[3];

typedef struct _mqttFrame
{
    uint8_t controlHdr;
    uint8_t remainingLength;
    uint8_t variableHeader[0];
    uint8_t data[0];
}mqttFrame;

// MQTT Packet Types
#define CONNECT 0x10
#define CONNACK 2
#define PUBLISH 3
#define PUBACK 4
#define PUBREC 5
#define PUBREL 6
#define PUBCOMP 7
#define SUBSCRIBE 8
#define SUBACK 9
#define UNSUBSCRIBE 10
#define UNSUBACK 11
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 0xE0

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void connectMqtt();
void disconnectMqtt();
//void publishMqtt(char strTopic[], char strData[]);
void publishMqtt(etherHeader *ether, socket *s, char strTopic[], char strData[]);
void subscribeMqtt(char strTopic[]);
void unsubscribeMqtt(char strTopic[]);

#endif

