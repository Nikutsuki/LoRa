package views

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/model"
	"github.com/user/lora-tui/internal/theme"
	"github.com/user/lora-tui/internal/ui/components"
)

type ContactsFocus int

const (
	FocusList ContactsFocus = iota
	FocusContactButtons
	FocusAddModal
)

const (
	VisibleContactSlots = 7
)

type ContactsView struct {
	controller controller.Controller
	cursor     int
	topIndex   int
	focus      ContactsFocus
	buttonIdx  int
	buttons    []components.Button

	showAddModal bool
	modalField   int
	nickInput    textinput.Model
	fpInput      textinput.Model
}

func NewContactsView(c controller.Controller) *ContactsView {
	buttons := []components.Button{
		{Label: "Select", Action: "select"},
		{Label: "Next", Action: "next"},
		{Label: "Add", Action: "add"},
		{Label: "Delete", Action: "delete"},
		{Label: "Back", Action: "back"},
	}

	nickInput := textinput.New()
	nickInput.Prompt = ""
	nickInput.Placeholder = "Nickname (e.g. Delta)"
	nickInput.CharLimit = 16

	fpInput := textinput.New()
	fpInput.Prompt = ""
	fpInput.Placeholder = "Fingerprint (or empty for auto)"
	fpInput.CharLimit = 32

	return &ContactsView{
		controller: c,
		cursor:     0,
		topIndex:   0,
		focus:      FocusList,
		buttonIdx:  0,
		buttons:    buttons,
		nickInput:  nickInput,
		fpInput:    fpInput,
	}
}

func (v *ContactsView) SetSize(w, h int) {}

func (v *ContactsView) Update(msg tea.Msg) (tea.Cmd, string) {
	var cmd tea.Cmd

	if v.showAddModal {
		return v.updateAddModal(msg)
	}

	contacts := v.controller.GetContacts()

	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "tab":
			if v.focus == FocusList {
				v.focus = FocusContactButtons
			} else {
				v.focus = FocusList
			}
			return nil, ""

		case "up", "k":
			if v.focus == FocusContactButtons {
				v.focus = FocusList
				return nil, ""
			}
			if v.focus == FocusList && v.cursor > 0 {
				v.cursor--
				if v.cursor < v.topIndex {
					v.topIndex = v.cursor
				}
				v.controller.SelectContactByIndex(v.cursor)
			}
			return nil, ""

		case "down", "j":
			if v.focus == FocusList {
				if v.cursor < len(contacts)-1 {
					v.cursor++
					if v.cursor >= v.topIndex+VisibleContactSlots {
						v.topIndex = v.cursor - VisibleContactSlots + 1
					}
					v.controller.SelectContactByIndex(v.cursor)
				} else {
					v.focus = FocusContactButtons
				}
				return nil, ""
			}

		case "left":
			if v.focus == FocusContactButtons {
				if v.buttonIdx > 0 {
					v.buttonIdx--
				} else {
					v.buttonIdx = len(v.buttons) - 1
				}
			}
			return nil, ""

		case "right":
			if v.focus == FocusContactButtons {
				v.buttonIdx = (v.buttonIdx + 1) % len(v.buttons)
			}
			return nil, ""

		case "enter":
			if v.focus == FocusContactButtons {
				return v.handleButtonAction(v.buttons[v.buttonIdx].Action)
			}
			if len(contacts) > 0 && v.cursor < len(contacts) {
				v.controller.SelectContactByIndex(v.cursor)
				return nil, "navigate:messages"
			}

		case "esc":
			return nil, "navigate:messages"
		}
	}

	return cmd, ""
}

func (v *ContactsView) updateAddModal(msg tea.Msg) (tea.Cmd, string) {
	var cmd tea.Cmd
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "tab", "down":
			if v.modalField == 0 {
				v.modalField = 1
				v.nickInput.Blur()
				v.fpInput.Focus()
			} else {
				v.modalField = 0
				v.fpInput.Blur()
				v.nickInput.Focus()
			}
			return nil, ""

		case "up":
			if v.modalField == 1 {
				v.modalField = 0
				v.fpInput.Blur()
				v.nickInput.Focus()
			}
			return nil, ""

		case "enter":
			nick := v.nickInput.Value()
			fp := v.fpInput.Value()
			if strings.TrimSpace(nick) != "" {
				if strings.TrimSpace(fp) == "" {
					fp = model.GenerateRandomFingerprint()
				}
				_, err := v.controller.AddContact(nick, fp)
				if err == nil {
					v.showAddModal = false
					v.focus = FocusList
					v.nickInput.Reset()
					v.fpInput.Reset()
				}
			}
			return nil, ""

		case "esc":
			v.showAddModal = false
			v.focus = FocusList
			v.nickInput.Reset()
			v.fpInput.Reset()
			return nil, ""
		}
	}

	if v.modalField == 0 {
		v.nickInput, cmd = v.nickInput.Update(msg)
	} else {
		v.fpInput, cmd = v.fpInput.Update(msg)
	}

	return cmd, ""
}

