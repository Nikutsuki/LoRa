#pragma once

#include "../protocol/LinkSession.hpp"
#include "../ui/Screen.hpp"

namespace pico_ui {

/*!
 * Screen implementation that mirrors the models over the UART link.
 *
 * Rendering decisions stay on the host side: this adapter only translates the
 * screen independent model into protocol messages.
 */
class UartScreen : public ui::Screen {
public:
    UartScreen(proto::LinkSession &session, uint16_t width, uint16_t height);

    void present(const ui::ScreenModel &model) override;

    void sendHello();

    /*!
     * False when the last frame could not be encoded, which means the host is
     * showing stale content and the app should surface a warning.
     */
    bool lastFrameSent() const;

private:
    static proto::ComponentMessage toMessage(const ui::ComponentView &view);

    proto::LinkSession &session_;
    uint16_t width_;
    uint16_t height_;
    bool lastFrameSent_ = true;
};

}
