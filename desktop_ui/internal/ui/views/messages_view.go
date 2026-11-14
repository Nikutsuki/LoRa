package views

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/theme"
	"github.com/user/lora-tui/internal/ui/components"
)

type MessagesFocus int

const (
	FocusInput MessagesFocus = iota
	FocusButtons
	FocusChat
)

const (
	ContentWidth  = 80
	ContentHeight = 21
	ChatLines     = 14
)

type MessagesView struct {
	controller   controller.Controller
	textInput    textinput.Model
	focus        MessagesFocus
	buttonIdx    int
	buttons      []components.Button
	scrollOffset int
}

func NewMessagesView(c controller.Controller) *MessagesView {
	ti := textinput.New()
	ti.Prompt = "> "
	ti.Placeholder = "Type message..."
	ti.Focus()
	ti.CharLimit = ContentWidth - 4
	ti.Width = 0

	ti.PromptStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Focus)
	ti.TextStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Text)
	ti.PlaceholderStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Muted)
	ti.Cursor.Style = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Focus)
	ti.Cursor.TextStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Text)

	buttons := []components.Button{
		{Label: "Send", Action: "send"},
		{Label: "Contacts", Action: "contacts"},
		{Label: "Invite", Action: "invite"},
		{Label: "Settings", Action: "settings"},
	}

	return &MessagesView{
		controller:   c,
		textInput:    ti,
		focus:        FocusInput,
		buttonIdx:    0,
		buttons:      buttons,
		scrollOffset: 0,
	}
}

func (v *MessagesView) SetSize(w, h int) {}

func (v *MessagesView) Update(msg tea.Msg) (tea.Cmd, string) {
	var cmd tea.Cmd

	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "tab":
			if v.focus == FocusInput {
				v.focus = FocusButtons
				v.textInput.Blur()
			} else {
				v.focus = FocusInput
				v.textInput.Focus()
			}
			return nil, ""

		case "down":
			if v.focus == FocusChat {
				if v.scrollOffset > 0 {
					v.scrollOffset--
				} else {
					v.focus = FocusInput
					v.textInput.Focus()
				}
				return nil, ""
			}
			if v.focus == FocusInput {
				v.focus = FocusButtons
				v.textInput.Blur()
				return nil, ""
			}

		case "up":
			if v.focus == FocusButtons {
				v.focus = FocusInput
				v.textInput.Focus()
				return nil, ""
			}
			if v.focus == FocusInput {
				contact := v.controller.GetSelectedContact()
				if contact != nil {
					msgs := v.controller.GetMessagesForContact(contact.ID)
					if len(msgs) > ChatLines {
						v.focus = FocusChat
						v.textInput.Blur()
						v.scrollOffset = 1
						return nil, ""
					}
				}
			}
			if v.focus == FocusChat {
				contact := v.controller.GetSelectedContact()
				if contact != nil {
					msgs := v.controller.GetMessagesForContact(contact.ID)
					maxOffset := len(msgs) - ChatLines
					if v.scrollOffset < maxOffset {
						v.scrollOffset++
					}
				}
				return nil, ""
			}

		case "pgup":
			if v.focus == FocusChat || v.focus == FocusInput {
				v.focus = FocusChat
				v.textInput.Blur()
				contact := v.controller.GetSelectedContact()
				if contact != nil {
					msgs := v.controller.GetMessagesForContact(contact.ID)
					maxOffset := max(0, len(msgs)-ChatLines)
					v.scrollOffset = min(maxOffset, v.scrollOffset+5)
				}
				return nil, ""
			}

		case "pgdown":
			if v.focus == FocusChat {
				if v.scrollOffset > 5 {
					v.scrollOffset -= 5
				} else {
					v.scrollOffset = 0
					v.focus = FocusInput
					v.textInput.Focus()
				}
				return nil, ""
			}

		case "left":
			if v.focus == FocusButtons {
				if v.buttonIdx > 0 {
					v.buttonIdx--
				} else {
					v.buttonIdx = len(v.buttons) - 1
				}
				return nil, ""
			}

		case "right":
			if v.focus == FocusButtons {
				v.buttonIdx = (v.buttonIdx + 1) % len(v.buttons)
				return nil, ""
			}

		case "enter":
			if v.focus == FocusButtons {
				return v.handleButtonAction(v.buttons[v.buttonIdx].Action)
			}
			return v.sendMessage()

		case "esc":
			if v.focus == FocusChat {
				v.focus = FocusInput
				v.textInput.Focus()
				v.scrollOffset = 0
				return nil, ""
			}
		}
	}

	if v.focus == FocusInput {
		v.textInput, cmd = v.textInput.Update(msg)
	}

	return cmd, ""
}

