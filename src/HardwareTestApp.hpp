#pragma once

#include <cstddef>
#include <cstdint>

#include "CardKbKeyboard.hpp"
#include "ChatHistory.hpp"
#include "ContactStore.hpp"
#include "CryptoTypes.hpp"
#include "Ili9341Display.hpp"
#include "LoraMessenger.hpp"
#include "adapters/CardKbInput.hpp"
#include "adapters/UartInput.hpp"
#include "adapters/UartScreen.hpp"
#include "protocol/LinkSession.hpp"
#include "protocol/UsbCdcTransport.hpp"
#include "ui/Input.hpp"
#include "ui/LcdScreen.hpp"
#include "ui/Screen.hpp"
#include "ui/Ui.hpp"
#include "pico/time.h"

/*!
 * Application shell.
 *
 * The app is transport agnostic: it describes screens through ui::ScreenModel
 * and reads keys through ui::Input. Which implementation backs those two
 * interfaces is decided once at boot.
 *
 * The on-screen layout deliberately mirrors the desktop TUI so that a photo of
 * the device and a terminal screenshot are recognisably the same product: a
 * header bar, a tab strip, a scrolling history, an input row and a button bar.
 */
class HardwareTestApp {
public:
    HardwareTestApp();

    void run();

private:
    static constexpr size_t InputMax = crypto_chat::MaxPlainTextSize;
    static constexpr size_t VisibleHistoryRows = 6;

    enum class Page {
        Chat,
        Contacts,
        Invite,
        Identity,
        Count,
    };

    enum class Action {
        None,
        Send,
        OpenContacts,
        OpenInvite,
        OpenIdentity,
        SelectContact,
        NextContact,
        BackToChat,
        BroadcastInvite,
        AcceptInvite,
        SetNickname,
        ProvisionSeed,
    };

    void initializeDisplay();
    bool initializeKeyboard();
    void initializeUartSurface();
    void announceGeometry();
    void initializeIdentity();

    void render();
    void invalidate();
    void setPage(Page page);
    void preparePage();
    void prepareCommon();
    void prepareChatPage();
    void prepareContactsPage();
    void prepareInvitePage();
    void prepareIdentityPage();
    void rebuildHistoryRows();
    void setNotice(const char *message, ui::Tone tone);
    void handleKeyEvent(ui::KeyEvent event);
    void activateAction(Action action);
    void handleCommand();
    void sendCurrentInput();
    void sendInvite();
    void acceptPendingInvite();
    void selectNextContact();
    void setNickname(const char *nickname);
    void provisionSeed(const char *hexSeed);
    void pollRadio();
    void handleReceivedPacket();
    void pollInput();
    const crypto_chat::Contact *selectedContact() const;
    void fingerprintText(const uint8_t fingerprint[crypto_chat::FingerprintSize], char *out, size_t outSize) const;
    bool parseHexSeed(const char *hexSeed, uint8_t out[32]) const;
    Action actionForFocusedComponent() const;
    size_t focusIndexForAction(Action action);

    Ili9341Display display_;
    CardKbKeyboard keyboard_;
    proto::UsbCdcTransport usbLink_;
    proto::LinkSession session_;
    ui::Theme theme_{};

    ui::LcdScreen lcdScreen_;
    pico_ui::UartScreen uartScreen_;
    pico_ui::CardKbInput cardKbInput_;
    pico_ui::UartInput uartInput_;

    ui::Screen *screen_ = nullptr;
    ui::Input *keySource_ = nullptr;

    crypto_chat::ContactStore contacts_;
    crypto_chat::LoraMessenger radio_;
    crypto_chat::ChatHistory history_;
    crypto_chat::Contact pendingInvite_{};

    char input_[InputMax]{};
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

