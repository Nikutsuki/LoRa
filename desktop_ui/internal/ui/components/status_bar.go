package components

import (
	"github.com/charmbracelet/lipgloss"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/theme"
)

func RenderStatusBar(notice controller.Notice, width int, th theme.Theme) string {
	var noticeStyle lipgloss.Style
	switch notice.Level {
	case controller.NoticeSuccess:
		noticeStyle = th.NoticeOk
	case controller.NoticeWarning:
		noticeStyle = th.NoticeWarn
	case controller.NoticeDanger:
		noticeStyle = th.NoticeDanger
	default:
		noticeStyle = th.NoticeInfo
	}

	noticeText := notice.Text
	if noticeText == "" {
		noticeText = "Ready"
	}

	return lipgloss.NewStyle().
		Background(th.Surface).
		Width(width).
		Padding(0, 1).
		Render(noticeStyle.Render(noticeText))
}
