#include "UiController.hpp"

#include <cstdio>
#include <cstring>

#include "FileTransfer.hpp"
#include "RadioSession.hpp"

namespace
{

    template <size_t N>
    void copyText(char (&dst)[N], const char *src)
    {
        std::snprintf(dst, N, "%s", (src != nullptr) ? src : "");
    }

    size_t wrapText(const char *text, char segs[][40], size_t maxSegs, size_t maxLen)
    {
        if (text == nullptr || segs == nullptr || maxSegs == 0 || maxLen == 0)
        {
            return 0;
        }

        size_t segCount = 0;
        const char *start = text;

        while (*start != '\0' && segCount < maxSegs)
        {
            if (segCount > 0)
            {
                while (*start == ' ')
                {
                    start++;
                }
                if (*start == '\0')
                {
                    break;
                }
            }

            const size_t len = std::strlen(start);
            if (len <= maxLen)
            {
                std::strncpy(segs[segCount], start, sizeof(segs[segCount]) - 1);
                segs[segCount][sizeof(segs[segCount]) - 1] = '\0';
                segCount++;
                break;
            }

            size_t breakPos = maxLen;
            while (breakPos > 0 && start[breakPos] != ' ')
            {
                breakPos--;
            }
            if (breakPos == 0)
            {
                breakPos = maxLen;
            }

            const size_t copyLen = breakPos < sizeof(segs[segCount]) ? breakPos : sizeof(segs[segCount]) - 1;
            std::memcpy(segs[segCount], start, copyLen);
            segs[segCount][copyLen] = '\0';
            segCount++;

            start += breakPos;
        }

        return segCount;
    }

} // namespace

UiController::UiController(Ili9341Display &display,
                           CardKbKeyboard &keyboard,
                           proto::LinkSession &session)
    : session_(session),
      lcdScreen_(display, theme_),
      uartScreen_(session, ScreenWidth, ScreenHeight),
      cardKbInput_(keyboard),
      uartInput_(session)
{
    std::strncpy(notice_, "Starting", sizeof(notice_) - 1);
}

void UiController::init(bool hasCardKb)
{
    if (hasCardKb)
    {
        screen_ = &lcdScreen_;
        keySource_ = &cardKbInput_;
        setNotice("CardKB connected", ui::Tone::Success);
    }
    else
    {
        screen_ = &uartScreen_;
        keySource_ = &uartInput_;
    }
    uiDirty_ = true;
    fullRedraw_ = true;
}

void UiController::invalidate()
{
    uiDirty_ = true;
}

void UiController::requestFullRedraw()
{
    fullRedraw_ = true;
    uiDirty_ = true;
}

void UiController::requestUartResync()
{
    uartResyncNeeded_ = true;
    uiDirty_ = true;
}

void UiController::setNotice(const char *message, ui::Tone tone)
{
    if (message == nullptr)
    {
        return;
    }
    copyText(notice_, message);
    statusBar_.setTone(tone);
    statusBar_.setText(notice_);
    invalidate();
}

bool UiController::pollInput(const RadioSession &radio,
                             const FileTransfer &fileTransfer,
                             UiCommand &outCommand)
{
    outCommand = UiCommand{};
    if (keySource_ == nullptr)
    {
        return false;
    }

    ui::KeyEvent event;
    while (keySource_->poll(event))
    {
        if (handleKeyEvent(event, radio, fileTransfer, outCommand))
        {
            return true;
        }
    }
    return false;
}

void UiController::render(const RadioSession &radio, const FileTransfer &fileTransfer)
{
    const bool fileActive = fileTransfer.isActive();
    if (fileActive != filePopupWasActive_)
    {
        filePopupWasActive_ = fileActive;
        fullRedraw_ = true;
        uiDirty_ = true;
    }

    if (!uiDirty_ || screen_ == nullptr)
    {
        return;
    }

    preparePage(radio, fileTransfer);

    ui::ScreenModel model;
    model.setFullRedraw(fullRedraw_);
    uiPage_.describe(model, theme_);

    // Skip identical frames so radio events that do not change visible widgets
    // do not cause the SPI LCD to flicker.
    const bool localContentChanged = fullRedraw_ || !lastModel_.sameContent(model);
    if (localContentChanged)
    {
        screen_->present(model);
    }

    if (screen_ != &uartScreen_ && session_.hostSeen() && (localContentChanged || uartResyncNeeded_))
    {
        ui::ScreenModel hostModel = model;
        if (uartResyncNeeded_)
        {
            hostModel.setFullRedraw(true);
        }
        uartScreen_.present(hostModel);
        uartResyncNeeded_ = false;
    }

    if (!uartScreen_.lastFrameSent())
    {
        proto::NoticeMessage overflow;
        overflow.level = proto::NoticeLevel::Danger;
        overflow.text = "screen frame too large";
        session_.sendNotice(overflow);
    }

    lastModel_ = model;
    fullRedraw_ = false;
    uiDirty_ = false;
}

