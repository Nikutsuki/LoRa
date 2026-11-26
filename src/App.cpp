#include "App.hpp"

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace {

constexpr uint TftCs = 5;
constexpr uint TftSck = 6;
constexpr uint TftMosi = 7;
constexpr uint TftDc = 8;
constexpr uint TftReset = 9;
constexpr uint8_t TftMadctlValue = 0xA0;
constexpr uint TftSpiBaud = 30000000u;

constexpr uint8_t CardKbAddress = 0x5F;
constexpr uint CardKbSda = 26;
constexpr uint CardKbScl = 27;
constexpr uint CardKbBaud = 100000;

constexpr uint32_t BootDelayMs = 1000;
constexpr uint32_t LoopIntervalMs = 20;

} // namespace

App::App()
    : display_({spi0, TftSpiBaud, UiController::ScreenWidth, UiController::ScreenHeight,
                TftCs, TftSck, TftMosi, TftDc, TftReset, TftMadctlValue}),
      keyboard_({i2c1, CardKbAddress, CardKbSda, CardKbScl, CardKbBaud}),
      hostLink_(UiController::ScreenWidth, UiController::ScreenHeight),
      ui_(display_, keyboard_, hostLink_.session()),
      radio_(NoticeSink{&App::onNotice, this}),
      fileTransfer_(radio_, hostLink_.session(), NoticeSink{&App::onNotice, this})
{
}

void App::run()
{
    stdio_init_all();
    sleep_ms(BootDelayMs);

    display_.init();
    hostLink_.init();
    handleNotice("UART control ready", ui::Tone::Focus);

    keyboard_.init();
    ui_.init(keyboard_.probe());

    radio_.init();

    while (true) {
        hostLink_.poll();
        if (hostLink_.consumeResync()) {
            ui_.requestUartResync();
        }

        radio_.poll(&fileTransfer_);
        fileTransfer_.poll();

        if (radio_.consumeDirty() || fileTransfer_.consumeDirty()) {
            ui_.invalidate();
        }

        pollUiCommands();
        ui_.render(radio_, fileTransfer_);
        sleep_ms(LoopIntervalMs);
    }
}

void App::onNotice(void *ctx, const char *message, ui::Tone tone)
{
    static_cast<App *>(ctx)->handleNotice(message, tone);
}

void App::handleNotice(const char *message, ui::Tone tone)
{
    ui_.setNotice(message, tone);

    const proto::NoticeLevel level =
        tone == ui::Tone::Danger    ? proto::NoticeLevel::Danger
        : tone == ui::Tone::Success ? proto::NoticeLevel::Success
                                    : proto::NoticeLevel::Info;
    hostLink_.sendNotice(message, level);
}

void App::pollUiCommands()
{
    UiCommand cmd;
    while (ui_.pollInput(radio_, fileTransfer_, cmd)) {
        dispatchCommand(cmd);
    }
}

void App::dispatchCommand(const UiCommand &cmd)
{
    switch (cmd.type) {
    case UiCommand::Type::SendText:
        radio_.sendTextMessage(cmd.text);
        break;
    case UiCommand::Type::SendInvite:
        radio_.sendInvite(cmd.text);
        break;
    case UiCommand::Type::AcceptInvite:
        radio_.acceptPendingInvite(cmd.text);
        break;
    case UiCommand::Type::SelectContact:
        radio_.selectContact(cmd.contactIndex);
        break;
    case UiCommand::Type::NextContact:
        radio_.selectNextContact();
        break;
    case UiCommand::Type::SetNickname:
        radio_.setNickname(cmd.text);
        break;
    case UiCommand::Type::ProvisionSeed:
        radio_.provisionSeed(cmd.text);
        break;
    case UiCommand::Type::CancelFileTransfer:
        fileTransfer_.cancel(true);
        break;
    case UiCommand::Type::None:
        break;
    }
}
