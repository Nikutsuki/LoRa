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
      keyboard_({ i2c1, CardKbAddress, CardKbSda, CardKbScl, CardKbBaud }),
      session_(usbLink_),
      lcdScreen_(display_, theme_),
      uartScreen_(session_, TftWidth, TftHeight),
      cardKbInput_(keyboard_),
      uartInput_(session_)
{
    std::strncpy(notice_, "Starting", sizeof(notice_) - 1);
}

void HardwareTestApp::run()
{
    stdio_init_all();
    sleep_ms(1000);

    initializeDisplay();
    initializeUartSurface();

    if (initializeKeyboard()) {
        screen_ = &lcdScreen_;
        keySource_ = &cardKbInput_;
        setNotice("CardKB connected", ui::Tone::Success);
    } else {
        screen_ = &uartScreen_;
        keySource_ = &uartInput_;
    }

    initializeIdentity();

    setNotice("Initializing LoRa radio", ui::Tone::Focus);
    radio_.init();
    setNotice("Listening on 433 MHz", ui::Tone::Success);

    nextKeyboardStatus_ = make_timeout_time_ms(1000);

    while (true) {
        session_.poll();

        // A host that just probed us has an empty widget table, so paint a full
        // frame for it instead of an incremental update.
        if (session_.consumeResyncRequest()) {
            fullRedraw_ = true;
            uiDirty_ = true;
        }

        // Announce the layout once per attached host. Hello is sent here rather
        // than at boot because the USB CDC port does not exist until the host
        // opens it, so an early announcement would be silently dropped.
        announceGeometry();

        pollRadio();
        pollInput();
        render();
        sleep_ms(20);
    }
}

void HardwareTestApp::initializeDisplay()
{
    display_.init();
}

bool HardwareTestApp::initializeKeyboard()
{
    keyboard_.init();
    return keyboard_.probe();
}

void HardwareTestApp::initializeUartSurface()
{
    session_.init();
    setNotice("UART control ready", ui::Tone::Focus);
}

void HardwareTestApp::announceGeometry()
{
    // The host opens the port some time after boot, so announce once it appears
    // and again if it reconnects.
    const bool connected = usbLink_.connected();
    if (connected && !geometryAnnounced_) {
        uartScreen_.sendHello();
        geometryAnnounced_ = true;
    } else if (!connected) {
        geometryAnnounced_ = false;
    }
}

void HardwareTestApp::initializeIdentity()
{
    contacts_.load();
    if (contacts_.hasIdentity()) {
        setNotice("Identity loaded from flash", ui::Tone::Success);
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
    setNotice("DEV identity saved", ui::Tone::Focus);
}

void HardwareTestApp::render()
{
    if (!uiDirty_) {
        return;
    }

    preparePage();

    ui::ScreenModel model;
    model.setFullRedraw(fullRedraw_);
    uiPage_.describe(model, theme_);

    // Repainting identical content is what makes the panel flicker: every label
    // is cleared and redrawn, and text is pushed one glyph pixel at a time. Most
    // invalidations come from radio events that do not change the visible page,
    // so skipping the identical case removes nearly all of the flicker for very
    // little code.
    const bool unchanged = !fullRedraw_ && lastModel_.sameContent(model);
    if (!unchanged) {
        screen_->present(model);
    }

    // The host still needs the frame even when the local display already shows
    // it, because a terminal can attach at any time with its own blank state.
    if (screen_ != &uartScreen_ && session_.hostSeen() && !unchanged) {
        uartScreen_.present(model);
    }

    if (!uartScreen_.lastFrameSent()) {
        // Overlong frames must not be written to the stream, so the host would
        // otherwise keep showing stale content with no explanation. Report it as
        // a protocol notice instead of printf, which would corrupt the binary
        // stream with interleaved text.
        proto::NoticeMessage overflow;
        overflow.level = proto::NoticeLevel::Danger;
        overflow.text = "screen frame too large";
        session_.sendNotice(overflow);
    }

    lastModel_ = model;
    fullRedraw_ = false;
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
    prepareCommon();

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
    case Page::Count:
        break;
    }
}