void UiController::setPage(Page page)
{
    page_ = page;
    const ui::Component *initialFocus = &inputBox_;
    if (page == Page::Contacts)
        initialFocus = &selectButton_;
    else if (page == Page::Invite)
        initialFocus = &broadcastButton_;
    uiPage_.focusComponent(initialFocus);
    requestFullRedraw();
}

void UiController::clearInput()
{
    std::memset(input_, 0, sizeof(input_));
    inputLength_ = 0;
    invalidate();
}

void UiController::preparePage(const RadioSession &radio, const FileTransfer &fileTransfer)
{
    prepareCommon(radio);

    if (fileTransfer.isActive())
    {
        prepareFilePopup(fileTransfer);
        return;
    }

    switch (page_)
    {
    case Page::Chat:
        prepareChatPage(radio);
        break;
    case Page::Contacts:
        prepareContactsPage(radio);
        break;
    case Page::Invite:
        prepareInvitePage(radio);
        break;
    case Page::Identity:
        prepareIdentityPage(radio);
        break;
    }
}

void UiController::prepareCommon(const RadioSession &radio)
{
    std::snprintf(headerText_, sizeof(headerText_), "User: %s", radio.identity().nickname);
    std::snprintf(headerRight_, sizeof(headerRight_), "433 MHz");
    headerBar_.setTitle(headerText_);
    headerBar_.setRight(headerRight_);
    statusBar_.setText(notice_);
}

void UiController::prepareFilePopup(const FileTransfer &fileTransfer)
{
    std::snprintf(filePopupTitle_, sizeof(filePopupTitle_), "%s: %s",
                  fileTransfer.isSending() ? "Sending" : "Receiving",
                  fileTransfer.filename());
    filePopupTitleLabel_.setText(filePopupTitle_);
    updateFileProgress(fileTransfer.bytesTransferred(), fileTransfer.fileSize());

    ui::Component *popupComponents[] = {
        &headerBar_,
        &filePopupTitleLabel_,
        &fileProgressBarLabel_,
        &fileProgressStatsLabel_,
        &fileCancelButton_,
        &statusBar_,
    };
    uiPage_.setComponents(popupComponents, sizeof(popupComponents) / sizeof(popupComponents[0]));
    uiPage_.focusComponent(&fileCancelButton_);
}

