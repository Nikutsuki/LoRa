package ui

import (
	"fmt"
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/sqweek/dialog"
	"github.com/user/lora-tui/internal/device"
	"github.com/user/lora-tui/internal/theme"
)

// deviceEventMsg carries one session event into the Bubble Tea loop.
type deviceEventMsg device.Event

// deviceClosedMsg reports that the event channel was closed.
type deviceClosedMsg struct{}

// DeviceModel renders the screen a physical device is currently showing and
// forwards every keystroke back over the UART link.
//
// It deliberately owns no application logic: the device remains the single
// source of truth, so the terminal acts purely as a replacement for the LCD and
// the CardKB keyboard.
type DeviceModel struct {
	session *device.Session
	theme   theme.Theme

	screen    device.Screen
	notice    device.Notice
	connected bool
	status    string
	width     int
	height    int
	fileData  []byte
}

// connectingStatus is the placeholder shown until the device says something.
const connectingStatus = "Connecting to device..."

const (
	TotalWidth  = 80
	TotalHeight = 24
)

// NewDeviceModel builds a remote control view for an established session.
func NewDeviceModel(session *device.Session) *DeviceModel {
	return &DeviceModel{
		session: session,
		theme:   theme.Default,
		status:  connectingStatus,
		width:   TotalWidth,
		height:  TotalHeight,
	}
}

// New builds a remote control view for an established session.
func New(session *device.Session) *DeviceModel {
	return NewDeviceModel(session)
}

// Init subscribes to the session event stream.
func (m *DeviceModel) Init() tea.Cmd {
	return waitForDeviceEvent(m.session.Events())
}

func (m *DeviceModel) FileDialog() (string, error) {
	filePath, err := dialog.File().Title("Select a file").Load()
	if err != nil {
		return "", fmt.Errorf("file dialog: %w", err)
	}

	return filePath, nil
}

// Update reacts to keys and device events.
func (m *DeviceModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		if msg.Width > 0 {
			m.width = msg.Width
		}
		if msg.Height > 0 {
			m.height = msg.Height
		}
		return m, nil

	case tea.KeyMsg:
		if msg.String() == "ctrl+c" {
			return m, tea.Quit
		} else if msg.String() == "ctrl+u" {
			filePath, err := m.FileDialog()
			if err != nil {
				m.status = fmt.Sprintf("File dialog error: %v", err)
				return m, nil
			}

			go m.session.UploadFile(filePath)
		}
		code, character, ok := mapKey(msg)
		if !ok {
			return m, nil
		}
		return m, sendKeyCmd(m.session, code, character)

	case deviceEventMsg:
		m.applyEvent(device.Event(msg))
		return m, waitForDeviceEvent(m.session.Events())

	case deviceClosedMsg:
		m.connected = false
		m.status = "Device disconnected"
		return m, nil
	}

	return m, nil
}

func (m *DeviceModel) applyEvent(event device.Event) {
	switch event.Kind {
	case device.EventConnected:
		m.connected = true
		m.status = "Connected"

	case device.EventScreen:
		m.screen = event.Screen
		// A screen frame proves the link is live, so clear the connecting
		// placeholder. Without this the label stayed on "Connecting..." forever
		// because firmware that sends no Hello frame never produces
		// EventConnected.
		if !m.connected || m.status == connectingStatus {
			m.connected = true
			m.status = "Connected"
		}

	case device.EventNotice:
		m.notice = event.Notice

	case device.EventDisconnected:
		m.connected = false
		if event.Err != nil {
			m.status = "Disconnected: " + event.Err.Error()
		} else {
			m.status = "Disconnected"
		}
	}
}

// View renders the mirrored device screen.
func (m *DeviceModel) View() string {
	th := m.theme
	width := m.width

	header := m.renderHeader(th, width)
	status := m.renderStatus(th, width)

	// The header and status bar consume two of the terminal rows, so the body is
	// given what remains. Without this the composed view overflowed by one line
	// and truncation silently dropped the status bar.
	headerLines := strings.Split(header, "\n")
	statusLines := strings.Split(status, "\n")
	bodyBudget := m.height - len(headerLines) - len(statusLines)
	if bodyBudget < 1 {
		bodyBudget = 1
	}

	body := m.renderBody(th, width, bodyBudget)

	lines := append(headerLines, strings.Split(body, "\n")...)
	lines = append(lines, statusLines...)

	for len(lines) < m.height {
		lines = append(lines, "")
	}
	if len(lines) > m.height {
		lines = lines[:m.height]
	}

	return strings.Join(lines, "\n")
}

