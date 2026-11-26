#pragma once

#include <cstdint>

#include "protocol/LinkSession.hpp"
#include "protocol/UsbCdcTransport.hpp"

/*!
 * Manages the USB CDC host connection state machine.
 *
 * Owns the underlying transport and frame session, announces screen geometry
 * whenever the host opens the CDC port, and surfaces host resync requests.
 */
class HostLink {
public:
    HostLink(uint16_t screenWidth, uint16_t screenHeight);

    void init();
    void poll();

    bool consumeResync();
    void sendNotice(const char *text, proto::NoticeLevel level);

    proto::LinkSession &session();
    const proto::LinkSession &session() const;

private:
    proto::UsbCdcTransport transport_;
    proto::LinkSession session_;
    uint16_t width_;
    uint16_t height_;
    bool geometryAnnounced_ = false;
    bool resyncNeeded_ = false;
};
