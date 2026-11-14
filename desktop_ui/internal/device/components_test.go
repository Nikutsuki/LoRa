package device

import "testing"

// TestDecodeScreenWithNewKinds covers the divider and status bar components the
// firmware added when the device UI was aligned with the desktop layout.
func TestDecodeScreenWithNewKinds(t *testing.T) {
	b := &payloadBuilder{}
	b.u16(320)
	b.u16(240)
	b.u8(1)
	b.u8(3)

	// Divider: a one pixel rule spanning the width.
	b.u8(uint8(KindDivider))
	b.u16(0)
	b.u16(44)
	b.u16(320)
	b.u16(1)
	b.u8(0)
	b.u16(0x8410)
	b.u8(1)
	_ = b.str("")
	_ = b.str("")

	// Text box carrying the input prompt.
	b.u8(uint8(KindTextBox))
	b.u16(8)
	b.u16(168)
	b.u16(304)
	b.u16(26)
	b.u8(FlagFocusable | FlagFocused)
	b.u16(0xFFFF)
	b.u8(1)
	_ = b.str("> hello")
	_ = b.str("")

	// Status bar pinned to the bottom.
	b.u8(uint8(KindStatusBar))
	b.u16(0)
	b.u16(232)
	b.u16(320)
	b.u16(18)
	b.u8(0)
	b.u16(0x07E0)
	b.u8(1)
	_ = b.str("Listening on 433 MHz")
	_ = b.str("")

	screen, err := DecodeScreen(b.buf)
	if err != nil {
		t.Fatalf("decode: %v", err)
	}
	if len(screen.Components) != 3 {
		t.Fatalf("components = %d, want 3", len(screen.Components))
	}

	if screen.Components[0].Kind != KindDivider {
		t.Fatalf("first kind = %v, want divider", screen.Components[0].Kind)
	}
	if screen.Components[1].Kind != KindTextBox || screen.Components[1].Primary != "> hello" {
		t.Fatalf("textbox = %+v", screen.Components[1])
	}
	if !screen.Components[1].Focused {
		t.Fatal("textbox should be focused")
	}
	status := screen.Components[2]
	if status.Kind != KindStatusBar || status.Primary != "Listening on 433 MHz" {
		t.Fatalf("status = %+v", status)
	}
	if status.Y != 232 {
		t.Fatalf("status y = %d, want 232", status.Y)
	}
}

// TestMaxPayloadMatchesFirmware guards against the two sides drifting apart on
// the frame budget, which would silently truncate dense screens.
func TestMaxPayloadMatchesFirmware(t *testing.T) {
	// Mirrors proto::MaxPayload in src/protocol/Protocol.hpp.
	const firmwareMaxPayload = 1536
	if MaxPayload != firmwareMaxPayload {
		t.Fatalf("MaxPayload = %d, firmware uses %d", MaxPayload, firmwareMaxPayload)
	}
}