    size_t inputLength_ = 0;
    size_t selectedContactIndex_ = 0;
    uint32_t sequence_ = 1;
    bool hasPendingInvite_ = false;
    bool geometryAnnounced_ = false;
    bool uiDirty_ = true;
    bool fullRedraw_ = true;
    Page page_ = Page::Chat;
    absolute_time_t nextKeyboardStatus_{};

    static constexpr const char *TabLabels[static_cast<size_t>(Page::Count)] = {
        "Messages", "Contacts", "Invite", "Identity",
    };

    ui::Page uiPage_{};

    /*!
     * Last frame described, kept to detect unchanged content.
     *
     * ComponentView holds borrowed text pointers into this object's own buffers,
     * so a shallow copy remains valid and comparison sees the current text.
     */
    ui::ScreenModel lastModel_{};
    ui::HeaderBar headerBar_{{0, 0, 320, 22}, headerText_, headerRight_};
    ui::TabStrip tabStrip_{{8, 26, 304, 14}, TabLabels, static_cast<size_t>(Page::Count), 0};
    ui::Divider headerDivider_{{0, 44, 320, 1}};
    ui::Label pageTitleLabel_{{8, 50, 304, 16}, pageTitle_, ui::Tone::Accent, 1};
    ui::Label primaryLabel_{{8, 68, 304, 14}, contactLine_, ui::Tone::Accent, 1};
    ui::Label secondaryLabel_{{8, 84, 304, 14}, contactMeta_, ui::Tone::Muted, 1};
    ui::Label historyLabels_[VisibleHistoryRows] = {
        ui::Label{{8, 68, 304, 14}, historyRows_[0], ui::Tone::Default, 1},
        ui::Label{{8, 84, 304, 14}, historyRows_[1], ui::Tone::Default, 1},
        ui::Label{{8, 100, 304, 14}, historyRows_[2], ui::Tone::Default, 1},
        ui::Label{{8, 116, 304, 14}, historyRows_[3], ui::Tone::Default, 1},
        ui::Label{{8, 132, 304, 14}, historyRows_[4], ui::Tone::Default, 1},
        ui::Label{{8, 148, 304, 14}, historyRows_[5], ui::Tone::Default, 1},
    };
    ui::TextBox inputBox_{{8, 168, 304, 26}, inputHint_};
    ui::ListItem contactItems_[4] = {
        ui::ListItem{{8, 68, 304, 28}, contactPrimary_[0], contactSecondary_[0], false},
        ui::ListItem{{8, 98, 304, 28}, contactPrimary_[1], contactSecondary_[1], false},
        ui::ListItem{{8, 128, 304, 28}, contactPrimary_[2], contactSecondary_[2], false},
        ui::ListItem{{8, 158, 304, 28}, contactPrimary_[3], contactSecondary_[3], false},
    };
    ui::Label inviteDetailLabel_{{8, 68, 304, 40}, inviteMeta_, ui::Tone::Muted, 1};
    ui::Label identityDetailLabel_{{8, 68, 304, 40}, identityMeta_, ui::Tone::Muted, 1};

    ui::Button sendButton_{{8, 202, 72, 26}, "Send"};
    ui::Button contactsButton_{{88, 202, 76, 26}, "Contacts"};
    ui::Button inviteButton_{{172, 202, 68, 26}, "Invite"};
    ui::Button identityButton_{{248, 202, 64, 26}, "ID"};
    ui::Button selectButton_{{8, 202, 72, 26}, "Select"};
    ui::Button nextButton_{{88, 202, 68, 26}, "Next"};
    ui::Button backButton_{{248, 202, 64, 26}, "Back"};
    ui::Button broadcastButton_{{8, 202, 88, 26}, "Broadcast"};
    ui::Button acceptButton_{{104, 202, 76, 26}, "Accept"};
    ui::Button setNickButton_{{8, 202, 76, 26}, "Set Nick"};
    ui::Button seedButton_{{92, 202, 60, 26}, "Seed"};

    ui::StatusBar statusBar_{{0, 232, 320, 18}, notice_, ui::Tone::Muted};
};