void HardwareTestApp::prepareCommon()
{
    // Header mirrors the Go UI's top bar: product name, nickname and page.
    std::snprintf(headerText_, sizeof(headerText_), "MESSENGER  |  User: %s",
                  contacts_.identity().nickname);
    std::snprintf(headerRight_, sizeof(headerRight_), "433 MHz");
    headerBar_.setTitle(headerText_);
    headerBar_.setRight(headerRight_);

    tabStrip_.setActive(static_cast<size_t>(page_));

    statusBar_.setText(notice_);
}

void HardwareTestApp::rebuildHistoryRows()
{
    const size_t total = history_.count();
    const size_t shown = total < VisibleHistoryRows ? total : VisibleHistoryRows;
    const size_t firstEntry = total - shown;

    for (size_t i = 0; i < VisibleHistoryRows; i++) {
        char *row = historyRows_[i];
        row[0] = '\0';

        if (i < VisibleHistoryRows - shown) {
            continue;
        }

        const crypto_chat::ChatEntry &entry = history_.at(firstEntry + (i - (VisibleHistoryRows - shown)));
        std::memset(row, 0, sizeof(historyRows_[i]));
        std::strncpy(row, entry.text, sizeof(historyRows_[i]) - 1);

        // Colour carries the direction so the eye can scan a conversation.
        ui::Tone tone = ui::Tone::Muted;
        if (entry.direction == crypto_chat::ChatEntry::Direction::Outgoing) {
            tone = ui::Tone::Success;
        } else if (entry.direction == crypto_chat::ChatEntry::Direction::Incoming) {
            tone = ui::Tone::Default;
        }
        historyLabels_[i].setTone(tone);
        historyLabels_[i].setText(row);
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

    pageTitleLabel_.setText(pageTitle_);
    primaryLabel_.setText(contactLine_);
    primaryLabel_.setTone(ui::Tone::Accent);
    secondaryLabel_.setText(contactMeta_);

    rebuildHistoryRows();

    // The Go UI prompts with "> " when focused and shows a dim prompt otherwise.
    std::snprintf(inputHint_, sizeof(inputHint_), "%s%s",
                  "> ", input_);
    inputBox_.setText(inputHint_);

    ui::Component *components[] = {
        &headerBar_, &tabStrip_, &headerDivider_, &pageTitleLabel_, &primaryLabel_, &secondaryLabel_,
        &historyLabels_[0], &historyLabels_[1], &historyLabels_[2],
        &historyLabels_[3], &historyLabels_[4], &historyLabels_[5],
        &inputBox_, &sendButton_, &contactsButton_, &inviteButton_, &identityButton_,
        &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::prepareContactsPage()
{
    std::strncpy(pageTitle_, "Contacts", sizeof(pageTitle_) - 1);
    pageTitleLabel_.setText(pageTitle_);

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
        &headerBar_, &tabStrip_, &headerDivider_, &pageTitleLabel_,
        &contactItems_[0], &contactItems_[1], &contactItems_[2], &contactItems_[3],
        &selectButton_, &nextButton_, &backButton_, &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::prepareInvitePage()
{
    std::strncpy(pageTitle_, "Invite", sizeof(pageTitle_) - 1);
    pageTitleLabel_.setText(pageTitle_);

    if (hasPendingInvite_) {
        char fingerprint[20];
        fingerprintText(pendingInvite_.fingerprint, fingerprint, sizeof(fingerprint));
        std::snprintf(inviteLine_, sizeof(inviteLine_), "Pending: %s", pendingInvite_.nickname);
        std::snprintf(inviteMeta_, sizeof(inviteMeta_), "Fingerprint: %s\r\nAccept to pair",
                      fingerprint);
        primaryLabel_.setTone(ui::Tone::Focus);
    } else {
        std::strncpy(inviteLine_, "No pending invite", sizeof(inviteLine_) - 1);
        std::strncpy(inviteMeta_, "Broadcast to pair nearby devices", sizeof(inviteMeta_) - 1);
        primaryLabel_.setTone(ui::Tone::Muted);
    }
    primaryLabel_.setText(inviteLine_);
    inviteDetailLabel_.setText(inviteMeta_);

    ui::Component *components[] = {
        &headerBar_, &tabStrip_, &headerDivider_, &pageTitleLabel_, &primaryLabel_, &inviteDetailLabel_,
        &broadcastButton_, &acceptButton_, &backButton_, &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::prepareIdentityPage()
{
    std::strncpy(pageTitle_, "Identity", sizeof(pageTitle_) - 1);
    char fingerprint[20];
    fingerprintText(contacts_.identity().fingerprint, fingerprint, sizeof(fingerprint));
    std::snprintf(identityLine_, sizeof(identityLine_), "Nick: %s", contacts_.identity().nickname);
    std::snprintf(identityMeta_, sizeof(identityMeta_), "Fingerprint: %s", fingerprint);

    pageTitleLabel_.setText(pageTitle_);
    primaryLabel_.setText(identityLine_);
    primaryLabel_.setTone(ui::Tone::Accent);
    identityDetailLabel_.setText(identityMeta_);

    std::snprintf(inputHint_, sizeof(inputHint_), "%s%s", "> ", input_);
    inputBox_.setText(inputHint_);

    ui::Component *components[] = {
        &headerBar_, &tabStrip_, &headerDivider_, &pageTitleLabel_, &primaryLabel_, &identityDetailLabel_,
        &inputBox_, &setNickButton_, &seedButton_, &backButton_, &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void HardwareTestApp::setNotice(const char *message, ui::Tone tone)
{
    std::memset(notice_, 0, sizeof(notice_));
    std::strncpy(notice_, message, sizeof(notice_) - 1);
    statusBar_.setTone(tone);
    statusBar_.setText(notice_);

    // Text is delivered only as protocol notices; writing it to stdout would
    // interleave with binary frames on the same USB CDC stream and corrupt them.
    proto::NoticeMessage notice;
    notice.level = tone == ui::Tone::Danger ? proto::NoticeLevel::Danger
                  : tone == ui::Tone::Success ? proto::NoticeLevel::Success
                                              : proto::NoticeLevel::Info;
    notice.text = notice_;
    session_.sendNotice(notice);

    invalidate();
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
        setNotice("No selected contact", ui::Tone::Danger);
        return;
    }

    uint8_t packet[crypto_chat::MaxPacketSize];
    size_t packetLength = 0;
    if (!crypto_chat::InviteProtocol::encodeEncryptedMessage(contacts_.identity(), *contact, sequence_++, input_, packet, sizeof(packet), packetLength)) {
        setNotice("Encrypt failed", ui::Tone::Danger);
        return;
    }

    if (!radio_.sendPacket(packet, packetLength)) {
        setNotice("LoRa TX rejected", ui::Tone::Danger);
        return;
    }

    // Record what we sent, so the history reads as a conversation rather than a
    // receive log. Added only after the radio accepted the packet, so the display
    // never claims a message that failed to transmit.
    history_.addOutgoing(input_);

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
    setNotice("Unknown command", ui::Tone::Danger);
}

void HardwareTestApp::sendInvite()
{
    uint8_t packet[crypto_chat::MaxPacketSize];
    size_t packetLength = 0;
    if (!crypto_chat::InviteProtocol::encodeInvite(contacts_.identity(), crypto_chat::PacketType::InviteBroadcast,
                                                   crypto_chat::InviteProtocol::BroadcastDeviceId,
                                                   sequence_++, packet, sizeof(packet), packetLength)) {
        setNotice("Invite encode failed", ui::Tone::Danger);
        return;
    }
    if (!radio_.sendPacket(packet, packetLength)) {
        setNotice("Invite TX rejected", ui::Tone::Danger);
        return;
    }
    setNotice("Invite sent", ui::Tone::Success);
}

void HardwareTestApp::acceptPendingInvite()
{
    if (!hasPendingInvite_) {
        setNotice("No pending invite", ui::Tone::Danger);
        return;
    }
    if (contacts_.nicknameCollisionWithDifferentKey(pendingInvite_)) {
        setNotice("Nickname impersonation rejected", ui::Tone::Danger);
        hasPendingInvite_ = false;
        return;
    }
    if (!contacts_.upsertContact(pendingInvite_)) {
        setNotice("Contact store full", ui::Tone::Danger);
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
    setNotice("Contact accepted", ui::Tone::Success);
}

void HardwareTestApp::selectNextContact()
{
    size_t count = contacts_.contactCount();
    if (count == 0) {
        setNotice("No contacts", ui::Tone::Danger);
        return;
    }
    selectedContactIndex_ = (selectedContactIndex_ + 1) % count;
    setNotice("Contact selected", ui::Tone::Success);
}

void HardwareTestApp::setNickname(const char *nickname)
{
    if (nickname == nullptr || nickname[0] == '\0') {
        setNotice("Nickname missing", ui::Tone::Danger);
        return;
    }
    std::memset(contacts_.identity().nickname, 0, crypto_chat::NicknameSize);
    std::strncpy(contacts_.identity().nickname, nickname, crypto_chat::NicknameSize - 1);
    contacts_.save();
    setNotice("Nickname saved", ui::Tone::Success);
}

void HardwareTestApp::provisionSeed(const char *hexSeed)
{
    uint8_t seed[32];
    if (!parseHexSeed(hexSeed, seed)) {
        setNotice("Seed must be 64 hex chars", ui::Tone::Danger);
        return;
    }

    crypto_chat::DeviceIdentity identity;
    crypto_chat::CryptoBox::makeIdentity(identity, contacts_.identity().deviceId, contacts_.identity().nickname, seed, false);
    contacts_.setIdentity(identity);
    contacts_.clearContacts();
    contacts_.save();
    selectedContactIndex_ = 0;
    hasPendingInvite_ = false;
    setNotice("Seed provisioned; contacts cleared", ui::Tone::Success);
}

void HardwareTestApp::pollRadio()
{
    radio_.processIrq();

    switch (radio_.consumeEvent()) {
    case crypto_chat::LoraMessenger::Event::PacketReceived:
        handleReceivedPacket();
        break;
    case crypto_chat::LoraMessenger::Event::TxDone:
        setNotice("LoRa TX complete", ui::Tone::Success);
        break;
    case crypto_chat::LoraMessenger::Event::TxTimeout:
        setNotice("LoRa TX timeout", ui::Tone::Danger);
        break;
    case crypto_chat::LoraMessenger::Event::RxError:
        setNotice("LoRa RX error", ui::Tone::Danger);
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
            setNotice("Bad invite signature", ui::Tone::Danger);
            return;
        }
        if (contacts_.nicknameCollisionWithDifferentKey(contact)) {
            setNotice("Impersonation invite rejected", ui::Tone::Danger);
            return;
        }
        if (packet.header.type == crypto_chat::PacketType::InviteAccept) {
            contacts_.upsertContact(contact);
            contacts_.save();
            setNotice("Invite accepted by peer", ui::Tone::Success);
            return;
        }
        pendingInvite_ = contact;
        hasPendingInvite_ = true;
        char status[64];
        std::snprintf(status, sizeof(status), "Invite from %s", contact.nickname);
        setNotice(status, ui::Tone::Focus);
        return;
    }

    const crypto_chat::Contact *contact = contacts_.findByDeviceId(packet.header.senderDeviceId);
    if (contact == nullptr) {
        setNotice("Encrypted msg from unknown", ui::Tone::Danger);
        return;
    }

    char message[crypto_chat::MaxPlainTextSize + 1];
    if (!crypto_chat::InviteProtocol::decodeEncryptedMessage(contacts_.identity(), *contact, packet, message, sizeof(message))) {
        setNotice("Message auth failed", ui::Tone::Danger);
        return;
    }
    history_.addIncoming(contact->nickname, message);
    setNotice("Encrypted message received", ui::Tone::Success);
}

void HardwareTestApp::pollInput()
{
    ui::KeyEvent event;
    if (!keySource_->poll(event)) {
        return;
    }
    handleKeyEvent(event);
}

const crypto_chat::Contact *HardwareTestApp::selectedContact() const
{
    return contacts_.contactAt(selectedContactIndex_);
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
    // Pointer identity keeps this correct when the layout gains or loses rows.
    const ui::Component *focused = uiPage_.focusedComponent();
    if (focused == nullptr) {
        return Action::None;
    }

    switch (page_) {
    case Page::Chat:
        if (focused == &inputBox_ || focused == &sendButton_) {
            return Action::Send;
        }
        if (focused == &contactsButton_) {
            return Action::OpenContacts;
        }
        if (focused == &inviteButton_) {
            return Action::OpenInvite;
        }
        if (focused == &identityButton_) {
            return Action::OpenIdentity;
        }
        break;

    case Page::Contacts:
        for (size_t i = 0; i < 4; i++) {
            if (focused == &contactItems_[i]) {
                return Action::SelectContact;
            }
        }
        if (focused == &selectButton_) {
            return Action::SelectContact;
        }
        if (focused == &nextButton_) {
            return Action::NextContact;
        }
        if (focused == &backButton_) {
            return Action::BackToChat;
        }
        break;

    case Page::Invite:
        if (focused == &broadcastButton_) {
            return Action::BroadcastInvite;
        }
        if (focused == &acceptButton_) {
            return Action::AcceptInvite;
        }
        if (focused == &backButton_) {
            return Action::BackToChat;
        }
        break;

    case Page::Identity:
        if (focused == &inputBox_) {
            return inputLength_ == 64 ? Action::ProvisionSeed : Action::SetNickname;
        }
        if (focused == &setNickButton_) {
            return Action::SetNickname;
        }
        if (focused == &seedButton_) {
            return Action::ProvisionSeed;
        }
        if (focused == &backButton_) {
            return Action::BackToChat;
        }
        break;

    case Page::Count:
        break;
    }
    return Action::None;
}

size_t HardwareTestApp::focusIndexForAction(Action action)
{
    // Resolve the widget a navigation action should land on, then let the page
    // report its current index.
    const ui::Component *target = nullptr;
    switch (page_) {
    case Page::Chat:
        if (action == Action::OpenContacts) {
            target = &contactsButton_;
        } else if (action == Action::OpenInvite) {
            target = &inviteButton_;
        } else if (action == Action::OpenIdentity) {
            target = &identityButton_;
        } else {
            target = &inputBox_;
        }
        break;
    case Page::Contacts:
        if (action == Action::NextContact) {
            target = &nextButton_;
        } else if (action == Action::BackToChat) {
            target = &backButton_;
        } else {
            target = &selectButton_;
        }
        break;
    case Page::Invite:
        if (action == Action::AcceptInvite) {
            target = &acceptButton_;
        } else if (action == Action::BackToChat) {
            target = &backButton_;
        } else {
            target = &broadcastButton_;
        }
        break;
    case Page::Identity:
        if (action == Action::BackToChat) {
            target = &backButton_;
        } else {
            target = &inputBox_;
        }
        break;
    case Page::Count:
        break;
    }

    if (target != nullptr) {
        // The page owns the index, so ask it to focus the widget and read back
        // the resolved index instead of duplicating layout knowledge here.
        uiPage_.focusComponent(target);
    }
    return uiPage_.focusedIndex();
}
