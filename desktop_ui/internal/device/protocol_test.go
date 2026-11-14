package device

import (
	"bytes"
	"io"
	"testing"
)

// TestFrameRoundTrip verifies the host encoder and decoder agree.
func TestFrameRoundTrip(t *testing.T) {
	payload, err := EncodeKey(KeyCharacter, 'A')
	if err != nil {
		t.Fatalf("encode key: %v", err)
	}

	frame, err := EncodeFrame(MessageKey, payload)
	if err != nil {
		t.Fatalf("encode frame: %v", err)
	}

	reader := NewFrameReader()
	var decoded Frame
	var ok bool
	for _, b := range frame {
		if decoded, ok = reader.Feed(b); ok {
			break
		}
	}

	if !ok {
		t.Fatal("frame was not decoded")
	}
	if decoded.Type != MessageKey {
		t.Fatalf("type = %v, want %v", decoded.Type, MessageKey)
	}
	if !bytes.Equal(decoded.Payload, payload) {
		t.Fatalf("payload = %v, want %v", decoded.Payload, payload)
	}
}

// TestFrameReaderResyncsAfterGarbage ensures a host that attaches mid-stream
// recovers instead of failing permanently.
func TestFrameReaderResyncsAfterGarbage(t *testing.T) {
	reader := NewFrameReader()
	for _, b := range []byte{0x00, 0xFF, 0xA5, 0x00, 0x13} {
		if _, ok := reader.Feed(b); ok {
			t.Fatal("garbage produced a frame")
		}
	}

	payload, _ := EncodeKey(KeyEnter, 0)
	frame, _ := EncodeFrame(MessageKey, payload)

	var decoded Frame
	var ok bool
	for _, b := range frame {
		if decoded, ok = reader.Feed(b); ok {
			break
		}
	}
	if !ok {
		t.Fatal("reader did not recover")
	}
	if decoded.Type != MessageKey {
		t.Fatalf("type = %v, want %v", decoded.Type, MessageKey)
	}
}

// TestFrameReaderRejectsBadCRC ensures corrupted frames are dropped.
func TestFrameReaderRejectsBadCRC(t *testing.T) {
	payload, _ := EncodeKey(KeyEnter, 0)
	frame, _ := EncodeFrame(MessageKey, payload)
	frame[len(frame)-1] ^= 0xFF

	reader := NewFrameReader()
	for _, b := range frame {
		if _, ok := reader.Feed(b); ok {
			t.Fatal("corrupted frame was accepted")
		}
	}
}

// TestDecodeScreen parses a hand-built screen payload.
func TestDecodeScreen(t *testing.T) {
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
	b.u8(FlagFocused | FlagFocusable)
	b.u16(0xFFFF)
	b.u8(1)
	_ = b.str("Send")
	_ = b.str("")

	screen, err := DecodeScreen(b.buf)
	if err != nil {
		t.Fatalf("decode screen: %v", err)
	}
	if screen.Width != 320 || screen.Height != 240 {
		t.Fatalf("geometry = %dx%d", screen.Width, screen.Height)
	}
	if !screen.FullRedraw {
		t.Fatal("fullRedraw not parsed")
	}
	if len(screen.Components) != 1 {
		t.Fatalf("components = %d, want 1", len(screen.Components))
	}

	c := screen.Components[0]
	if c.Kind != KindButton || c.Primary != "Send" {
		t.Fatalf("component = %+v", c)
	}
	if !c.Focused || !c.Focusable || c.Selected {
		t.Fatalf("flags = %+v", c)
	}
}

// TestDecodeScreenRejectsTruncatedPayload ensures malformed input is safe.
func TestDecodeScreenRejectsTruncatedPayload(t *testing.T) {
	if _, err := DecodeScreen([]byte{0x40, 0x01}); err == nil {
		t.Fatal("expected error for truncated payload")
	}
}

// pipe is an in-memory Transport used to drive the Link end to end.
type pipe struct {
	r *io.PipeReader
	w *io.PipeWriter
}

func newPipe() *pipe {
	r, w := io.Pipe()
	return &pipe{r: r, w: w}
}

func (p *pipe) Read(b []byte) (int, error)  { return p.r.Read(b) }
func (p *pipe) Write(b []byte) (int, error) { return p.w.Write(b) }
func (p *pipe) Close() error                { return p.r.Close() }

// TestLinkSendKey checks the Link writes a frame the reader accepts.
func TestLinkSendKey(t *testing.T) {
	p := newPipe()
	link := NewLink(p)

	go func() {
		_ = link.SendKey(KeyCharacter, 'z')
		p.w.Close()
	}()

	frame, err := link.ReadFrame()
	if err != nil {
		t.Fatalf("read frame: %v", err)
	}
	if frame.Type != MessageKey {
		t.Fatalf("type = %v, want %v", frame.Type, MessageKey)
	}

	p2 := payloadReader{buf: frame.Payload}
	code, _ := p2.u8()
	char, _ := p2.u8()
	if KeyCode(code) != KeyCharacter || char != 'z' {
		t.Fatalf("key = %v/%q", code, char)
	}
}