func (v *MessagesView) sendMessage() (tea.Cmd, string) {
	text := v.textInput.Value()
	if strings.TrimSpace(text) == "" {
		return nil, ""
	}

	_, _ = v.controller.SendMessage(text)
	v.textInput.SetValue("")
	v.scrollOffset = 0
	return nil, ""
}

func (v *MessagesView) handleButtonAction(action string) (tea.Cmd, string) {
	switch action {
	case "send":
		return v.sendMessage()
	case "contacts":
		return nil, "navigate:contacts"
	case "invite":
		return nil, "navigate:invite"
	case "settings":
		return nil, "navigate:settings"
	}
	return nil, ""
}

func (v *MessagesView) View(th theme.Theme) string {
	var lines []string

	contact := v.controller.GetSelectedContact()
	if contact == nil {
		lines = append(lines, th.NoticeWarn.Render("No contact selected"))
		lines = append(lines, th.NoticeInfo.Render("Select a contact from Contacts tab"))
	} else {
		lines = append(lines, th.HeaderTitle.Render(contact.Nickname))
		lines = append(lines, th.NoticeInfo.Render(contact.Fingerprint+"  |  "+contact.Status))
	}

	var formattedMsgs []string
	if contact != nil {
		rawMsgs := v.controller.GetMessagesForContact(contact.ID)
		for _, m := range rawMsgs {
			timeStr := m.Timestamp.Format("15:04")
			line := fmt.Sprintf("[%s] <%s> %s", timeStr, m.SenderNick, m.Text)
			formattedMsgs = append(formattedMsgs, line)
		}
	}

	total := len(formattedMsgs)
	var visibleChat []string

	if total <= ChatLines {
		padding := ChatLines - total
		for i := 0; i < padding; i++ {
			visibleChat = append(visibleChat, "")
		}
		visibleChat = append(visibleChat, formattedMsgs...)
	} else {
		end := total - v.scrollOffset
		if end > total {
			end = total
		}
		start := end - ChatLines
		if start < 0 {
			start = 0
			end = ChatLines
		}
		visibleChat = formattedMsgs[start:end]

		if v.scrollOffset > 0 && len(visibleChat) > 0 {
			visibleChat[0] = th.NoticeWarn.Render(fmt.Sprintf("-- scroll: %d earlier --", v.scrollOffset))
		}
	}

	for i := 0; i < ChatLines; i++ {
		line := ""
		if i < len(visibleChat) {
			line = visibleChat[i]
		}
		lines = append(lines, line)
	}

	bgStyle := lipgloss.NewStyle().Background(th.Surface)

	if v.focus == FocusInput {
		v.textInput.Prompt = "> "
		v.textInput.PromptStyle = bgStyle.Foreground(th.Focus)
	} else if v.focus == FocusChat {
		v.textInput.Prompt = "(scroll) "
		v.textInput.PromptStyle = bgStyle.Foreground(th.Muted)
	} else {
		v.textInput.Prompt = "> "
		v.textInput.PromptStyle = bgStyle.Foreground(th.Muted)
	}

	v.textInput.TextStyle = bgStyle.Foreground(th.Text)
	v.textInput.PlaceholderStyle = bgStyle.Foreground(th.Muted)
	v.textInput.Cursor.Style = bgStyle.Foreground(th.Focus)
	v.textInput.Cursor.TextStyle = bgStyle.Foreground(th.Text)

	inputRow := bgStyle.Width(ContentWidth).Render(v.textInput.View())
	lines = append(lines, inputRow)

	buttonFocusIdx := -1
	if v.focus == FocusButtons {
		buttonFocusIdx = v.buttonIdx
	}
	buttonBar := components.RenderButtonBar(v.buttons, buttonFocusIdx, th)
	lines = append(lines, buttonBar)

	for len(lines) < ContentHeight {
		lines = append(lines, "")
	}
	if len(lines) > ContentHeight {
		lines = lines[:ContentHeight]
	}

	return strings.Join(lines, "\n")
}
