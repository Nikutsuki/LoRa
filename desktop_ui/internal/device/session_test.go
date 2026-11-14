package device

import (
	"net"
	"testing"
	"time"
)

// fakeDevice writes protocol frames into one end of a pipe, standing in for the
// Pico firmware.
type fakeDevice struct {
	conn net.Conn
	t    *testing.T
}

func (f *fakeDevice) send(msgType MessageType, payload []byte) {
	f.t.Helper()
	frame, err := EncodeFrame(msgType, payload)
	if err != nil {
		f.t.Fatalf("encode frame: %v", err)
	}
	if _, err := f.conn.Write(frame); err != nil {
		f.t.Fatalf("write frame: %v", err)
	}
}

func (f *fakeDevice) recv() Frame {
	f.t.Helper()
	reader := NewFrameReader()
	buf := make([]byte, 256)
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		_ = f.conn.SetReadDeadline(time.Now().Add(200 * time.Millisecond))
		n, err := f.conn.Read(buf)
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				continue
			}
			f.t.Fatalf("read frame: %v", err)
		}
		for i := 0; i < n; i++ {
			if frame, ok := reader.Feed(buf[i]); ok {
				return frame
			}
		}
	}
	f.t.Fatal("timed out waiting for frame")
	return Frame{}
}

// TestSessionReceivesScreenAndForwardsKeys drives the full session path: the
// device announces itself and paints a screen, then a forwarded key must arrive
// on the device side byte-exact.
func TestSessionReceivesScreenAndForwardsKeys(t *testing.T) {
	hostConn, deviceConn := dialFakeDevice()

	session := Start(hostConn)
	defer session.Close()

	dev := &fakeDevice{conn: deviceConn, t: t}

	// Hello announces device geometry.
	helloPayload := &payloadBuilder{}
	helloPayload.u16(320)
	helloPayload.u16(240)
	helloPayload.u8(1)
	dev.send(MessageHello, helloPayload.buf)

	// Screen carries a title label and a focused button.
	screenPayload := &payloadBuilder{}
	screenPayload.u16(320)
	screenPayload.u16(240)
	screenPayload.u8(1)
	screenPayload.u8(2)
	screenPayload.u8(uint8(KindLabel))
	screenPayload.u16(8)
	screenPayload.u16(8)
	screenPayload.u16(304)
	screenPayload.u16(18)
	screenPayload.u8(0)
	screenPayload.u16(0xFFFF)
	screenPayload.u8(2)
	_ = screenPayload.str("Messages")
	_ = screenPayload.str("")
	screenPayload.u8(uint8(KindButton))
	screenPayload.u16(8)
	screenPayload.u16(180)
	screenPayload.u16(68)
	screenPayload.u16(24)
	screenPayload.u8(FlagFocused | FlagFocusable)
	screenPayload.u16(0xFFE0)
	screenPayload.u8(1)
	_ = screenPayload.str("Send")
	_ = screenPayload.str("")
	dev.send(MessageScreen, screenPayload.buf)

	// Collect events until the screen arrives.
	var screen Screen
	deadline := time.After(2 * time.Second)
	for screen.Components == nil {
		select {
		case ev, ok := <-session.Events():
			if !ok {
				t.Fatal("event channel closed early")
			}
			if ev.Kind == EventScreen {
				screen = ev.Screen
			}
		case <-deadline:
			t.Fatal("timed out waiting for screen event")
		}
	}

	if screen.Width != 320 || screen.Height != 240 {
		t.Fatalf("geometry = %dx%d", screen.Width, screen.Height)
	}
	if len(screen.Components) != 2 {
		t.Fatalf("components = %d, want 2", len(screen.Components))
	}
	if screen.Components[0].Primary != "Messages" || screen.Components[0].Scale != 2 {
		t.Fatalf("title = %+v", screen.Components[0])
	}
	if screen.Components[1].Primary != "Send" || !screen.Components[1].Focused {
		t.Fatalf("button = %+v", screen.Components[1])
	}

	if !session.Connected() {
		t.Fatal("session should report connected")
	}
	if got := session.Hello().Width; got != 320 {
		t.Fatalf("hello width = %d", got)
	}

	// A forwarded key must arrive at the device as a Key frame. The heartbeat
	// also sends Ping frames on its own schedule, so skip anything that is not
	// the key we are waiting for.
	keyErr := make(chan error, 1)
	go func() {
		keyErr <- session.SendKey(KeyCharacter, 'x')
	}()

	var frame Frame
	for {
		frame = dev.recv()
		if frame.Type == MessageKey {
			break
		}
	}
	if err := <-keyErr; err != nil {
		t.Fatalf("send key: %v", err)
	}
	r := &payloadReader{buf: frame.Payload}
	code, _ := r.u8()
	char, _ := r.u8()
	if KeyCode(code) != KeyCharacter || char != 'x' {
		t.Fatalf("key = %v/%q", KeyCode(code), char)
	}
}

// TestSessionReportsNotice checks notice frames surface as events.
func TestSessionReportsNotice(t *testing.T) {
	hostConn, deviceConn := dialFakeDevice()

	session := Start(hostConn)
	defer session.Close()

	dev := &fakeDevice{conn: deviceConn, t: t}
	payload := &payloadBuilder{}
	payload.u8(uint8(NoticeSuccess))
	_ = payload.str("CardKB connected")
	dev.send(MessageNotice, payload.buf)

	deadline := time.After(2 * time.Second)
	for {
		select {
		case ev := <-session.Events():
			if ev.Kind == EventNotice {
				if ev.Notice.Text != "CardKB connected" {
					t.Fatalf("notice = %+v", ev.Notice)
				}
				if ev.Notice.Level != NoticeSuccess {
					t.Fatalf("level = %v", ev.Notice.Level)
				}
				return
			}
		case <-deadline:
			t.Fatal("timed out waiting for notice")
		}
	}
}
