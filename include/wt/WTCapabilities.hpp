#pragma once

#include <wt/wtprot.h>
#include <cstdint>

class WTCapabilities {
public:
    WTCapabilities();
    ~WTCapabilities();

    void addSensor(uint8_t id, const char* name);
    void addFirmwareVersion(const char* version);
    void addBatteryInfo(uint32_t capacityMilliamps);

    wtp_packet_t* buildPacket();
private:
    wtp_packet_t* packet;
    uint16_t entriesCount;
};