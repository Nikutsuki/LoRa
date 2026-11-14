package views

import (
	"strings"

	"github.com/charmbracelet/bubbles/textinput"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/theme"
	"github.com/user/lora-tui/internal/ui/components"
)

type SettingsFocus int

const (
	FocusNickInput SettingsFocus = iota
	FocusSettingsButtons
)

type SettingsView struct {
	controller controller.Controller
	nickInput  textinput.Model
	focus      SettingsFocus
	buttonIdx  int
	buttons    []components.Button
}

func NewSettingsView(c controller.Controller) *SettingsView {
	id := c.GetIdentity()

	ti := textinput.New()
	ti.Prompt = ""
	ti.SetValue(id.Nickname)
	ti.Placeholder = "Nickname..."
	ti.CharLimit = 16
	ti.Width = 0

	ti.TextStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Text)
	ti.PlaceholderStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Muted)
	ti.Cursor.Style = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Focus)
	ti.Cursor.TextStyle = lipgloss.NewStyle().Background(theme.Default.Surface).Foreground(theme.Default.Text)

	buttons := []components.Button{
		{Label: "Set Nick", Action: "set_nick"},
		{Label: "Back", Action: "back"},
	}

	return &SettingsView{
		controller: c,
		nickInput:  ti,
		focus:      FocusNickInput,
		buttonIdx:  0,
		buttons:    buttons,
	}
}

func (v *SettingsView) SetSize(w, h int) {}

func (v *SettingsView) Update(msg tea.Msg) (tea.Cmd, string) {
	var cmd tea.Cmd

	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "tab":
			if v.focus == FocusNickInput {
				v.focus = FocusSettingsButtons
				v.nickInput.Blur()
			} else {
				v.focus = FocusNickInput
				v.nickInput.Focus()
			}
			return nil, ""

		case "down":
			if v.focus == FocusNickInput {
				v.focus = FocusSettingsButtons
				v.nickInput.Blur()
				return nil, ""
			}

		case "up":
			if v.focus == FocusSettingsButtons {
				v.focus = FocusNickInput
				v.nickInput.Focus()
				return nil, ""
			}

		case "left":
			if v.focus == FocusSettingsButtons {
				if v.buttonIdx > 0 {
					v.buttonIdx--
				} else {
					v.buttonIdx = len(v.buttons) - 1
				}
			}
			return nil, ""

		case "right":
			if v.focus == FocusSettingsButtons {
				v.buttonIdx = (v.buttonIdx + 1) % len(v.buttons)
			}
			return nil, ""

		case "enter":
			if v.focus == FocusSettingsButtons {
				return v.handleButtonAction(v.buttons[v.buttonIdx].Action)
			}
			return v.handleButtonAction("set_nick")

		case "esc":
			return nil, "navigate:messages"
		}
	}

	if v.focus == FocusNickInput {
		v.nickInput, cmd = v.nickInput.Update(msg)
	}

	return cmd, ""
}

func (v *SettingsView) handleButtonAction(action string) (tea.Cmd, string) {
	switch action {
	case "set_nick":
		nick := strings.TrimSpace(v.nickInput.Value())
		if nick != "" {
			_ = v.controller.SetNickname(nick)
		}
	case "back":
		return nil, "navigate:messages"
	}
	return nil, ""
}

func (v *SettingsView) View(th theme.Theme) string {
	var lines []string
	id := v.controller.GetIdentity()

	lines = append(lines, th.HeaderTitle.Render("SETTINGS"))

	keycard := components.RenderKeycard(id, th.Accent, th.Muted)
	lines = append(lines, strings.Split(keycard, "\n")...)

	lines = append(lines, "")

	bgStyle := lipgloss.NewStyle().Background(th.Surface)
	nickPrompt := bgStyle.Foreground(th.Muted).Render("  Nickname: ")
	if v.focus == FocusNickInput {
		nickPrompt = bgStyle.Foreground(th.Focus).Render("> Nickname: ")
	}
	v.nickInput.TextStyle = bgStyle.Foreground(th.Text)
	v.nickInput.PlaceholderStyle = bgStyle.Foreground(th.Muted)
	v.nickInput.Cursor.Style = bgStyle.Foreground(th.Focus)
	v.nickInput.Cursor.TextStyle = bgStyle.Foreground(th.Text)

	nickRow := bgStyle.Width(ContentWidth).Render(nickPrompt + v.nickInput.View())
	lines = append(lines, nickRow)

	lines = append(lines, "")

	buttonFocusIdx := -1
	if v.focus == FocusSettingsButtons {
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
