#pragma once

#include <cstddef>
#include <cstdint>

#include "CardKbKeyboard.hpp"
#include "Ili9341Display.hpp"
#include "Types.hpp"
#include "adapters/CardKbInput.hpp"
#include "adapters/UartInput.hpp"
#include "adapters/UartScreen.hpp"
#include "protocol/LinkSession.hpp"
#include "ui/Input.hpp"
#include "ui/LcdScreen.hpp"
#include "ui/Screen.hpp"
#include "ui/Ui.hpp"

class FileTransfer;
class RadioSession;

/*!
 * Command emitted by the UI state machine when the user triggers an action.
 */
struct UiCommand {
    enum class Type {
        None,
        SendText,
        SendInvite,
        AcceptInvite,
        SelectContact,
        NextContact,
        PrevContact,
        SetNickname,
        ProvisionSeed,
        CancelFileTransfer,
    };

    Type type = Type::None;
    size_t contactIndex = 0;
    const char *text = nullptr;
};

/*!
 * Manages the display, input source, pages, focus navigation, and frame diffing.
 */
class UiController {
public:
    static constexpr uint16_t ScreenWidth = 320;
    static constexpr uint16_t ScreenHeight = 240;

    UiController(Ili9341Display &display,
                 CardKbKeyboard &keyboard,
                 proto::LinkSession &session);

    void init(bool hasCardKb);
    void invalidate();
    void requestFullRedraw();
    void requestUartResync();

    void setNotice(const char *message, ui::Tone tone);
    const char *notice() const { return notice_; }

    bool pollInput(const RadioSession &radio,
                   const FileTransfer &fileTransfer,
                   UiCommand &outCommand);

    void render(const RadioSession &radio, const FileTransfer &fileTransfer);

private:
    static constexpr size_t InputMax = lora::MaxPlainTextSize;
    static constexpr size_t VisibleHistoryRows = 6;
    static constexpr size_t VisibleContacts = 4;
    static constexpr size_t WrapColumnWidth = 36;
    static constexpr int ProgressBarWidth = 20;

    enum class Page {
        Chat,
        Contacts,
        Invite,
        Identity,
    };

    void setPage(Page page);
    void clearInput();
    void preparePage(const RadioSession &radio, const FileTransfer &fileTransfer);
    void prepareCommon(const RadioSession &radio);
    void prepareFilePopup(const FileTransfer &fileTransfer);
    void prepareChatPage(const RadioSession &radio);
    void prepareContactsPage(const RadioSession &radio);
    void prepareInvitePage(const RadioSession &radio);
    void prepareIdentityPage(const RadioSession &radio);
    void rebuildHistoryRows(const RadioSession &radio);
    void updateFileProgress(uint32_t current, uint32_t total);

    bool handleKeyEvent(ui::KeyEvent event,
                        const RadioSession &radio,
                        const FileTransfer &fileTransfer,
                        UiCommand &outCommand);
    bool activateFocused(const RadioSession &radio, UiCommand &outCommand);
    bool parseSlashCommand(UiCommand &outCommand);
    bool emitTextCommand(UiCommand::Type type,
                         const char *source,
                         bool clear,
                         UiCommand &outCommand);
    static void formatFingerprint(const uint8_t fingerprint[lora::FingerprintSize],
                                  char *out,
                                  size_t outSize);

    proto::LinkSession &session_;
    ui::Theme theme_{};

    ui::LcdScreen lcdScreen_;
    pico_ui::UartScreen uartScreen_;
    pico_ui::CardKbInput cardKbInput_;
    pico_ui::UartInput uartInput_;

    ui::Screen *screen_ = nullptr;
    ui::Input *keySource_ = nullptr;

    Page page_ = Page::Chat;
    bool uiDirty_ = true;
    bool fullRedraw_ = true;
    bool uartResyncNeeded_ = false;
    bool filePopupWasActive_ = false;

    char input_[InputMax]{};
    char commandText_[InputMax]{};
    size_t inputLength_ = 0;

