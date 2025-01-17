#include "HardwareSerial.h"
/*
  LoRaNet.cpp
  Created by Bruno Carneiro
  Released into the public domain
*/

#include "LoRaNet.h"
#include "util/eeprom.h"
#include "util/headers.h"

static uint8_t          thisNodeAddr[4];
// TODO: this is not thread-safe
static genericFrame     snd;
static genericFrame     rcv;

static void (*_onReceive)(int, IPAddress);

LoRaNetClass::LoRaNetClass() :
  _onReceive(NULL)
{
    ;
}

void 
LoRaNetClass::onReceive(void(*callback)(int, IPAddress))
{
  _onReceive = callback;
}

void
LoRaNetClass::handlePacket(int packetSize)
{
    if (packetSize < sizeof(networkHeader)) {
        // Serial.printf("LoRaNet::handlePacket: Packet is too short!\n");
        return;
    }

    // TODO: check remaining packet size
    // Serial.printf("LoRaNet::handlePacket: Data message received\n");
    for (int i = 0; i < sizeof(rcv.nh); i++) 
        *(((uint8_t*) &rcv.nh) + i) = (uint8_t) LoRa.read();
    // Serial.printf("LoRaNet::handlePacket: Network layer header parsed successfully\n");

    if (IPAddress(rcv.nh.dstAddr) == IPAddress(thisNodeAddr)) {
        if (_onReceive) 
            _onReceive(packetSize - sizeof(rcv.nh), IPAddress(rcv.nh.srcAddr));
    }
    else {
        // Serial.printf("LoRaNet::handlePacket: Message addressed to another node, forwarding...\n");
        for (int i = 0; i < rcv.nh.len; i++)
            *(((uint8_t*) &rcv.payload) + i) = (uint8_t) LoRa.read();
        // TODO: check if remaining packetsize is good 
        
        /*********************************
         * PAREI AQUI
         *  FAZER O DECRESCIMO DO TTL
         *  SE NAO HOUVER => RERR
        *********************************/
        LoRaNetSwitch.push(rcv, DATA); 
    }
}

// int
void
LoRaNetClass::begin(long frequency)
{
    // TODO: return SUCCESS or FAIL
    // LoRa radio setup
    SPI.begin(SCK,MISO,MOSI,SS);
    // LoRa.setPins(SS,RST_LoRa,DIO0);
    LoRa.setPins(SS,23,26);

    // TODO: PADBOOST?
    if (!LoRa.begin(frequency, true)) {
        Serial.printf("LoRaNet::begin: LoRa startup failed!\n");
        // goto lmFail;
    }
    else {
        Serial.printf("LoRaNet::begin: LoRa startup successful.\n");
    }

    // Address recovery
    if (!EEPROM.begin(EEPROM_SIZE)) {
        Serial.printf("LoRaNet::begin: EEPROM initialization failed!\n");
        // goto lmFail;
    }
    else
        Serial.printf("LoRaNet::begin: EEPROM initialized!\n");

    if (eepromConfigured()) eepromRead(thisNodeAddr);
    else eepromSetup(thisNodeAddr);
    for (int i=0; i<4; i++){
      Serial.printf("%u.", thisNodeAddr[i]);
    }
    Serial.printf("\n");
    LoRaNetSwitch.begin(thisNodeAddr);
    LoRaNetRouter.begin(thisNodeAddr);

    // lmFail:
    //     return FAIL;
}

void
LoRaNetClass::beginPacket(const char *host)
{
    // TODO: error checking
    // TODO: input sanitization
    snd.nh.version = 0;
    snd.nh.len = 0;
    snd.nh.ttl = INITIAL_TTL;

    IPAddress dst;
    dst.fromString(host);
    uint8_t dstAddr[4] = {dst[0], dst[1], dst[2], dst[3]};
    memcpy(snd.nh.srcAddr, thisNodeAddr, 4);
    memcpy(snd.nh.dstAddr, dstAddr, 4);
}

size_t 
LoRaNetClass::write(const uint8_t *buffer, size_t size)
{
    // TODO: error checking
    // TODO: return an informative error
    if (snd.nh.len + size > PAYLOAD_MAX_LEN) return FAIL;

    memcpy(snd.payload + snd.nh.len, buffer, size);
    snd.nh.len += size;
    Serial.println("LoRaNetClass::write finished");
    return size;
}

void
LoRaNetClass::endPacket()
{
    // TODO: error checking
    // TODO: use mutex below
    Serial.println("LoRaNetClass::endPacket begin");
    LoRaNetSwitch.push(snd, DATA); 
    Serial.println("LoRaNetClass::endPacket finished");
}

void
LoRaNetClass::run()
{
    LoRaNetSwitch.run(); 
    LoRaNetRouter.run();
}


LoRaNetClass LoRaNet;