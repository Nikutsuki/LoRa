package ui

import (
	"strings"
	"testing"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/user/lora-tui/internal/device"
	"github.com/user/lora-tui/internal/theme"
)

// sampleScreen builds the frame the firmware sends for the chat page.
func sampleScreen() device.Screen {
	return device.Screen{
		Width:      320,
		Height:     240,
		FullRedraw: true,
		Components: []device.Component{
			{Kind: device.KindLabel, X: 0, Y: 0, W: 320, H: 22, Primary: "MESSENGER  |  User: pico", Secondary: "433 MHz", Color: 0x07FF},
			{Kind: device.KindLabel, X: 8, Y: 26, W: 304, H: 14, Primary: "*Messages  Contacts  Invite  Identity", Color: 0x8410},
			{Kind: device.KindDivider, X: 0, Y: 44, W: 320, H: 1, Color: 0x8410},
			{Kind: device.KindLabel, X: 8, Y: 50, W: 304, H: 16, Primary: "Messages", Color: 0x07FF},
			{Kind: device.KindLabel, X: 8, Y: 68, W: 304, H: 14, Primary: "Alice", Color: 0x07FF},
			{Kind: device.KindLabel, X: 8, Y: 84, W: 304, H: 14, Primary: "AABB-CCDD-EEFF-0011", Color: 0x8410},
			{Kind: device.KindLabel, X: 8, Y: 100, W: 304, H: 14, Primary: "Alice: hi", Color: 0xFFFF},
			{Kind: device.KindLabel, X: 8, Y: 116, W: 304, H: 14, Primary: "> hey Alice", Color: 0x07E0},
			{Kind: device.KindTextBox, X: 8, Y: 168, W: 304, H: 26, Focused: true, Focusable: true, Primary: "> "},
			{Kind: device.KindButton, X: 8, Y: 202, W: 72, H: 26, Focusable: true, Primary: "Send"},
			{Kind: device.KindButton, X: 88, Y: 202, W: 76, H: 26, Focusable: true, Primary: "Contacts"},
			{Kind: device.KindButton, X: 172, Y: 202, W: 68, H: 26, Focusable: true, Primary: "Invite"},
			{Kind: device.KindButton, X: 248, Y: 202, W: 64, H: 26, Focusable: true, Primary: "ID"},
			{Kind: device.KindStatusBar, X: 0, Y: 232, W: 320, H: 18, Primary: "Listening on 433 MHz", Color: 0x07E0},
		},
	}
}

// newTestModel returns a model with a screen already applied, bypassing the
// serial session.
func newTestModel(screen device.Screen) *DeviceModel {
	m := &DeviceModel{
		theme:  theme.Default,
		width:  TotalWidth,
		height: TotalHeight,
		screen: screen,
		status: "Connected",
	}
	m.connected = true
	return m
}

// TestViewDoesNotDuplicateHeader guards the regression where the header bar was
// drawn both by the dedicated header helper and again as a body label.
func TestViewDoesNotDuplicateHeader(t *testing.T) {
	m := newTestModel(sampleScreen())
	out := m.View()

	if got := strings.Count(out, "MESSENGER"); got != 1 {
		t.Fatalf("header appears %d times, want 1:\n%s", got, out)
	}
}

// TestViewDoesNotDuplicateStatus checks the status bar is rendered once.
func TestViewDoesNotDuplicateStatus(t *testing.T) {
	m := newTestModel(sampleScreen())
	out := m.View()

	if got := strings.Count(out, "Listening on 433 MHz"); got != 1 {
		t.Fatalf("status appears %d times, want 1:\n%s", got, out)
	}
}

// TestViewFitsHeight ensures the composed view never exceeds the terminal height.
func TestViewFitsHeight(t *testing.T) {
	m := newTestModel(sampleScreen())
	lines := strings.Split(m.View(), "\n")
	if len(lines) > m.height {
		t.Fatalf("view has %d lines, want <= %d", len(lines), m.height)
	}
}

// TestViewShowsBothMessageDirections checks sent and received lines are visible.
func TestViewShowsBothMessageDirections(t *testing.T) {
	m := newTestModel(sampleScreen())
	out := m.View()

	if !strings.Contains(out, "Alice: hi") {
		t.Fatalf("incoming message missing:\n%s", out)
	}
	if !strings.Contains(out, "hey Alice") {
		t.Fatalf("outgoing message missing:\n%s", out)
	}
}

// TestButtonsAreNotBracketed guards against the bracket overflow regression.
func TestButtonsAreNotBracketed(t *testing.T) {
	m := newTestModel(sampleScreen())
	m.screen.Components[9].Focused = true

	out := m.View()
	if strings.Contains(out, "[ Send ]") {
		t.Fatalf("focused button is bracketed:\n%s", out)
	}
	if !strings.Contains(out, "Send") {
		t.Fatalf("button label missing:\n%s", out)
	}
}

// TestScreenEventClearsConnectingStatus guards the regression where a Screen
// frame set connected but left the label on the connecting placeholder, which
// showed as grey "Connecting to device..." on a working link. Firmware that
// sends no Hello frame never produces EventConnected.
func TestScreenEventClearsConnectingStatus(t *testing.T) {
	m := NewDeviceModel(nil)
	if m.status != connectingStatus {
		t.Fatalf("initial status = %q, want %q", m.status, connectingStatus)
	}

	m.applyEvent(device.Event{Kind: device.EventScreen, Screen: sampleScreen()})

	if !m.connected {
		t.Fatal("a screen frame should mark the link connected")
	}
	if m.status == connectingStatus {
		t.Fatalf("status still %q after a screen frame", m.status)
	}
}

var _ tea.Model = (*DeviceModel)(nil)