    char notice_[64]{};
    char headerText_[48]{};
    char headerRight_[24]{};
    char pageTitle_[32]{};
    char contactLine_[64]{};
    char contactMeta_[32]{};
    char inviteLine_[64]{};
    char inviteMeta_[64]{};
    char identityLine_[64]{};
    char identityMeta_[32]{};
    char contactPrimary_[4][32]{};
    char contactSecondary_[4][32]{};
    char historyRows_[VisibleHistoryRows][80]{};
    char inputHint_[InputMax + 2]{};
    char filePopupTitle_[48]{};
    char fileProgressBarText_[36]{};
    char fileProgressStats_[36]{};

    ui::Page uiPage_{};
    ui::ScreenModel lastModel_{};

    ui::HeaderBar headerBar_{{0, 0, ScreenWidth, 18}, headerText_, headerRight_};
    ui::Divider headerDivider_{{0, 20, ScreenWidth, 1}};
    ui::Label pageTitleLabel_{{8, 24, 304, 10}, pageTitle_, ui::Tone::Accent, 1};
    ui::Label primaryLabel_{{8, 36, 304, 10}, contactLine_, ui::Tone::Accent, 1};
    ui::Label secondaryLabel_{{8, 48, 304, 10}, contactMeta_, ui::Tone::Muted, 1};

    ui::Label historyLabels_[VisibleHistoryRows] = {
        ui::Label{{8, 76,  304, 11}, historyRows_[0], ui::Tone::Default, 1},
        ui::Label{{8, 88,  304, 11}, historyRows_[1], ui::Tone::Default, 1},
        ui::Label{{8, 100, 304, 11}, historyRows_[2], ui::Tone::Default, 1},
        ui::Label{{8, 112, 304, 11}, historyRows_[3], ui::Tone::Default, 1},
        ui::Label{{8, 124, 304, 11}, historyRows_[4], ui::Tone::Default, 1},
        ui::Label{{8, 136, 304, 11}, historyRows_[5], ui::Tone::Default, 1},
    };

    ui::TextBox inputBox_{{8, 152, 304, 26}, inputHint_};
    ui::ListItem contactItems_[4] = {
        ui::ListItem{{8, 52, 304, 28}, contactPrimary_[0], contactSecondary_[0], false},
        ui::ListItem{{8, 82, 304, 28}, contactPrimary_[1], contactSecondary_[1], false},
        ui::ListItem{{8, 112, 304, 28}, contactPrimary_[2], contactSecondary_[2], false},
        ui::ListItem{{8, 142, 304, 28}, contactPrimary_[3], contactSecondary_[3], false},
    };
    ui::Label inviteDetailLabel_{{8, 64, 304, 40}, inviteMeta_, ui::Tone::Muted, 1};
    ui::Label identityDetailLabel_{{8, 64, 304, 40}, identityMeta_, ui::Tone::Muted, 1};

    ui::Button sendButton_{{8, 184, 72, 26}, "Send"};
    ui::Button contactsButton_{{88, 184, 76, 26}, "Contacts"};
    ui::Button inviteButton_{{172, 184, 68, 26}, "Invite"};
    ui::Button identityButton_{{248, 184, 64, 26}, "ID"};
    ui::Button selectButton_{{8, 184, 72, 26}, "Select"};
    ui::Button nextButton_{{88, 184, 68, 26}, "Next"};
    ui::Button backButton_{{248, 184, 64, 26}, "Back"};
    ui::Button broadcastButton_{{8, 184, 88, 26}, "Broadcast"};
    ui::Button acceptButton_{{104, 184, 76, 26}, "Accept"};
    ui::Button setNickButton_{{8, 184, 76, 26}, "Set Nick"};
    ui::Button seedButton_{{92, 184, 60, 26}, "Seed"};

    ui::StatusBar statusBar_{{0, 216, ScreenWidth, 22}, notice_, ui::Tone::Muted};

    ui::Label filePopupTitleLabel_{{40, 55, 240, 20}, filePopupTitle_, ui::Tone::Accent, 1};
    ui::Label fileProgressBarLabel_{{40, 85, 240, 20}, fileProgressBarText_, ui::Tone::Focus, 1};
    ui::Label fileProgressStatsLabel_{{40, 115, 240, 20}, fileProgressStats_, ui::Tone::Muted, 1};
    ui::Button fileCancelButton_{{110, 145, 100, 26}, "Cancel"};
};
