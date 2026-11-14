#pragma once

#include "../protocol/LinkSession.hpp"
#include "../ui/Input.hpp"

namespace pico_ui {

/*!
 * Input adapter that surfaces keys received over the UART link.
 */
class UartInput : public ui::Input {
public:
    explicit UartInput(proto::LinkSession &session);

    bool poll(ui::KeyEvent &event) override;

private:
    static ui::Key toKey(proto::KeyCode code);

    proto::LinkSession &session_;
};

}
