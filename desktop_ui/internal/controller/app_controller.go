package controller

import (
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/user/lora-tui/internal/model"
)

type NoticeLevel int

const (
	NoticeInfo NoticeLevel = iota
	NoticeSuccess
	NoticeWarning
	NoticeDanger
)

type Notice struct {
	Text      string
	Level     NoticeLevel
	Timestamp time.Time
}

type Controller interface {
	GetIdentity() model.DeviceIdentity
	SetNickname(nickname string) error

	GetContacts() []*model.Contact
	GetSelectedContact() *model.Contact
	SelectContactByIndex(index int)
	SelectNextContact()
	AddContact(nickname, fingerprint string) (*model.Contact, error)
	DeleteContact(id string) error

	GetMessagesForContact(contactID string) []model.Message
	SendMessage(text string) (*model.Message, error)
	ClearChatHistory(contactID string)

	GetPendingInvite() *model.Contact
	BroadcastInvite() string
	AcceptPendingInvite() (*model.Contact, error)

	GetNotice() Notice
	SetNotice(text string, level NoticeLevel)

	Save() error
	GetStoragePath() string
}

type AppController struct {
	state       *model.AppState
	storage     *model.StorageManager
	selectedIdx int
	notice      Notice
	mu          sync.RWMutex
}

func NewAppController(storagePath string) (*AppController, error) {
	sm := model.NewStorageManager(storagePath)
	state, err := sm.Load()
	if err != nil {
		return nil, fmt.Errorf("controller error: %w", err)
	}

	c := &AppController{
		state:       state,
		storage:     sm,
		selectedIdx: 0,
		notice: Notice{
			Text:      "Ready",
			Level:     NoticeInfo,
			Timestamp: time.Now(),
		},
	}

	return c, nil
}

func (c *AppController) Close() {
	_ = c.Save()
}

func (c *AppController) GetIdentity() model.DeviceIdentity {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.state.Identity
}

func (c *AppController) SetNickname(nickname string) error {
	nickname = strings.TrimSpace(nickname)
	if nickname == "" {
		return errors.New("empty nickname")
	}
	if len(nickname) > 16 {
		nickname = nickname[:16]
	}

	c.mu.Lock()
	c.state.Identity.Nickname = nickname
	c.mu.Unlock()

	c.SetNotice("Nickname: "+nickname, NoticeSuccess)
	_ = c.Save()
	return nil
}

func (c *AppController) GetContacts() []*model.Contact {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.state.Contacts
}

func (c *AppController) GetSelectedContact() *model.Contact {
	c.mu.RLock()
	defer c.mu.RUnlock()
	if len(c.state.Contacts) == 0 {
		return nil
	}
	if c.selectedIdx >= len(c.state.Contacts) {
		c.selectedIdx = 0
	}
	return c.state.Contacts[c.selectedIdx]
}

func (c *AppController) SelectContactByIndex(index int) {
	c.mu.Lock()
	if len(c.state.Contacts) > 0 {
		if index < 0 {
			index = 0
		}
		if index >= len(c.state.Contacts) {
			index = len(c.state.Contacts) - 1
		}
		c.selectedIdx = index
	}
	c.mu.Unlock()

	contact := c.GetSelectedContact()
	if contact != nil {
		c.SetNotice("Selected "+contact.Nickname, NoticeInfo)
	}
}

func (c *AppController) SelectNextContact() {
	c.mu.Lock()
	if len(c.state.Contacts) > 0 {
		c.selectedIdx = (c.selectedIdx + 1) % len(c.state.Contacts)
	}
	c.mu.Unlock()

	contact := c.GetSelectedContact()
	if contact != nil {
		c.SetNotice("Switched to "+contact.Nickname, NoticeInfo)
	}
}

