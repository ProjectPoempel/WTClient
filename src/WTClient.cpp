#include "wt/WTClient.hpp"

#include <wt/wtprot.h>
#include <WiFiClient.h>
#include <stdlib.h>

class WTClient::Impl {
public:
    WiFiClient client;

    void* userdata = nullptr;
    WTPacketHandler_Disconnect disconnectHandler = nullptr;
    WTPacketHandler_ConfigPush configPushHandler = nullptr;
    WTPacketHandler_DataACK dataACKHandler = nullptr;
    WTPacketHandler_DataFAIL dataFAILHandler = nullptr;

    WTError connect(const char* host, uint16_t port, wtp_station_id_t stationId) {
        if (client.connected()) return WT_ERROR_ALREADY_CONNECTED;

        if (!client.connect(host, port)) return WT_ERROR_CONNECT;

        wtp_packet_t* helloPacket = wtp_packet_create(WTP_PACKET_HELLO);
        wtp_packet_write_u16(helloPacket, stationId);
        wtp_packet_finalize(helloPacket);
        sendPacket(helloPacket);
        wtp_packet_free(helloPacket);

        wtp_packet_t* respPacket;
        WTError err;
        if ((err = doReceivePacket(&respPacket, true)) != WT_SUCCESS) {
            client.stop();
            return err;
        }
        
        uint8_t protVersion;
        if (wtp_packet_read_u8(respPacket, &protVersion)) {
            wtp_packet_free(respPacket);
            client.stop();
            return WT_ERROR_PACKET_DATA_MISSING;
        }

        if (protVersion != WTP_VERSION_CURRENT) {
            wtp_packet_free(respPacket);
            client.stop();
            return WT_ERROR_PROTOCOL_MISMATCH;
        }

        uint8_t pktId;
        if (wtp_packet_read_u8(respPacket, &pktId)) {
            wtp_packet_free(respPacket);
            client.stop();
            return WT_ERROR_PACKET_DATA_MISSING;
        }
        respPacket->offset += 2;

        switch (pktId)
        {
            case WTP_PACKET_HELLO: {
                uint16_t id;
                if (wtp_packet_read_u16(respPacket, &id)) {
                    wtp_packet_free(respPacket);
                    client.stop();
                    return WT_ERROR_PACKET_DATA_MISSING;
                }

                if (id != stationId) {
                    wtp_packet_free(respPacket);
                    client.stop();
                    return WT_ERROR_HANDSHAKE;
                }

                break;
            }
            default: {
                wtp_packet_free(respPacket);
                client.stop();
                return WT_ERROR_PROTOCOL_VIOLATION;
            }
        }
        
        wtp_packet_free(respPacket);
        return WT_SUCCESS;
    }

    void disconnect() {
        client.stop();
    }

    void sendPacket(wtp_packet_t* packet) {
        if (!client.connected()) return;
        size_t written = client.write(packet->buf, packet->bufSize);
    }

    WTError doReceivePacket(wtp_packet_t** packet, bool wait) {
        uint8_t buf[1024] = { 0 };

        if (wait) {
            unsigned long start = millis();
            while (client.available() < WTP_HEADER_SIZE) {
                unsigned long now = millis();
                if (now - start >= 15000) return WT_ERROR_TIMEOUT;
                delay(1);
            }
        } else if (client.available() < WTP_HEADER_SIZE) {
            return WT_WAIT;
        }

        client.read(buf, WTP_HEADER_SIZE);
        uint16_t packetLen = wtp_get_payload_length_from_header(buf, WTP_HEADER_SIZE);

        unsigned long start = millis();
        while (client.available() < packetLen) {
            unsigned long now = millis();
            if (now - start >= 5000) return WT_ERROR_TIMEOUT;
            delay(1);
        }

        client.read(&buf[WTP_HEADER_SIZE], packetLen);

        *packet = wtp_packet_create_from_buffer(buf, packetLen + WTP_HEADER_SIZE);
        return WT_SUCCESS;
    }

    WTError receivePacket(bool wait = false) {
        if (!client.connected()) return WT_ERROR_NOT_CONNECTED;
        wtp_packet_t* packet;
        
        WTError err;
        if ((err = doReceivePacket(&packet, wait)) != WT_SUCCESS) return err;

        uint8_t protVersion;
        if (wtp_packet_read_u8(packet, &protVersion)) {
            wtp_packet_free(packet);
            return WT_ERROR_PACKET_DATA_MISSING;
        }

        if (protVersion != WTP_VERSION_CURRENT) {
            wtp_packet_free(packet);
            return WT_ERROR_PROTOCOL_MISMATCH;
        }

        uint8_t pktId;
        if (wtp_packet_read_u8(packet, &pktId)) {
            wtp_packet_free(packet);
            return WT_ERROR_PACKET_DATA_MISSING;
        }
        packet->offset += 2;

        switch (pktId)
        {
            case WTP_PACKET_PING: {
                wtp_packet_t* pkt = wtp_packet_create(WTP_PACKET_PONG);
                wtp_packet_finalize(pkt);
                sendPacket(pkt);
                wtp_packet_free(pkt);
                break;
            }
            case WTP_PACKET_DISCONNECT: {
                if ((err = handleDisconnectPacket(packet)) != WT_SUCCESS) {
                    wtp_packet_free(packet);
                    return err;
                }

                break;
            }
            case WTP_PACKET_CONFIG_PUSH: {
                if ((err = handleConfigPushPacket(packet)) != WT_SUCCESS) {
                    wtp_packet_free(packet);
                    return err;
                }

                break;
            }
            case WTP_PACKET_DATA_ACK: {
                if (dataACKHandler) {
                    dataACKHandler(userdata);
                }

                break;
            }
            case WTP_PACKET_DATA_FAIL: {
                if (dataFAILHandler) {
                    dataFAILHandler(userdata);
                }
                
                break;
            }
            default: {
                wtp_packet_free(packet);
                return WT_ERROR_PROTOCOL_VIOLATION;
            }
        }

        wtp_packet_free(packet);
        return WT_SUCCESS;
    }

