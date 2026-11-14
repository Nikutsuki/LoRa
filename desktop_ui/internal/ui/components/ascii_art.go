package components

import (
	"fmt"
	"strings"

	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/model"
)

func RenderKeycard(id model.DeviceIdentity, accentColor, mutedColor lipgloss.Color) string {
	accent := lipgloss.NewStyle().Foreground(accentColor)
	muted := lipgloss.NewStyle().Foreground(mutedColor)

	lines := []string{
		muted.Render("+-[ DEVICE IDENTITY ]-------------------------------+"),
		fmt.Sprintf("| Nickname    : %-36s|", accent.Render(id.Nickname)),
		fmt.Sprintf("| Device ID   : %-36s|", muted.Render(id.DeviceID)),
		fmt.Sprintf("| Fingerprint : %-36s|", accent.Render(id.Fingerprint)),
		muted.Render("+---------------------------------------------------+"),
	}

	return strings.Join(lines, "\n")
}

func RenderInviteBadge(contact *model.Contact, focusColor, mutedColor lipgloss.Color) string {
	if contact == nil {
		return ""
	}
	focus := lipgloss.NewStyle().Bold(true).Foreground(focusColor)
	muted := lipgloss.NewStyle().Foreground(mutedColor)

	lines := []string{
		focus.Render("+-[ PENDING INVITE ]--------------------------------+"),
		fmt.Sprintf("| Remote Node : %-36s|", focus.Render(contact.Nickname)),
		fmt.Sprintf("| Fingerprint : %-36s|", focus.Render(contact.Fingerprint)),
		fmt.Sprintf("| Status      : %-36s|", muted.Render(contact.Status)),
		focus.Render("+---------------------------------------------------+"),
	}

	return strings.Join(lines, "\n")
}
