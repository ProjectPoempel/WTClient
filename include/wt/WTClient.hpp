#pragma once

#include "WTError.hpp"

#include <wt/wtprot.h>
#include <cstdint>
#include <cfloat>

using WTPacketHandler_Disconnect = void(*)(void*, uint16_t, const char*);
using WTPacketHandler_ConfigPush = void(*)(void*, const uint8_t*, uint16_t);
using WTPacketHandler_DataACK = void(*)(void*);
using WTPacketHandler_DataFAIL = void(*)(void*);

struct WTDataset {
    uint64_t timestamp;
    float temperature = FLT_MAX;
    float humidity = FLT_MAX;
    float airPressure = FLT_MAX;
    float pm10 = FLT_MAX;
    float pm25 = FLT_MAX;
    uint16_t co2 = UINT16_MAX;
};

class WTClient {
public:
    WTClient();
    ~WTClient();

    [[nodiscard]] WTError connect(const char* host, uint16_t port, wtp_station_id_t stationId);
    void disconnect();
    bool isConnected();

    void setUserdata(void* data);
    void setPacketHandler_Disconnect(WTPacketHandler_Disconnect handler);
    void setPacketHandler_ConfigPush(WTPacketHandler_ConfigPush handler);
    void setPacketHandler_DataACK(WTPacketHandler_DataACK handler);
    void setPacketHandler_DataFAIL(WTPacketHandler_DataFAIL handler);

    void sendPacket(wtp_packet_t* packet, bool freePacket = true);
    WTError receivePackets();
    WTError receivePacket(wtp_packet_t** packet);

    void sendLiveData(const WTDataset& dataset);
private:
    class Impl;
    Impl* impl;
};