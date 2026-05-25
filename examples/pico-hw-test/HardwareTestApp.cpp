#include "HardwareTestApp.hpp"

#include <cstdio>
#include <cstring>

#include "CryptoBox.hpp"
#include "InviteProtocol.hpp"
#include "board.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace {
constexpr uint TftCs = 5;
constexpr uint TftSck = 6;
constexpr uint TftMosi = 7;
constexpr uint TftDc = 8;
constexpr uint TftReset = 9;
constexpr uint8_t TftMadctlValue = 0x68;
constexpr uint TftSpiBaud = 30000000u;
constexpr uint16_t TftWidth = 320;
constexpr uint16_t TftHeight = 240;

constexpr uint8_t CardKbAddress = 0x5F;
constexpr uint CardKbSda = 26;
constexpr uint CardKbScl = 27;
constexpr uint CardKbBaud = 100000;
}

HardwareTestApp::HardwareTestApp()
    : display_({ spi0, TftSpiBaud, TftWidth, TftHeight, TftCs, TftSck, TftMosi, TftDc, TftReset, TftMadctlValue }),
      keyboard_({ i2c1, CardKbAddress, CardKbSda, CardKbScl, CardKbBaud })
{
    std::strncpy(notice_, "Starting", sizeof(notice_) - 1);
}

void HardwareTestApp::run()
{
    stdio_init_all();
    sleep_ms(1000);

    initializeDisplay();
    initializeKeyboard();
    initializeIdentity();

    setNotice("Initializing LoRa radio", theme_.focus);
    radio_.init();
    setNotice("Listening on 433 MHz", theme_.success);

    nextKeyboardStatus_ = make_timeout_time_ms(1000);

    while (true) {
        pollRadio();
        pollKeyboard();
        refreshKeyboardStatus();
        render();
        sleep_ms(20);
    }
}

void HardwareTestApp::initializeDisplay()
{
    display_.init();
    setNotice("Display initialized", theme_.success);
}

void HardwareTestApp::initializeKeyboard()
{
    keyboard_.init();
    if (keyboard_.probe()) {
        setNotice("CardKB connected", theme_.success);
        return;
    }

    char status[64];
    uint8_t foundAddress = keyboard_.scanFirstAddress();
    if (foundAddress != 0) {
        std::snprintf(status, sizeof(status), "I2C found 0x%02X, CardKB 0x5F result %d", foundAddress, keyboard_.lastResult());
    } else {
        std::snprintf(status, sizeof(status), "I2C found nothing, 0x5F result %d", keyboard_.lastResult());
    }
    setNotice(status, theme_.danger);
}

void HardwareTestApp::initializeIdentity()
{
    contacts_.load();
    if (contacts_.hasIdentity()) {
        setNotice("Identity loaded from flash", theme_.success);
        return;
    }

    uint8_t deviceId[crypto_chat::DeviceIdSize];
    BoardGetUniqueId(deviceId);

    uint8_t seed[32];
    crypto_chat::CryptoBox::deriveDevelopmentSeed(seed, deviceId);

    crypto_chat::DeviceIdentity identity;
    crypto_chat::CryptoBox::makeIdentity(identity, deviceId, "pico", seed, true);
    contacts_.setIdentity(identity);
    contacts_.save();
    setNotice("DEV identity saved", theme_.focus);
}

void HardwareTestApp::render()
{
    if (!uiDirty_) {
        return;
    }

    preparePage();
    if (fullRedraw_) {
        display_.fillRect(0, 0, TftWidth, TftHeight, theme_.background);
        fullRedraw_ = false;
    }
    uiPage_.render(display_, theme_);
    uiDirty_ = false;
}

void HardwareTestApp::invalidate()
{
    uiDirty_ = true;
}

void HardwareTestApp::setPage(Page page)
{
    page_ = page;
    uiPage_.setFocusedIndex(focusIndexForAction(Action::None));
    fullRedraw_ = true;
    invalidate();
}

void HardwareTestApp::preparePage()
{
    switch (page_) {
    case Page::Chat:
        prepareChatPage();
        break;
    case Page::Contacts:
        prepareContactsPage();
        break;
    case Page::Invite:
        prepareInvitePage();
        break;
    case Page::Identity:
        prepareIdentityPage();
        break;
    }
}

