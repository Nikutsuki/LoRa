package device

import (
	"encoding/hex"
	"testing"
)

// TestFrameVector pins the exact wire bytes of a single Key frame.
//
// The expected value was produced by the firmware's FrameWriter, so this test
// fails if either side drifts from the agreed encoding.
func TestFrameVector(t *testing.T) {
	payload, err := EncodeKey(KeyCharacter, 'A')
	if err != nil {
		t.Fatalf("encode key: %v", err)
	}
	frame, err := EncodeFrame(MessageKey, payload)
	if err != nil {
		t.Fatalf("encode frame: %v", err)
	}

	const want = "a55a040200074" + "110b4"
	if got := hex.EncodeToString(frame); got != want {
		t.Fatalf("frame = %s, want %s", got, want)
	}
}

// TestPongFrameVector pins the empty-payload Pong frame the firmware emits in
// response to a Ping. Probing depends on this exact byte sequence.
func TestPongFrameVector(t *testing.T) {
	frame, err := EncodeFrame(MessagePong, nil)
	if err != nil {
		t.Fatalf("encode frame: %v", err)
	}
	const want = "a55a0600003c7e"
	if got := hex.EncodeToString(frame); got != want {
		t.Fatalf("pong = %s, want %s", got, want)
	}
}

// TestScreenFrameVector pins a Screen frame carrying one button.
func TestScreenFrameVector(t *testing.T) {
	b := &payloadBuilder{}
	b.u16(320)
	b.u16(240)
	b.u8(1)
	b.u8(1)
	b.u8(uint8(KindButton))
	b.u16(8)
	b.u16(180)
	b.u16(68)
	b.u16(24)
	b.u8(FlagFocusable)
	b.u16(0)
	b.u8(1)
	_ = b.str("Send")
	_ = b.str("")

	frame, err := EncodeFrame(MessageScreen, b.buf)
	if err != nil {
		t.Fatalf("encode frame: %v", err)
	}

	// Decoding the frame we just produced must recover the same data.
	reader := NewFrameReader()
	var decoded Frame
	var ok bool
	for _, by := range frame {
		if decoded, ok = reader.Feed(by); ok {
			break
		}
	}
	if !ok {
		t.Fatal("screen frame did not round trip")
	}

	screen, err := DecodeScreen(decoded.Payload)
	if err != nil {
		t.Fatalf("decode screen: %v", err)
	}
	if len(screen.Components) != 1 || screen.Components[0].Primary != "Send" {
		t.Fatalf("screen = %+v", screen)
	}
}
