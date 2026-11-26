#pragma once

#include "CardKbKeyboard.hpp"
#include "FileTransfer.hpp"
#include "HostLink.hpp"
#include "Ili9341Display.hpp"
#include "RadioSession.hpp"
#include "UiController.hpp"

/*!
 * Top-level application coordinator.
 *
 * Wires together hardware drivers and drives the four state machines:
 * USB host link, LoRa radio session, file transfer, and UI controller.
 */
class App {
public:
    App();

    void run();

private:
    static void onNotice(void *ctx, const char *message, ui::Tone tone);
    void handleNotice(const char *message, ui::Tone tone);
    void pollUiCommands();
    void dispatchCommand(const UiCommand &cmd);

    Ili9341Display display_;
    CardKbKeyboard keyboard_;
    HostLink hostLink_;
    UiController ui_;
    RadioSession radio_;
    FileTransfer fileTransfer_;
};
