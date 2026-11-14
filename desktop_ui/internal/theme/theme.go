package theme

import "github.com/charmbracelet/lipgloss"

type Theme struct {
	Background lipgloss.Color
	Surface    lipgloss.Color
	SurfaceAlt lipgloss.Color
	Text       lipgloss.Color
	Muted      lipgloss.Color
	Accent     lipgloss.Color
	Focus      lipgloss.Color
	Danger     lipgloss.Color
	Success    lipgloss.Color

	HeaderTitle  lipgloss.Style
	TabActive    lipgloss.Style
	TabInactive  lipgloss.Style
	ButtonNormal lipgloss.Style
	ButtonFocus  lipgloss.Style
	NoticeInfo   lipgloss.Style
	NoticeOk     lipgloss.Style
	NoticeWarn   lipgloss.Style
	NoticeDanger lipgloss.Style
}

var Default = buildTheme()

func buildTheme() Theme {
	t := Theme{
		Background: lipgloss.Color("#0A0E17"),
		Surface:    lipgloss.Color("#161F30"),
		SurfaceAlt: lipgloss.Color("#0D253F"),
		Text:       lipgloss.Color("#E2E8F0"),
		Muted:      lipgloss.Color("#64748B"),
		Accent:     lipgloss.Color("#00E5FF"),
		Focus:      lipgloss.Color("#FFD600"),
		Danger:     lipgloss.Color("#FF3366"),
		Success:    lipgloss.Color("#00E676"),
	}

	t.HeaderTitle = lipgloss.NewStyle().
		Bold(true).
		Foreground(t.Accent).
		Background(t.SurfaceAlt).
		Padding(0, 1)

	t.TabActive = lipgloss.NewStyle().
		Bold(true).
		Foreground(t.Focus).
		Background(t.SurfaceAlt).
		Padding(0, 1)

	t.TabInactive = lipgloss.NewStyle().
		Foreground(t.Muted).
		Background(t.Surface).
		Padding(0, 1)

	t.ButtonNormal = lipgloss.NewStyle().
		Foreground(t.Text).
		Background(t.SurfaceAlt).
		Padding(0, 1)

	t.ButtonFocus = lipgloss.NewStyle().
		Bold(true).
		Foreground(t.Background).
		Background(t.Focus).
		Padding(0, 1)

	t.NoticeInfo = lipgloss.NewStyle().Foreground(t.Muted)
	t.NoticeOk = lipgloss.NewStyle().Bold(true).Foreground(t.Success)
	t.NoticeWarn = lipgloss.NewStyle().Bold(true).Foreground(t.Focus)
	t.NoticeDanger = lipgloss.NewStyle().Bold(true).Foreground(t.Danger)

	return t
}