func (m *DeviceModel) renderHeader(th theme.Theme, width int) string {
	// The device sends its own header bar (title plus right hand text), so mirror
	// that rather than inventing a second title line.
	title := "MESSENGER"
	right := ""
	for _, c := range m.screen.Components {
		if c.Kind != device.KindLabel {
			continue
		}
		if c.Y != 0 {
			continue
		}
		if c.Primary != "" {
			title = c.Primary
		}
		right = c.Secondary
		break
	}

	badge := "○ offline"
	if m.connected {
		badge = "● online"
	}

	left := th.HeaderTitle.Render(title)
	if right != "" {
		left += "  " + th.NoticeInfo.Render(right)
	}
	return left + "  " + th.NoticeInfo.Render(badge)
}

func (m *DeviceModel) renderBody(th theme.Theme, width int, budget int) string {
	var lines []string

	// Labels render as text; list items and the text box render as interactive
	// rows; buttons collect into a bar at the bottom.
	//
	// The header bar and the status bar are drawn by View from dedicated helpers,
	// so they are skipped here. Rendering them again produced a duplicated header
	// and status line, which is what made the layout look broken.
	var buttonLine string

	for _, c := range m.screen.Components {
		switch c.Kind {
		case device.KindLabel:
			// The header is the first label anchored to the top of the screen.
			if c.Y == 0 {
				continue
			}
			if c.Primary == "" {
				continue
			}
			lines = append(lines, m.renderLabel(c, th))

		case device.KindListItem:
			lines = append(lines, m.renderListItem(c, th))

		case device.KindTextBox:
			lines = append(lines, m.renderTextbox(c, th))

		case device.KindButton:
			if buttonLine != "" {
				buttonLine += " "
			}
			buttonLine += m.renderButton(c, th)

		case device.KindDivider:
			lines = append(lines, th.NoticeInfo.Render(strings.Repeat("─", width)))

		case device.KindStatusBar:
			// Rendered by View.
		}
	}

	if len(lines) == 0 {
		lines = append(lines, th.NoticeInfo.Render("  waiting for device screen..."))
	}

	// Leave room inside the budget for the button bar so it pins to the bottom.
	reserved := 0
	if buttonLine != "" {
		reserved = 2 // blank spacer plus the bar
	}
	for len(lines) < budget-reserved {
		lines = append(lines, "")
	}
	if buttonLine != "" {
		lines = append(lines, "")
		lines = append(lines, buttonLine)
	}

	// A device screen with many rows can outgrow the terminal; clamp so the
	// caller's fixed height is always honoured.
	if len(lines) > budget {
		lines = lines[len(lines)-budget:]
	}

	return strings.Join(lines, "\n")
}

func (m *DeviceModel) renderLabel(c device.Component, th theme.Theme) string {
	style := m.toneStyle(c.Color, th)
	text := c.Primary
	if text == "" {
		text = " "
	}

	line := " " + style.Render(text)

	if c.Secondary != "" {
		line += "  " + th.NoticeInfo.Render(c.Secondary)
	}

	if c.StatusColor != 0 {
		dotStyle := m.toneStyle(c.StatusColor, th)
		line += "  " + dotStyle.Render("●")
	}

	return line
}

func (m *DeviceModel) renderListItem(c device.Component, th theme.Theme) string {
	prefix := "  "
	style := th.HeaderTitle
	if c.Focused {
		prefix = "> "
		style = th.NoticeWarn
	} else if c.Selected {
		prefix = "* "
	}

	line := style.Render(prefix + c.Primary)
	if c.Secondary != "" {
		line += "  " + th.NoticeInfo.Render(c.Secondary)
	}
	return line
}

func (m *DeviceModel) renderTextbox(c device.Component, th theme.Theme) string {
	// The firmware includes the "> " prompt in the text it sends, so it is
	// rendered verbatim; adding another prompt here would double it up.
	foreground := th.Text
	if c.Focused {
		foreground = th.Focus
	}

	box := lipgloss.NewStyle().
		Background(th.Surface).
		Foreground(foreground).
		Width(m.width - 6).
		Render(c.Primary)

	return "  " + box
}

