package views

import (
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/theme"
	"github.com/user/lora-tui/internal/ui/components"
)

type InviteView struct {
	controller controller.Controller
	buttonIdx  int
	buttons    []components.Button
}

func NewInviteView(c controller.Controller) *InviteView {
	buttons := []components.Button{
		{Label: "Broadcast", Action: "broadcast"},
		{Label: "Accept", Action: "accept"},
		{Label: "Back", Action: "back"},
	}

	return &InviteView{
		controller: c,
		buttonIdx:  0,
		buttons:    buttons,
	}
}

func (v *InviteView) SetSize(w, h int) {}

func (v *InviteView) Update(msg tea.Msg) (tea.Cmd, string) {
	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "tab", "right":
			v.buttonIdx = (v.buttonIdx + 1) % len(v.buttons)
			return nil, ""

		case "left":
			if v.buttonIdx > 0 {
				v.buttonIdx--
			} else {
				v.buttonIdx = len(v.buttons) - 1
			}
			return nil, ""

		case "enter":
			return v.handleButtonAction(v.buttons[v.buttonIdx].Action)

		case "esc":
			return nil, "navigate:messages"
		}
	}

	return nil, ""
}

func (v *InviteView) handleButtonAction(action string) (tea.Cmd, string) {
	switch action {
	case "broadcast":
		v.controller.BroadcastInvite()
	case "accept":
		_, _ = v.controller.AcceptPendingInvite()
	case "back":
		return nil, "navigate:messages"
	}
	return nil, ""
}

func (v *InviteView) View(th theme.Theme) string {
	var lines []string

	lines = append(lines, th.HeaderTitle.Render("INVITE / PAIRING"))
	lines = append(lines, "")

	pending := v.controller.GetPendingInvite()
	if pending != nil {
		badge := components.RenderInviteBadge(pending, th.Focus, th.Muted)
		lines = append(lines, strings.Split(badge, "\n")...)
	} else {
		idleStyle := lipgloss.NewStyle().
			Background(th.Surface).
			Padding(1, 2).
			Width(ContentWidth)

		msg := th.NoticeInfo.Render("No pending invite.")
		lines = append(lines, idleStyle.Render(msg))
		lines = append(lines, "", "", "")
	}

	lines = append(lines, "")

	buttonBar := components.RenderButtonBar(v.buttons, v.buttonIdx, th)
	lines = append(lines, buttonBar)

	for len(lines) < ContentHeight {
		lines = append(lines, "")
	}
	if len(lines) > ContentHeight {
		lines = lines[:ContentHeight]
	}

	return strings.Join(lines, "\n")
}