void UiController::prepareChatPage(const RadioSession &radio)
{
    copyText(pageTitle_, "Messages");
    const lora::Contact *contact = radio.selectedContact();
    if (contact == nullptr)
    {
        copyText(contactLine_, "No contact selected");
        copyText(contactMeta_, "Use Invite to pair a device");
    }
    else
    {
        char fingerprint[20];
        formatFingerprint(contact->fingerprint, fingerprint, sizeof(fingerprint));
        std::snprintf(contactLine_, sizeof(contactLine_), "%s", contact->nickname);
        std::snprintf(contactMeta_, sizeof(contactMeta_), "%s", fingerprint);
    }

    pageTitleLabel_.setText(pageTitle_);
    primaryLabel_.setText(contactLine_);
    primaryLabel_.setTone(ui::Tone::Accent);
    secondaryLabel_.setText(contactMeta_);

    rebuildHistoryRows(radio);

    std::snprintf(inputHint_, sizeof(inputHint_), "> %s", input_);
    inputBox_.setText(inputHint_);

    ui::Component *components[] = {
        &headerBar_,
        &headerDivider_,
        &pageTitleLabel_,
        &primaryLabel_,
        &secondaryLabel_,
        &historyLabels_[0],
        &historyLabels_[1],
        &historyLabels_[2],
        &historyLabels_[3],
        &historyLabels_[4],
        &historyLabels_[5],
        &inputBox_,
        &sendButton_,
        &contactsButton_,
        &inviteButton_,
        &identityButton_,
        &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void UiController::prepareContactsPage(const RadioSession &radio)
{
    copyText(pageTitle_, "Contacts");
    pageTitleLabel_.setText(pageTitle_);

    const size_t selectedIdx = radio.selectedContactIndex();
    const size_t firstVisible = selectedIdx >= VisibleContacts ? selectedIdx - (VisibleContacts - 1) : 0;
    for (size_t i = 0; i < VisibleContacts; i++)
    {
        const size_t contactIndex = firstVisible + i;
        const lora::Contact *contact = radio.contacts().contactAt(contactIndex);
        if (contact == nullptr)
        {
            copyText(contactPrimary_[i], "-");
            contactSecondary_[i][0] = '\0';
        }
        else
        {
            char fingerprint[20];
            formatFingerprint(contact->fingerprint, fingerprint, sizeof(fingerprint));
            copyText(contactPrimary_[i], contact->nickname);
            copyText(contactSecondary_[i], fingerprint);
        }
        contactItems_[i].setContent(contactPrimary_[i], contactSecondary_[i], contactIndex == selectedIdx);
    }

    ui::Component *components[] = {
        &headerBar_,
        &headerDivider_,
        &pageTitleLabel_,
        &contactItems_[0],
        &contactItems_[1],
        &contactItems_[2],
        &contactItems_[3],
        &selectButton_,
        &nextButton_,
        &backButton_,
        &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void UiController::prepareInvitePage(const RadioSession &radio)
{
    copyText(pageTitle_, "Invite (Password Pair)");
    pageTitleLabel_.setText(pageTitle_);

    if (radio.hasPendingInvite())
    {
        char fingerprint[20];
        formatFingerprint(radio.pendingInvite().fingerprint, fingerprint, sizeof(fingerprint));
        std::snprintf(inviteLine_, sizeof(inviteLine_), "Pending: %s", radio.pendingInvite().nickname);
        std::snprintf(inviteMeta_, sizeof(inviteMeta_), "Fingerprint: %s", fingerprint);
        primaryLabel_.setTone(ui::Tone::Focus);
    }
    else if (radio.hasPendingEncryptedInvite())
    {
        copyText(inviteLine_, "Pending: [Encrypted Invite]");
        copyText(inviteMeta_, "Enter password below & press Accept");
        primaryLabel_.setTone(ui::Tone::Accent);
    }
    else
    {
        copyText(inviteLine_, "No pending invite");
        copyText(inviteMeta_, "Enter password to broadcast/accept");
        primaryLabel_.setTone(ui::Tone::Muted);
    }
    primaryLabel_.setText(inviteLine_);
    inviteDetailLabel_.setText(inviteMeta_);

    std::snprintf(inputHint_, sizeof(inputHint_), "Pass: %s", input_);
    inputBox_.setText(inputHint_);

    ui::Component *components[] = {
        &headerBar_,
        &headerDivider_,
        &pageTitleLabel_,
        &primaryLabel_,
        &inviteDetailLabel_,
        &inputBox_,
        &broadcastButton_,
        &acceptButton_,
        &backButton_,
        &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void UiController::prepareIdentityPage(const RadioSession &radio)
{
    std::strncpy(pageTitle_, "Identity", sizeof(pageTitle_) - 1);
    char fingerprint[20];
    formatFingerprint(radio.identity().fingerprint, fingerprint, sizeof(fingerprint));
    std::snprintf(identityLine_, sizeof(identityLine_), "Nick: %s", radio.identity().nickname);
    std::snprintf(identityMeta_, sizeof(identityMeta_), "Fingerprint: %s", fingerprint);

    pageTitleLabel_.setText(pageTitle_);
    primaryLabel_.setText(identityLine_);
    primaryLabel_.setTone(ui::Tone::Accent);
    identityDetailLabel_.setText(identityMeta_);

    std::snprintf(inputHint_, sizeof(inputHint_), "> %s", input_);
    inputBox_.setText(inputHint_);

    ui::Component *components[] = {
        &headerBar_,
        &headerDivider_,
        &pageTitleLabel_,
        &primaryLabel_,
        &identityDetailLabel_,
        &inputBox_,
        &setNickButton_,
        &seedButton_,
        &backButton_,
        &statusBar_,
    };
    uiPage_.setComponents(components, sizeof(components) / sizeof(components[0]));
}

void UiController::rebuildHistoryRows(const RadioSession &radio)
{
    struct VisualLine
    {
        char text[80]{};
        ui::Tone tone = ui::Tone::Muted;
        uint16_t statusColor = 0;
    };

    VisualLine lines[VisibleHistoryRows]{};
    int currentVisualRow = static_cast<int>(VisibleHistoryRows) - 1;

    size_t linesToSkip = chatScrollOffset_;
    size_t totalVisualLines = 0;

    const lora::ChatHistory &history = radio.history();
    const size_t total = history.count();

    for (size_t i = 0; i < total; i++)
    {
        const size_t entryIndex = total - 1 - i;
        const lora::ChatEntry &entry = history.at(entryIndex);

        char fullText[160];
        if (entry.RSSI == 0 && entry.SNR == 0)
        {
            std::snprintf(fullText, sizeof(fullText), "%s [%s]", entry.text, "waiting for RSSI and SNR");
        }
        else
        {
            std::snprintf(fullText, sizeof(fullText), "%s [RSSI: %0.1fdBm, SNR: %0.1fdB]",
                          entry.text, entry.RSSI, entry.SNR);
        }

        ui::Tone tone = ui::Tone::Muted;
        if (entry.direction == lora::ChatEntry::Direction::Outgoing)
        {
            tone = ui::Tone::Success;
        }
        else if (entry.direction == lora::ChatEntry::Direction::Incoming)
        {
            tone = ui::Tone::Default;
        }

        uint16_t statusColor = 0;
        if (entry.direction == lora::ChatEntry::Direction::Outgoing)
        {
            switch (entry.status)
            {
            case lora::ChatEntry::Status::Sending:
                statusColor = Ili9341Display::Yellow;
                break;
            case lora::ChatEntry::Status::Delivered:
                statusColor = Ili9341Display::Green;
                break;
            case lora::ChatEntry::Status::Failed:
                statusColor = Ili9341Display::Red;
                break;
            default:
                break;
            }
        }

        char segs[MaxWrappedLines][40]{};
        const size_t segCount = wrapText(fullText, segs, MaxWrappedLines, WrapColumnWidth);

        totalVisualLines += segCount;

        // Fill visual lines from bottom to top so the newest message stays anchored at the bottom.
        for (int s = static_cast<int>(segCount) - 1; s >= 0 && currentVisualRow >= 0; s--)
        {
            if (linesToSkip > 0)
            {
                linesToSkip--;
                continue;
            }
            copyText(lines[currentVisualRow].text, segs[s]);
            lines[currentVisualRow].tone = tone;
            lines[currentVisualRow].statusColor = (s == static_cast<int>(segCount) - 1) ? statusColor : 0;
            currentVisualRow--;
        }
    }

    for (size_t i = 0; i < VisibleHistoryRows; i++)
    {
        copyText(historyRows_[i], lines[i].text);
        historyLabels_[i].setTone(lines[i].tone);
        historyLabels_[i].setText(historyRows_[i]);
        historyLabels_[i].setStatusColor(lines[i].statusColor);
    }

    maxChatScroll_ = (totalVisualLines > VisibleHistoryRows) ? (totalVisualLines - VisibleHistoryRows) : 0;
}

void UiController::updateFileProgress(uint32_t current, uint32_t total)
{
    int percent = (total > 0) ? static_cast<int>((current * 100ULL) / total) : 0;
    if (percent > 100)
    {
        percent = 100;
    }

    char bar[ProgressBarWidth + 4]{};
    bar[0] = '[';
    const int filled = (percent * ProgressBarWidth) / 100;
    for (int i = 0; i < ProgressBarWidth; i++)
    {
        bar[1 + i] = (i < filled) ? '=' : ' ';
    }
    bar[ProgressBarWidth + 1] = ']';
    bar[ProgressBarWidth + 2] = '\0';
    copyText(fileProgressBarText_, bar);
    fileProgressBarLabel_.setText(fileProgressBarText_);

    std::snprintf(fileProgressStats_, sizeof(fileProgressStats_), "%d%% (%lu / %lu B)",
                  percent, static_cast<unsigned long>(current), static_cast<unsigned long>(total));
    fileProgressStatsLabel_.setText(fileProgressStats_);
}

bool UiController::handleKeyEvent(ui::KeyEvent event,
                                  const RadioSession &radio,
                                  const FileTransfer &fileTransfer,
                                  UiCommand &outCommand)
{
    if (event.key == ui::Key::None)
    {
        return false;
    }

    if (fileTransfer.isActive())
    {
        if (event.key == ui::Key::Enter)
        {
            outCommand.type = UiCommand::Type::CancelFileTransfer;
            return true;
        }
        return false;
    }

    const bool textPage = (page_ == Page::Chat || page_ == Page::Identity || page_ == Page::Invite);

    if (event.key == ui::Key::Character)
    {
        if (textPage && inputLength_ < InputMax - 1)
        {
            input_[inputLength_++] = event.character;
            input_[inputLength_] = '\0';
            invalidate();
        }
        return false;
    }

    if (event.key == ui::Key::Backspace)
    {
        if (textPage && inputLength_ > 0)
        {
            input_[--inputLength_] = '\0';
            invalidate();
        }
        return false;
    }

    if (event.key == ui::Key::Enter)
    {
        if (page_ == Page::Contacts && uiPage_.focusedIndex() >= 1 && uiPage_.focusedIndex() <= 4)
        {
            const size_t selectedIdx = radio.selectedContactIndex();
            const size_t firstVisible = selectedIdx >= 4 ? selectedIdx - 3 : 0;
            const size_t chosen = firstVisible + uiPage_.focusedIndex() - 1;
            if (chosen < radio.contacts().contactCount())
            {
                outCommand.type = UiCommand::Type::SelectContact;
                outCommand.contactIndex = chosen;
                setPage(Page::Chat);
                return true;
            }
        }
        return activateFocused(radio, outCommand);
    }

    if (page_ == Page::Chat && uiPage_.focusedComponent() == &inputBox_)
    {
        if (event.key == ui::Key::Up)
        {
            if (chatScrollOffset_ < maxChatScroll_)
            {
                chatScrollOffset_++;
                invalidate();
            }
            return false;
        }
        if (event.key == ui::Key::Down)
        {
            if (chatScrollOffset_ > 0)
            {
                chatScrollOffset_--;
                invalidate();
                return false;
            }
        }
    }

    uiPage_.handle(event);
    invalidate();
    return false;
}

bool UiController::activateFocused(const RadioSession &radio, UiCommand &outCommand)
{
    const ui::Component *focused = uiPage_.focusedComponent();
    if (focused == nullptr)
    {
        return false;
    }

    if (focused == &backButton_ || focused == &selectButton_)
    {
        setPage(Page::Chat);
        return false;
    }
    if (focused == &contactsButton_)
    {
        setPage(Page::Contacts);
        return false;
    }
    if (focused == &inviteButton_)
    {
        setPage(Page::Invite);
        return false;
    }
    if (focused == &identityButton_)
    {
        setPage(Page::Identity);
        return false;
    }
    if (focused == &nextButton_)
    {
        outCommand.type = UiCommand::Type::NextContact;
        return true;
    }
    if (focused == &broadcastButton_)
    {
        return emitTextCommand(UiCommand::Type::SendInvite, input_, false, outCommand);
    }
    if (focused == &acceptButton_)
    {
        return emitTextCommand(UiCommand::Type::AcceptInvite, input_, true, outCommand);
    }
    if (focused == &setNickButton_)
    {
        return emitTextCommand(UiCommand::Type::SetNickname, input_, true, outCommand);
    }
    if (focused == &seedButton_)
    {
        return emitTextCommand(UiCommand::Type::ProvisionSeed, input_, true, outCommand);
    }

    if (page_ == Page::Contacts)
    {
        for (const auto &item : contactItems_)
        {
            if (focused == &item)
            {
                setPage(Page::Chat);
                return false;
            }
        }
    }

    if (page_ == Page::Chat && (focused == &inputBox_ || focused == &sendButton_))
    {
        if (inputLength_ == 0 || radio.radio().isBusy())
        {
            return false;
        }
        return emitTextCommand(UiCommand::Type::SendText, input_, true, outCommand);
    }

    return false;
}

bool UiController::emitTextCommand(UiCommand::Type type,
                                   const char *source,
                                   bool clear,
                                   UiCommand &outCommand)
{
    copyText(commandText_, source);
    if (clear)
    {
        clearInput();
    }
    outCommand.type = type;
    outCommand.text = commandText_;
    return true;
}

void UiController::formatFingerprint(const uint8_t fingerprint[lora::FingerprintSize],
                                     char *out,
                                     size_t outSize)
{
    if (outSize == 0)
    {
        return;
    }
    static constexpr char hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    for (size_t i = 0; i < lora::FingerprintSize && pos + 2 < outSize; i++)
    {
        out[pos++] = hex[fingerprint[i] >> 4];
        out[pos++] = hex[fingerprint[i] & 0x0f];
    }
    out[pos] = '\0';
}