    void setUserdata(void* data) {
        userdata = data;
    }

    WTError handleDisconnectPacket(wtp_packet_t* packet) {
        disconnect();

        uint16_t bitmask;
        if (wtp_packet_read_u16(packet, &bitmask)) {
            wtp_packet_free(packet);
            return WT_ERROR_PACKET_DATA_MISSING;
        }
        uint8_t messageLen;
        if (wtp_packet_read_u8(packet, &messageLen)) {
            wtp_packet_free(packet);
            return WT_ERROR_PACKET_DATA_MISSING;
        }

        char* msg = nullptr;
        if (messageLen > 0) {
            msg = wtp_packet_read_string(packet, messageLen);
        }

        if (disconnectHandler) {
            disconnectHandler(userdata, bitmask, msg);
        }

        if (msg) wtp_free_string(msg);
        return WT_SUCCESS;
    }

    WTError handleConfigPushPacket(wtp_packet_t* packet) {
        uint16_t size;
        if (wtp_packet_read_u16(packet, &size)) {
            wtp_packet_free(packet);
            return WT_ERROR_PACKET_DATA_MISSING;
        }

        if (configPushHandler) {
            configPushHandler(userdata, &packet->buf[packet->offset], size);
        }

        return WT_SUCCESS;
    }
};

WTClient::WTClient() {
    impl = new Impl;
}

WTClient::~WTClient() {
    delete impl;
}

WTError WTClient::connect(const char* host, uint16_t port, wtp_station_id_t stationId) {
    return impl->connect(host, port, stationId);
}

void WTClient::disconnect() {
    impl->disconnect();
}

bool WTClient::isConnected() {
    return impl->client.connected();
}

void WTClient::setUserdata(void* data) {
    impl->setUserdata(data);
}

void WTClient::setPacketHandler_Disconnect(WTPacketHandler_Disconnect handler) {
    impl->disconnectHandler = handler;
}

void WTClient::setPacketHandler_ConfigPush(WTPacketHandler_ConfigPush handler) {
    impl->configPushHandler = handler;
}

void WTClient::setPacketHandler_DataACK(WTPacketHandler_DataACK handler) {
    impl->dataACKHandler = handler;
}

void WTClient::setPacketHandler_DataFAIL(WTPacketHandler_DataFAIL handler) {
    impl->dataFAILHandler = handler;
}

void WTClient::sendPacket(wtp_packet_t* packet, bool freePacket) {
    wtp_packet_finalize(packet);

    impl->sendPacket(packet);

    if (freePacket) wtp_packet_free(packet);
}

WTError WTClient::receivePackets() {
    return impl->receivePacket();
}

WTError WTClient::receivePacket(wtp_packet_t** packet) {
    return impl->doReceivePacket(packet, true);
}

void WTClient::sendLiveData(const WTDataset& dataset) {
    wtp_packet_t* pkt = wtp_packet_create(WTP_PACKET_LIVE_DATA);
    wtp_packet_write_u64(pkt, dataset.timestamp);
    wtp_packet_write_u32(pkt, 0); // Temporary

    uint32_t bitmask = 0;

    if (dataset.temperature != FLT_MAX) {
        bitmask |= WTP_VBIT_TEMPERATURE;
        wtp_packet_write_s16(pkt, static_cast<int16_t>(dataset.temperature * 100.0f));
    }
    if (dataset.humidity != FLT_MAX) {
        bitmask |= WTP_VBIT_HUMIDITY;
        wtp_packet_write_u16(pkt, static_cast<uint16_t>(dataset.humidity * 100.0f));
    }
    if (dataset.airPressure != FLT_MAX) {
        bitmask |= WTP_VBIT_AIR_PRESSURE;
        wtp_packet_write_float(pkt, dataset.airPressure);
    }
    if (dataset.pm10 != FLT_MAX) {
        bitmask |= WTP_VBIT_PM10;
        wtp_packet_write_u16(pkt, static_cast<uint16_t>(dataset.pm10 * 100.0f));
    }
    if (dataset.pm25 != FLT_MAX) {
        bitmask |= WTP_VBIT_PM25;
        wtp_packet_write_u16(pkt, static_cast<uint16_t>(dataset.pm25 * 100.0f));
    }
    if (dataset.co2 != UINT16_MAX) {
        bitmask |= WTP_VBIT_CO2;
        wtp_packet_write_u16(pkt, static_cast<uint16_t>(dataset.co2));
    }

    uint32_t endOffset = pkt->offset;
    pkt->offset = WTP_HEADER_SIZE + 8;
    wtp_packet_write_u32(pkt, bitmask);
    pkt->offset = endOffset;

    sendPacket(pkt);
}