func (v *ContactsView) handleButtonAction(action string) (tea.Cmd, string) {
	contacts := v.controller.GetContacts()

	switch action {
	case "select":
		if len(contacts) > 0 && v.cursor < len(contacts) {
			v.controller.SelectContactByIndex(v.cursor)
			return nil, "navigate:messages"
		}
	case "next":
		v.controller.SelectNextContact()
		if len(contacts) > 0 {
			v.cursor = (v.cursor + 1) % len(contacts)
		}
	case "add":
		v.showAddModal = true
		v.modalField = 0
		v.nickInput.Reset()
		v.fpInput.Reset()
		v.nickInput.Focus()
		v.fpInput.Blur()
	case "delete":
		if len(contacts) > 0 && v.cursor < len(contacts) {
			_ = v.controller.DeleteContact(contacts[v.cursor].ID)
			if v.cursor >= len(v.controller.GetContacts()) && v.cursor > 0 {
				v.cursor--
			}
		}
	case "back":
		return nil, "navigate:messages"
	}
	return nil, ""
}

func (v *ContactsView) View(th theme.Theme) string {
	if v.showAddModal {
		return v.renderAddModal(th)
	}

	var lines []string
	contacts := v.controller.GetContacts()

	lines = append(lines, th.HeaderTitle.Render("CONTACTS"))
	lines = append(lines, th.NoticeInfo.Render(fmt.Sprintf("%d contacts", len(contacts))))

	var contactLines []string
	start := v.topIndex
	end := min(len(contacts), start+VisibleContactSlots)

	for i := start; i < end; i++ {
		c := contacts[i]
		isSelected := (i == v.cursor)

		var prefix string
		var nickStyle lipgloss.Style
		if isSelected && v.focus == FocusList {
			prefix = "> "
			nickStyle = th.NoticeWarn
		} else if isSelected {
			prefix = "* "
			nickStyle = th.HeaderTitle
		} else {
			prefix = "  "
			nickStyle = th.HeaderTitle
		}

		line1 := nickStyle.Render(prefix + c.Nickname)
		line2 := th.NoticeInfo.Render(fmt.Sprintf("    %s  |  %s", c.Fingerprint, c.Status))
		contactLines = append(contactLines, line1, line2)
	}

	for len(contactLines) < 14 {
		contactLines = append(contactLines, "")
	}
	if len(contactLines) > 14 {
		contactLines = contactLines[:14]
	}
	lines = append(lines, contactLines...)

	buttonFocusIdx := -1
	if v.focus == FocusContactButtons {
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

func (v *ContactsView) renderAddModal(th theme.Theme) string {
	var lines []string

	lines = append(lines, th.NoticeWarn.Render("ADD NEW CONTACT"))
	lines = append(lines, "")

	var nickLabel, fpLabel string
	if v.modalField == 0 {
		nickLabel = th.NoticeWarn.Render("> Nickname: ")
		fpLabel = th.NoticeInfo.Render("  Fingerprint: ")
	} else {
		nickLabel = th.NoticeInfo.Render("  Nickname: ")
		fpLabel = th.NoticeWarn.Render("> Fingerprint: ")
	}

	lines = append(lines, nickLabel+v.nickInput.View())
	lines = append(lines, fpLabel+v.fpInput.View())
	lines = append(lines, "")
	lines = append(lines, th.NoticeInfo.Render("[Enter]: Save  |  [Esc]: Cancel"))

	for len(lines) < ContentHeight {
		lines = append(lines, "")
	}
	if len(lines) > ContentHeight {
		lines = lines[:ContentHeight]
	}

	return strings.Join(lines, "\n")
}
