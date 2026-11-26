#include "HostLink.hpp"

HostLink::HostLink(uint16_t screenWidth, uint16_t screenHeight)
    : session_(transport_),
      width_(screenWidth),
      height_(screenHeight)
{
}

void HostLink::init()
{
    session_.init();
}

void HostLink::poll()
{
    session_.poll();

    if (session_.consumeResyncRequest()) {
        resyncNeeded_ = true;
    }

    // The host opens the USB CDC port some time after boot, so announce once
    // it appears and again if it reconnects.
    const bool connected = transport_.connected();
    if (connected && !geometryAnnounced_) {
        session_.sendHello(width_, height_);
        geometryAnnounced_ = true;
    } else if (!connected) {
        geometryAnnounced_ = false;
    }
}

bool HostLink::consumeResync()
{
    const bool needed = resyncNeeded_;
    resyncNeeded_ = false;
    return needed;
}

void HostLink::sendNotice(const char *text, proto::NoticeLevel level)
{
    if (text == nullptr) {
        return;
    }
    proto::NoticeMessage notice;
    notice.level = level;
    notice.text = text;
    session_.sendNotice(notice);
}

proto::LinkSession &HostLink::session()
{
    return session_;
}

const proto::LinkSession &HostLink::session() const
{
    return session_;
}