func (c *AppController) AddContact(nickname, fingerprint string) (*model.Contact, error) {
	nickname = strings.TrimSpace(nickname)
	if nickname == "" {
		return nil, errors.New("empty nickname")
	}
	if len(nickname) > 16 {
		nickname = nickname[:16]
	}

	fp := model.FormatFingerprint(fingerprint)

	c.mu.Lock()
	newContact := &model.Contact{
		ID:          fmt.Sprintf("contact-%d", time.Now().UnixNano()),
		Nickname:    nickname,
		DeviceID:    model.GenerateRandomDeviceID(),
		Fingerprint: fp,
		BoxPubKey:   model.GenerateRandomHex(32),
		Status:      "Online",
		LastSeen:    time.Now(),
		Valid:       true,
	}

	c.state.Contacts = append(c.state.Contacts, newContact)
	c.selectedIdx = len(c.state.Contacts) - 1
	c.mu.Unlock()

	c.SetNotice("Added "+nickname, NoticeSuccess)
	_ = c.Save()
	return newContact, nil
}

func (c *AppController) DeleteContact(id string) error {
	c.mu.Lock()
	found := false
	var updated []*model.Contact
	for _, ct := range c.state.Contacts {
		if ct.ID == id {
			found = true
			continue
		}
		updated = append(updated, ct)
	}

	if !found {
		c.mu.Unlock()
		return errors.New("not found")
	}

	c.state.Contacts = updated
	if c.selectedIdx >= len(c.state.Contacts) && len(c.state.Contacts) > 0 {
		c.selectedIdx = len(c.state.Contacts) - 1
	}
	c.mu.Unlock()

	c.SetNotice("Removed", NoticeWarning)
	_ = c.Save()
	return nil
}

func (c *AppController) GetMessagesForContact(contactID string) []model.Message {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.state.Messages[contactID]
}

func (c *AppController) SendMessage(text string) (*model.Message, error) {
	text = strings.TrimSpace(text)
	if text == "" {
		return nil, errors.New("empty text")
	}

	contact := c.GetSelectedContact()
	if contact == nil {
		c.SetNotice("No contact selected", NoticeDanger)
		return nil, errors.New("no contact")
	}

	c.mu.Lock()
	c.state.Sequence++
	seq := c.state.Sequence

	msg := model.Message{
		ID:         fmt.Sprintf("msg-%d", time.Now().UnixNano()),
		ContactID:  contact.ID,
		SenderNick: c.state.Identity.Nickname,
		IsOutgoing: true,
		Text:       text,
		Sequence:   seq,
		Timestamp:  time.Now(),
	}

	c.state.Messages[contact.ID] = append(c.state.Messages[contact.ID], msg)
	c.mu.Unlock()

	c.SetNotice("Sent to "+contact.Nickname, NoticeSuccess)
	_ = c.Save()

	return &msg, nil
}

func (c *AppController) ClearChatHistory(contactID string) {
	c.mu.Lock()
	delete(c.state.Messages, contactID)
	c.mu.Unlock()

	c.SetNotice("Cleared", NoticeInfo)
	_ = c.Save()
}

func (c *AppController) GetPendingInvite() *model.Contact {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.state.PendingInvite
}

func (c *AppController) BroadcastInvite() string {
	msg := "Invite broadcasted"
	c.SetNotice(msg, NoticeSuccess)
	return msg
}

func (c *AppController) AcceptPendingInvite() (*model.Contact, error) {
	c.mu.Lock()
	pending := c.state.PendingInvite
	if pending == nil {
		c.mu.Unlock()
		c.SetNotice("No pending invite", NoticeDanger)
		return nil, errors.New("no invite")
	}

	c.state.Contacts = append(c.state.Contacts, pending)
	c.selectedIdx = len(c.state.Contacts) - 1
	c.state.PendingInvite = nil
	c.mu.Unlock()

	c.SetNotice("Accepted "+pending.Nickname, NoticeSuccess)
	_ = c.Save()
	return pending, nil
}

func (c *AppController) GetNotice() Notice {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.notice
}

func (c *AppController) SetNotice(text string, level NoticeLevel) {
	c.mu.Lock()
	c.notice = Notice{
		Text:      text,
		Level:     level,
		Timestamp: time.Now(),
	}
	c.mu.Unlock()
}

func (c *AppController) Save() error {
	return c.storage.Save(c.state)
}

func (c *AppController) GetStoragePath() string {
	return c.storage.GetFilePath()
}