void HardwareTestApp::prepareChatPage()
{
    std::strncpy(pageTitle_, "Messages", sizeof(pageTitle_) - 1);
    const crypto_chat::Contact *contact = selectedContact();
    if (contact == nullptr) {
        std::strncpy(contactLine_, "No contact selected", sizeof(contactLine_) - 1);
        std::strncpy(contactMeta_, "Use Invite to pair a device", sizeof(contactMeta_) - 1);
    } else {
        char fingerprint[20];
        fingerprintText(contact->fingerprint, fingerprint, sizeof(fingerprint));
        std::snprintf(contactLine_, sizeof(contactLine_), "%s", contact->nickname);
        std::snprintf(contactMeta_, sizeof(contactMeta_), "%s", fingerprint);
    }

    titleLabel_.setText(pageTitle_);
    primaryLabel_.setText(contactLine_);
    primaryLabel_.setColor(theme_.accent);
    secondaryLabel_.setText(contactMeta_);
    inputBox_.setText(input_);
    recentLabel_.setText(recentMessage_);
    ui::Component *components[] = {
        &titleLabel_, &inputBox_, &sendButton_, &contactsButton_, &inviteButton_, &identityButton_,
        &primaryLabel_, &secondaryLabel_, &recentLabel_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::prepareContactsPage()
{
    std::strncpy(pageTitle_, "Contacts", sizeof(pageTitle_) - 1);
    titleLabel_.setText(pageTitle_);

    size_t firstVisible = selectedContactIndex_ >= 4 ? selectedContactIndex_ - 3 : 0;
    for (size_t i = 0; i < 4; i++) {
        size_t contactIndex = firstVisible + i;
        const crypto_chat::Contact *contact = contacts_.contactAt(contactIndex);
        std::memset(contactPrimary_[i], 0, sizeof(contactPrimary_[i]));
        std::memset(contactSecondary_[i], 0, sizeof(contactSecondary_[i]));
        if (contact == nullptr) {
            std::strncpy(contactPrimary_[i], "-", sizeof(contactPrimary_[i]) - 1);
        } else {
            char fingerprint[20];
            fingerprintText(contact->fingerprint, fingerprint, sizeof(fingerprint));
            std::snprintf(contactPrimary_[i], sizeof(contactPrimary_[i]), "%s", contact->nickname);
            std::snprintf(contactSecondary_[i], sizeof(contactSecondary_[i]), "%s", fingerprint);
        }
        contactItems_[i].setContent(contactPrimary_[i], contactSecondary_[i], contactIndex == selectedContactIndex_);
    }

    ui::Component *components[] = {
        &titleLabel_, &contactItems_[0], &contactItems_[1], &contactItems_[2], &contactItems_[3],
        &selectButton_, &nextButton_, &backButton_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::prepareInvitePage()
{
    std::strncpy(pageTitle_, "Invite", sizeof(pageTitle_) - 1);
    titleLabel_.setText(pageTitle_);
    if (hasPendingInvite_) {
        char fingerprint[20];
        fingerprintText(pendingInvite_.fingerprint, fingerprint, sizeof(fingerprint));
        std::snprintf(inviteLine_, sizeof(inviteLine_), "Pending: %s", pendingInvite_.nickname);
        std::snprintf(inviteMeta_, sizeof(inviteMeta_), "%s", fingerprint);
        primaryLabel_.setColor(theme_.focus);
    } else {
        std::strncpy(inviteLine_, "No pending invite", sizeof(inviteLine_) - 1);
        std::strncpy(inviteMeta_, "Broadcast to pair nearby devices", sizeof(inviteMeta_) - 1);
        primaryLabel_.setColor(theme_.muted);
    }
    primaryLabel_.setText(inviteLine_);
    secondaryLabel_.setText(inviteMeta_);
    ui::Component *components[] = {
        &titleLabel_, &primaryLabel_, &secondaryLabel_, &broadcastButton_, &acceptButton_, &backButton_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::prepareIdentityPage()
{
    std::strncpy(pageTitle_, "Identity", sizeof(pageTitle_) - 1);
    char fingerprint[20];
    fingerprintText(contacts_.identity().fingerprint, fingerprint, sizeof(fingerprint));
    std::snprintf(identityLine_, sizeof(identityLine_), "Nick: %s", contacts_.identity().nickname);
    std::snprintf(identityMeta_, sizeof(identityMeta_), "%s", fingerprint);

    titleLabel_.setText(pageTitle_);
    primaryLabel_.setText(identityLine_);
    primaryLabel_.setColor(theme_.accent);
    secondaryLabel_.setText(identityMeta_);
    inputBox_.setText(input_);
    noticeLabel_.setText(notice_);

    ui::Component *components[] = {
        &titleLabel_, &primaryLabel_, &secondaryLabel_, &inputBox_, &setNickButton_, &seedButton_, &backButton_, &noticeLabel_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::setNotice(const char *message, uint16_t color)
{
    std::memset(notice_, 0, sizeof(notice_));
    std::strncpy(notice_, message, sizeof(notice_) - 1);
    noticeLabel_.setColor(color);
    std::printf("%s\r\n", message);
    invalidate();
}

void HardwareTestApp::handleKey(uint8_t key)
{
    uint8_t previousEscapeState = keyEscapeState_;
    ui::KeyEvent event = keyEventFromCode(key);
    if (event.key == ui::Key::None && (previousEscapeState != 0 || keyEscapeState_ != 0 || key == 0x1b)) {
        return;
    }
    handleKeyEvent(event);
}

void HardwareTestApp::handleKeyEvent(ui::KeyEvent event)
{
    if (event.key == ui::Key::None) {
        return;
    }

    if (event.key == ui::Key::Character) {
        if ((page_ == Page::Chat || page_ == Page::Identity) && inputLength_ < InputMax - 1) {
            input_[inputLength_++] = event.character;
            input_[inputLength_] = '\0';
            invalidate();
        }
        return;
    }

    if (event.key == ui::Key::Backspace) {
        if ((page_ == Page::Chat || page_ == Page::Identity) && inputLength_ > 0) {
            input_[--inputLength_] = '\0';
            invalidate();
        }
        return;
    }

    if (event.key == ui::Key::Enter) {
        if (page_ == Page::Contacts && uiPage_.focusedIndex() >= 1 && uiPage_.focusedIndex() <= 4) {
            size_t firstVisible = selectedContactIndex_ >= 4 ? selectedContactIndex_ - 3 : 0;
            size_t chosen = firstVisible + uiPage_.focusedIndex() - 1;
            if (chosen < contacts_.contactCount()) {
                selectedContactIndex_ = chosen;
            }
        }
        activateAction(actionForFocusedComponent());
        return;
    }

    uiPage_.handle(event);
    if (page_ == Page::Contacts && event.key == ui::Key::Up && selectedContactIndex_ > 0) {
        selectedContactIndex_--;
    } else if (page_ == Page::Contacts && event.key == ui::Key::Down && selectedContactIndex_ + 1 < contacts_.contactCount()) {
        selectedContactIndex_++;
    }
    invalidate();
}

void HardwareTestApp::activateAction(Action action)
{
    switch (action) {
    case Action::Send:
        sendCurrentInput();
        break;
    case Action::OpenContacts:
        setPage(Page::Contacts);
        break;
    case Action::OpenInvite:
        setPage(Page::Invite);
        break;
    case Action::OpenIdentity:
        setPage(Page::Identity);
        break;
    case Action::SelectContact:
        setPage(Page::Chat);
        break;
    case Action::NextContact:
        selectNextContact();
        break;
    case Action::BackToChat:
        setPage(Page::Chat);
        break;
    case Action::BroadcastInvite:
        sendInvite();
        break;
    case Action::AcceptInvite:
        acceptPendingInvite();
        break;
    case Action::SetNickname:
        setNickname(input_);
        std::memset(input_, 0, sizeof(input_));
        inputLength_ = 0;
        invalidate();
        break;
    case Action::ProvisionSeed:
        provisionSeed(input_);
        std::memset(input_, 0, sizeof(input_));
        inputLength_ = 0;
        invalidate();
        break;
    case Action::None:
        break;
    }
}

void HardwareTestApp::sendCurrentInput()
{
    if (inputLength_ == 0 || radio_.isBusy()) {
        return;
    }

    if (input_[0] == '/') {
        handleCommand();
        std::memset(input_, 0, sizeof(input_));
        inputLength_ = 0;
        invalidate();
        return;
    }

    const crypto_chat::Contact *contact = selectedContact();
    if (contact == nullptr) {
        setNotice("No selected contact", theme_.danger);
        return;
    }

    uint8_t packet[crypto_chat::MaxPacketSize];
    size_t packetLength = 0;
    if (!crypto_chat::InviteProtocol::encodeEncryptedMessage(contacts_.identity(), *contact, sequence_++, input_, packet, sizeof(packet), packetLength)) {
        setNotice("Encrypt failed", theme_.danger);
        return;
    }

    if (!radio_.sendPacket(packet, packetLength)) {
        setNotice("LoRa TX rejected", theme_.danger);
        return;
    }

    std::memset(input_, 0, sizeof(input_));
    inputLength_ = 0;
    invalidate();
}

void HardwareTestApp::handleCommand()
{
    if (std::strcmp(input_, "/i") == 0) {
        sendInvite();
        return;
    }
    if (std::strcmp(input_, "/a") == 0) {
        acceptPendingInvite();
        return;
    }
    if (std::strcmp(input_, "/c") == 0) {
        selectNextContact();
        return;
    }
    if (std::strncmp(input_, "/n ", 3) == 0) {
        setNickname(input_ + 3);
        return;
    }
    if (std::strncmp(input_, "/seed ", 6) == 0) {
        provisionSeed(input_ + 6);
        return;
    }
    setNotice("Unknown command", theme_.danger);
}

void HardwareTestApp::sendInvite()
{
    uint8_t packet[crypto_chat::MaxPacketSize];
    size_t packetLength = 0;
    if (!crypto_chat::InviteProtocol::encodeInvite(contacts_.identity(), crypto_chat::PacketType::InviteBroadcast,
                                                   crypto_chat::InviteProtocol::BroadcastDeviceId,
                                                   sequence_++, packet, sizeof(packet), packetLength)) {
        setNotice("Invite encode failed", theme_.danger);
        return;
    }
    if (!radio_.sendPacket(packet, packetLength)) {
        setNotice("Invite TX rejected", theme_.danger);
        return;
    }
    setNotice("Invite sent", theme_.success);
}

void HardwareTestApp::acceptPendingInvite()
{
    if (!hasPendingInvite_) {
        setNotice("No pending invite", theme_.danger);
        return;
    }
    if (contacts_.nicknameCollisionWithDifferentKey(pendingInvite_)) {
        setNotice("Nickname impersonation rejected", theme_.danger);
        hasPendingInvite_ = false;
        return;
    }
    if (!contacts_.upsertContact(pendingInvite_)) {
        setNotice("Contact store full", theme_.danger);
        return;
    }
    contacts_.save();
    selectedContactIndex_ = contacts_.contactCount() - 1;
    invalidate();

    uint8_t packet[crypto_chat::MaxPacketSize];
    size_t packetLength = 0;
    if (crypto_chat::InviteProtocol::encodeInvite(contacts_.identity(), crypto_chat::PacketType::InviteAccept,
                                                  pendingInvite_.deviceId, sequence_++,
                                                  packet, sizeof(packet), packetLength)) {
        radio_.sendPacket(packet, packetLength);
    }
    hasPendingInvite_ = false;
    setNotice("Contact accepted", theme_.success);
}

void HardwareTestApp::selectNextContact()
{
    size_t count = contacts_.contactCount();
    if (count == 0) {
        setNotice("No contacts", theme_.danger);
        return;
    }
    selectedContactIndex_ = (selectedContactIndex_ + 1) % count;
    setNotice("Contact selected", theme_.success);
}

void HardwareTestApp::setNickname(const char *nickname)
{
    if (nickname == nullptr || nickname[0] == '\0') {
        setNotice("Nickname missing", theme_.danger);
        return;
    }
    std::memset(contacts_.identity().nickname, 0, crypto_chat::NicknameSize);
    std::strncpy(contacts_.identity().nickname, nickname, crypto_chat::NicknameSize - 1);
    contacts_.save();
    setNotice("Nickname saved", theme_.success);
}

void HardwareTestApp::provisionSeed(const char *hexSeed)
{
    uint8_t seed[32];
    if (!parseHexSeed(hexSeed, seed)) {
        setNotice("Seed must be 64 hex chars", theme_.danger);
        return;
    }

    crypto_chat::DeviceIdentity identity;
    crypto_chat::CryptoBox::makeIdentity(identity, contacts_.identity().deviceId, contacts_.identity().nickname, seed, false);
    contacts_.setIdentity(identity);
    contacts_.clearContacts();
    contacts_.save();
    selectedContactIndex_ = 0;
    hasPendingInvite_ = false;
    setNotice("Seed provisioned; contacts cleared", theme_.success);
}

void HardwareTestApp::pollRadio()
{
    radio_.processIrq();

    switch (radio_.consumeEvent()) {
    case crypto_chat::LoraMessenger::Event::PacketReceived:
        handleReceivedPacket();
        break;
    case crypto_chat::LoraMessenger::Event::TxDone:
        setNotice("LoRa TX complete", theme_.success);
        break;
    case crypto_chat::LoraMessenger::Event::TxTimeout:
        setNotice("LoRa TX timeout", theme_.danger);
        break;
    case crypto_chat::LoraMessenger::Event::RxError:
        setNotice("LoRa RX error", theme_.danger);
        break;
    case crypto_chat::LoraMessenger::Event::None:
        break;
    }
}

void HardwareTestApp::handleReceivedPacket()
{
    crypto_chat::DecodedPacket packet;
    if (!crypto_chat::InviteProtocol::decode(radio_.receivedPacket(), radio_.receivedPacketLength(), packet) ||
        !crypto_chat::InviteProtocol::isForDevice(packet, contacts_.identity().deviceId)) {
        return;
    }

    if (std::memcmp(packet.header.senderDeviceId, contacts_.identity().deviceId, crypto_chat::DeviceIdSize) == 0) {
        return;
    }

    if (packet.header.type == crypto_chat::PacketType::InviteBroadcast ||
        packet.header.type == crypto_chat::PacketType::InviteAccept) {
        crypto_chat::Contact contact;
        if (!crypto_chat::InviteProtocol::decodeInvitePayload(packet, contact)) {
            setNotice("Bad invite signature", theme_.danger);
            return;
        }
        if (contacts_.nicknameCollisionWithDifferentKey(contact)) {
            setNotice("Impersonation invite rejected", theme_.danger);
            return;
        }
        if (packet.header.type == crypto_chat::PacketType::InviteAccept) {
            contacts_.upsertContact(contact);
            contacts_.save();
            setNotice("Invite accepted by peer", theme_.success);
            return;
        }
        pendingInvite_ = contact;
        hasPendingInvite_ = true;
        char status[64];
        std::snprintf(status, sizeof(status), "Invite from %s", contact.nickname);
        setNotice(status, theme_.focus);
        return;
    }

    const crypto_chat::Contact *contact = contacts_.findByDeviceId(packet.header.senderDeviceId);
    if (contact == nullptr) {
        setNotice("Encrypted msg from unknown", theme_.danger);
        return;
    }

    char message[crypto_chat::MaxPlainTextSize + 1];
    if (!crypto_chat::InviteProtocol::decodeEncryptedMessage(contacts_.identity(), *contact, packet, message, sizeof(message))) {
        setNotice("Message auth failed", theme_.danger);
        return;
    }
    std::snprintf(recentMessage_, sizeof(recentMessage_), "%s: %s", contact->nickname, message);
    setNotice("Encrypted message received", theme_.success);
}

void HardwareTestApp::pollKeyboard()
{
    uint8_t key = 0;
    if (!keyboard_.readKey(key)) {
        return;
    }

    std::printf("CardKB key: 0x%02X '%c'\r\n", key, key >= 32 && key <= 126 ? key : '.');
    handleKey(key);
}

void HardwareTestApp::refreshKeyboardStatus()
{
    if (absolute_time_diff_us(get_absolute_time(), nextKeyboardStatus_) > 0) {
        return;
    }

    if (keyboard_.lastResult() != 1) {
        char status[64];
        std::snprintf(status, sizeof(status), "CardKB read failed, result %d", keyboard_.lastResult());
        setNotice(status, theme_.danger);
        keyboard_.init();
    }

    nextKeyboardStatus_ = make_timeout_time_ms(1000);
}

const crypto_chat::Contact *HardwareTestApp::selectedContact() const
{
    return contacts_.contactAt(selectedContactIndex_);
}

ui::KeyEvent HardwareTestApp::keyEventFromCode(uint8_t key)
{
    ui::KeyEvent event{};
    if (keyEscapeState_ == 1) {
        keyEscapeState_ = key == '[' ? 2 : 0;
        return event;
    }

    if (keyEscapeState_ == 2) {
        keyEscapeState_ = 0;
        if (key == 'A') {
            event.key = ui::Key::Up;
            return event;
        }
        if (key == 'B') {
            event.key = ui::Key::Down;
            return event;
        }
        if (key == 'C') {
            event.key = ui::Key::Right;
            return event;
        }
        if (key == 'D') {
            event.key = ui::Key::Left;
            return event;
        }
        return event;
    }

    if (key == 0x1b) {
        keyEscapeState_ = 1;
        return event;
    }

    switch (key) {
    case 0x1c:
    case 0x80:
    case 0xb4:
        event.key = ui::Key::Left;
        return event;
    case 0x1d:
    case 0x81:
    case 0xb6:
        event.key = ui::Key::Right;
        return event;
    case 0x1e:
    case 0x82:
    case 0xb5:
        event.key = ui::Key::Up;
        return event;
    case 0x1f:
    case 0x83:
    case 0xb7:
        event.key = ui::Key::Down;
        return event;
    case '\r':
    case '\n':
        event.key = ui::Key::Enter;
        return event;
    case 8:
    case 127:
        event.key = ui::Key::Backspace;
        return event;
    default:
        if (key >= 32 && key <= 126) {
            event.key = ui::Key::Character;
            event.character = static_cast<char>(key);
        }
        return event;
    }
}

void HardwareTestApp::fingerprintText(const uint8_t fingerprint[crypto_chat::FingerprintSize], char *out, size_t outSize) const
{
    if (outSize == 0) {
        return;
    }
    static constexpr char hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    for (size_t i = 0; i < crypto_chat::FingerprintSize && pos + 2 < outSize; i++) {
        out[pos++] = hex[fingerprint[i] >> 4];
        out[pos++] = hex[fingerprint[i] & 0x0f];
    }
    out[pos] = '\0';
}

bool HardwareTestApp::parseHexSeed(const char *hexSeed, uint8_t out[32]) const
{
    if (hexSeed == nullptr || std::strlen(hexSeed) != 64) {
        return false;
    }

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };

    for (size_t i = 0; i < 32; i++) {
        int high = nibble(hexSeed[i * 2]);
        int low = nibble(hexSeed[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

HardwareTestApp::Action HardwareTestApp::actionForFocusedComponent() const
{
    size_t focus = uiPage_.focusedIndex();
    switch (page_) {
    case Page::Chat:
        if (focus == 1 || focus == 2) {
            return Action::Send;
        }
        if (focus == 3) {
            return Action::OpenContacts;
        }
        if (focus == 4) {
            return Action::OpenInvite;
        }
        if (focus == 5) {
            return Action::OpenIdentity;
        }
        break;
    case Page::Contacts:
        if (focus >= 1 && focus <= 4) {
            size_t firstVisible = selectedContactIndex_ >= 4 ? selectedContactIndex_ - 3 : 0;
            size_t chosen = firstVisible + focus - 1;
            if (chosen < contacts_.contactCount()) {
                return Action::SelectContact;
            }
        }
        if (focus == 5) {
            return Action::SelectContact;
        }
        if (focus == 6) {
            return Action::NextContact;
        }
        if (focus == 7) {
            return Action::BackToChat;
        }
        break;
    case Page::Invite:
        if (focus == 1) {
            return hasPendingInvite_ ? Action::AcceptInvite : Action::BroadcastInvite;
        }
        if (focus == 3) {
            return Action::BroadcastInvite;
        }
        if (focus == 4) {
            return Action::AcceptInvite;
        }
        if (focus == 5) {
            return Action::BackToChat;
        }
        break;
    case Page::Identity:
        if (focus == 3) {
            return inputLength_ == 64 ? Action::ProvisionSeed : Action::SetNickname;
        }
        if (focus == 4) {
            return Action::SetNickname;
        }
        if (focus == 5) {
            return Action::ProvisionSeed;
        }
        if (focus == 6) {
            return Action::BackToChat;
        }
        break;
    }
    return Action::None;
}

size_t HardwareTestApp::focusIndexForAction(Action action) const
{
    switch (page_) {
    case Page::Chat:
        if (action == Action::OpenContacts) {
            return 3;
        }
        if (action == Action::OpenInvite) {
            return 4;
        }
        if (action == Action::OpenIdentity) {
            return 5;
        }
        return 1;
    case Page::Contacts:
        if (action == Action::NextContact) {
            return 6;
        }
        if (action == Action::BackToChat) {
            return 7;
        }
        return 1;
    case Page::Invite:
        if (action == Action::AcceptInvite || hasPendingInvite_) {
            return 4;
        }
        if (action == Action::BackToChat) {
            return 5;
        }
        return 3;
    case Page::Identity:
        if (action == Action::BackToChat) {
            return 6;
        }
        return 3;
    }
    return 0;
}
