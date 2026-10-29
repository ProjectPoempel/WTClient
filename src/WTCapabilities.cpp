#include "wt/WTCapabilities.hpp"

#include <string.h>

WTCapabilities::WTCapabilities() : entriesCount(0) {
    packet = wtp_packet_create(WTP_PACKET_CAPABILITIES);
    wtp_packet_write_u16(packet, 0);
}

WTCapabilities::~WTCapabilities() {
    if (packet) wtp_packet_free(packet);
}

void WTCapabilities::addSensor(uint8_t id, const char* name) {
    entriesCount++;

    uint16_t len = strlen(name);

    wtp_packet_write_u8(packet, WTP_CT_SENSOR);
    wtp_packet_write_u16(packet, strlen(name) + 2);
    wtp_packet_write_u8(packet, id);
    wtp_packet_write_u8(packet, len);
    wtp_packet_write_string(packet, name, len);
}

void WTCapabilities::addFirmwareVersion(const char* version) {
    entriesCount++;

    uint16_t len = strlen(version);

    wtp_packet_write_u8(packet, WTP_CT_FIRMWARE_INFO);
    wtp_packet_write_u16(packet, strlen(version) + 1);
    wtp_packet_write_u8(packet, len);
    wtp_packet_write_string(packet, version, len);
}

void WTCapabilities::addBatteryInfo(uint32_t capacityMilliamps) {
    entriesCount++;

    wtp_packet_write_u8(packet, WTP_CT_BATTERY_INFO);
    wtp_packet_write_u16(packet, 4);
    wtp_packet_write_u32(packet, capacityMilliamps);
}

wtp_packet_t* WTCapabilities::buildPacket() {
    wtp_packet_t* pkt = wtp_packet_copy(packet);

    uint32_t oldOffset = pkt->offset;
    pkt->offset = 4;
    wtp_packet_write_u16(pkt, entriesCount);
    pkt->offset = oldOffset;

    return pkt;
}
