package components

import (
	"strings"

	"github.com/user/lora-tui/internal/theme"
)

type Button struct {
	Label  string
	Action string
}

func RenderButtonBar(buttons []Button, focusedIndex int, th theme.Theme) string {
	var rendered []string
	for i, btn := range buttons {
		if i == focusedIndex {
			rendered = append(rendered, th.ButtonFocus.Render("[ "+btn.Label+" ]"))
		} else {
			rendered = append(rendered, th.ButtonNormal.Render("  "+btn.Label+"  "))
		}
	}
	return strings.Join(rendered, " ")
}
