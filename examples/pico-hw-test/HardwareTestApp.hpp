#pragma once

#include <cstddef>
#include <cstdint>

#include "CardKbKeyboard.hpp"
#include "ContactStore.hpp"
#include "CryptoTypes.hpp"
#include "Ili9341Display.hpp"
#include "LoraMessenger.hpp"
#include "Ui.hpp"
#include "pico/time.h"

class HardwareTestApp {
public:
    HardwareTestApp();

    void run();

private:
    static constexpr size_t InputMax = crypto_chat::MaxPlainTextSize;

    enum class Page {
        Chat,
        Contacts,
        Invite,
        Identity,
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
    void initializeKeyboard();
    void initializeIdentity();
    void render();
    void invalidate();
    void setPage(Page page);
    void preparePage();
    void prepareChatPage();
    void prepareContactsPage();
    void prepareInvitePage();
    void prepareIdentityPage();
    void setNotice(const char *message, uint16_t color);
    void handleKey(uint8_t key);
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
    void pollKeyboard();
    void refreshKeyboardStatus();
    const crypto_chat::Contact *selectedContact() const;
    ui::KeyEvent keyEventFromCode(uint8_t key);
    void fingerprintText(const uint8_t fingerprint[crypto_chat::FingerprintSize], char *out, size_t outSize) const;
    bool parseHexSeed(const char *hexSeed, uint8_t out[32]) const;
    Action actionForFocusedComponent() const;
    size_t focusIndexForAction(Action action) const;

    Ili9341Display display_;
    CardKbKeyboard keyboard_;
    crypto_chat::ContactStore contacts_;
    crypto_chat::LoraMessenger radio_;
    crypto_chat::Contact pendingInvite_{};
    char input_[InputMax]{};
    char recentMessage_[96]{};
    char notice_[64]{};
    char pageTitle_[32]{};
    char contactLine_[64]{};
    char contactMeta_[32]{};
    char inviteLine_[64]{};
    char inviteMeta_[64]{};
    char identityLine_[64]{};
    char identityMeta_[32]{};
    char contactPrimary_[4][32]{};
    char contactSecondary_[4][32]{};
    size_t inputLength_ = 0;
    size_t selectedContactIndex_ = 0;
    uint32_t sequence_ = 1;
    bool hasPendingInvite_ = false;
    bool uiDirty_ = true;
    bool fullRedraw_ = true;
    Page page_ = Page::Chat;
    uint8_t keyEscapeState_ = 0;
    absolute_time_t nextKeyboardStatus_{};

    ui::Theme theme_{};
    ui::Page uiPage_{};
    ui::Label titleLabel_{{8, 8, 304, 18}, pageTitle_, Ili9341Display::White, 2};
    ui::Label noticeLabel_{{8, 222, 304, 14}, notice_, Ili9341Display::Gray, 1};
    ui::Label primaryLabel_{{8, 42, 304, 14}, contactLine_, Ili9341Display::Cyan, 1};
    ui::Label secondaryLabel_{{8, 58, 304, 14}, contactMeta_, Ili9341Display::Gray, 1};
    ui::TextBox inputBox_{{8, 90, 304, 32}, input_};
    ui::Label recentLabel_{{8, 132, 304, 36}, recentMessage_, Ili9341Display::White, 1};
    ui::Button sendButton_{{8, 180, 68, 24}, "Send"};
    ui::Button contactsButton_{{84, 180, 68, 24}, "Contacts"};
    ui::Button inviteButton_{{160, 180, 68, 24}, "Invite"};
    ui::Button identityButton_{{236, 180, 68, 24}, "ID"};
    ui::ListItem contactItems_[4] = {
        ui::ListItem{{8, 42, 304, 30}, contactPrimary_[0], contactSecondary_[0], false},
        ui::ListItem{{8, 76, 304, 30}, contactPrimary_[1], contactSecondary_[1], false},
        ui::ListItem{{8, 110, 304, 30}, contactPrimary_[2], contactSecondary_[2], false},
        ui::ListItem{{8, 144, 304, 30}, contactPrimary_[3], contactSecondary_[3], false},
    };
    ui::Button selectButton_{{8, 180, 84, 24}, "Select"};
    ui::Button nextButton_{{102, 180, 84, 24}, "Next"};
    ui::Button backButton_{{196, 180, 84, 24}, "Back"};
    ui::Button broadcastButton_{{8, 180, 96, 24}, "Broadcast"};
    ui::Button acceptButton_{{114, 180, 84, 24}, "Accept"};
    ui::Button setNickButton_{{8, 180, 92, 24}, "Set Nick"};
    ui::Button seedButton_{{110, 180, 72, 24}, "Seed"};
};