func (m *DeviceModel) renderButton(c device.Component, th theme.Theme) string {
	// Focus is shown by inversion rather than brackets, which overflow the
	// device's fixed-size buttons.
	if c.Focused {
		return th.ButtonFocus.Render(" " + c.Primary + " ")
	}
	return th.ButtonNormal.Render("  " + c.Primary + "  ")
}

func (m *DeviceModel) renderStatus(th theme.Theme, width int) string {
	// Prefer the status bar the device itself sends, because that is what its own
	// screen shows; fall back to the notice stream and then the connection state.
	text := m.status
	level := device.NoticeInfo

	for _, c := range m.screen.Components {
		if c.Kind == device.KindStatusBar && c.Primary != "" {
			text = c.Primary
			level = statusLevelFor(c.Color)
			break
		}
	}

	if m.notice.Text != "" && !m.hasDeviceStatus() {
		text = m.notice.Text
		level = m.notice.Level
	}

	var style lipgloss.Style
	switch level {
	case device.NoticeSuccess:
		style = th.NoticeOk
	case device.NoticeDanger:
		style = th.NoticeDanger
	default:
		style = th.NoticeInfo
	}
	if !m.connected {
		style = th.NoticeDanger
		text = m.status
	}

	return lipgloss.NewStyle().
		Background(th.Surface).
		Width(width).
		Padding(0, 1).
		Render(style.Render(text))
}

// hasDeviceStatus reports whether the frame carries its own status bar.
func (m *DeviceModel) hasDeviceStatus() bool {
	for _, c := range m.screen.Components {
		if c.Kind == device.KindStatusBar && c.Primary != "" {
			return true
		}
	}
	return false
}

// statusLevelFor maps the device's RGB565 status colour onto a notice level.
func statusLevelFor(color uint16) device.NoticeLevel {
	switch color {
	case 0xF800:
		return device.NoticeDanger
	case 0x07E0:
		return device.NoticeSuccess
	default:
		return device.NoticeInfo
	}
}

// toneStyle maps the device's RGB565 colour onto a terminal colour.
func (m *DeviceModel) toneStyle(color uint16, th theme.Theme) lipgloss.Style {
	switch color {
	case 0x07FF:
		return lipgloss.NewStyle().Foreground(th.Accent)
	case 0xFFE0:
		return lipgloss.NewStyle().Bold(true).Foreground(th.Focus)
	case 0xF800:
		return lipgloss.NewStyle().Bold(true).Foreground(th.Danger)
	case 0x07E0:
		return lipgloss.NewStyle().Bold(true).Foreground(th.Success)
	case 0x8410:
		return lipgloss.NewStyle().Foreground(th.Muted)
	default:
		return lipgloss.NewStyle().Foreground(th.Text)
	}
}

// mapKey converts a terminal key press into a protocol key code.
func mapKey(msg tea.KeyMsg) (device.KeyCode, byte, bool) {
	switch msg.String() {
	case "up":
		return device.KeyUp, 0, true
	case "down":
		return device.KeyDown, 0, true
	case "left", "shift+tab":
		return device.KeyLeft, 0, true
	case "right", "tab":
		return device.KeyRight, 0, true
	case "enter":
		return device.KeyEnter, 0, true
	case "backspace", "delete":
		return device.KeyBackspace, 0, true
	}

	runes := []rune(msg.Runes)
	if len(runes) == 1 && runes[0] >= 32 && runes[0] <= 126 {
		return device.KeyCharacter, byte(runes[0]), true
	}
	return device.KeyNone, 0, false
}

// waitForDeviceEvent blocks on the session channel inside a command so the UI
// stays responsive while the device is idle.
func waitForDeviceEvent(events <-chan device.Event) tea.Cmd {
	return func() tea.Msg {
		event, ok := <-events
		if !ok {
			return deviceClosedMsg{}
		}
		return deviceEventMsg(event)
	}
}

// sendKeyCmd forwards a key without blocking the render loop.
func sendKeyCmd(session *device.Session, code device.KeyCode, character byte) tea.Cmd {
	return func() tea.Msg {
		_ = session.SendKey(code, character)
		return nil
	}
}

var _ tea.Model = (*DeviceModel)(nil)

// String produces a human readable summary, used by diagnostics.
func (m *DeviceModel) String() string {
	return fmt.Sprintf("device(%s, %dx%d)", m.status, m.screen.Width, m.screen.Height)
}
