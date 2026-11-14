package components

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/model"
	"github.com/user/lora-tui/internal/theme"
)

type TabInfo struct {
	Index int
	Title string
	Key   string
}

var Tabs = []TabInfo{
	{Index: 0, Title: "Messages", Key: "1"},
	{Index: 1, Title: "Contacts", Key: "2"},
	{Index: 2, Title: "Settings", Key: "3"},
	{Index: 3, Title: "Invite", Key: "4"},
}

func RenderHeader(activeTab int, identity model.DeviceIdentity, width int, th theme.Theme) string {
	topBar := lipgloss.NewStyle().
		Bold(true).
		Foreground(th.Accent).
		Background(th.SurfaceAlt).
		Width(width).
		Padding(0, 1).
		Render(fmt.Sprintf("MESSENGER  |  User: %s", identity.Nickname))

	var renderedTabs []string
	for _, t := range Tabs {
		label := fmt.Sprintf("[%s: %s]", t.Key, t.Title)
		if t.Index == activeTab {
			renderedTabs = append(renderedTabs, th.TabActive.Render(label))
		} else {
			renderedTabs = append(renderedTabs, th.TabInactive.Render(label))
		}
	}
	tabsLine := strings.Join(renderedTabs, " ")

	return lipgloss.JoinVertical(lipgloss.Left, topBar, tabsLine)
}
