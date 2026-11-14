// Package device implements the host side of the UART control protocol
// spoken by the Pico firmware.
package device

import (
	"encoding/binary"
	"errors"
	"fmt"
)

// Framing constants mirror examples/pico-hw-test/protocol/Protocol.hpp.
const (
	SyncByte0   = 0xA5
	SyncByte1   = 0x5A
	HeaderSize  = 5
	TrailerSize = 2

	// MaxPayload must match proto::MaxPayload in Protocol.hpp. A dense chat
	// screen with eight history rows needs roughly 1.2 KB.
	MaxPayload = 1536
	MaxFrame   = HeaderSize + MaxPayload + TrailerSize
)

// MessageType identifies a frame payload.
type MessageType uint8

const (
	MessageHello  MessageType = 1
	MessageScreen MessageType = 2
	MessageNotice MessageType = 3
	MessageKey    MessageType = 4
	MessagePing   MessageType = 5
	MessagePong   MessageType = 6
)

// ComponentKind mirrors ui::ComponentKind.
type ComponentKind uint8

const (
	KindLabel     ComponentKind = 0
	KindButton    ComponentKind = 1
	KindTextBox   ComponentKind = 2
	KindListItem  ComponentKind = 3
	KindDivider   ComponentKind = 4
	KindStatusBar ComponentKind = 5
)

// KeyCode mirrors proto::KeyCode.
type KeyCode uint8

const (
	KeyNone      KeyCode = 0
	KeyUp        KeyCode = 1
	KeyDown      KeyCode = 2
	KeyLeft      KeyCode = 3
	KeyRight     KeyCode = 4
	KeyEnter     KeyCode = 5
	KeyBackspace KeyCode = 6
	KeyCharacter KeyCode = 7
)

// NoticeLevel mirrors proto::NoticeLevel.
type NoticeLevel uint8

const (
	NoticeInfo    NoticeLevel = 0
	NoticeSuccess NoticeLevel = 1
	NoticeDanger  NoticeLevel = 2
)

// Component flags inside a Screen frame.
const (
	FlagFocused   = 0x01
	FlagSelected  = 0x02
	FlagFocusable = 0x04
)

// ErrShortPayload reports a payload that ended before all fields were read.
var ErrShortPayload = errors.New("device: truncated payload")

// CRC16 computes CRC-16/CCITT-FALSE, matching proto::crc16.
func CRC16(data []byte) uint16 {
	crc := uint16(0xFFFF)
	for _, b := range data {
		crc ^= uint16(b) << 8
		for i := 0; i < 8; i++ {
			if crc&0x8000 != 0 {
				crc = crc<<1 ^ 0x1021
			} else {
				crc <<= 1
			}
		}
	}
	return crc
}

// EncodeFrame wraps payload in the sync/type/length/CRC envelope.
func EncodeFrame(msgType MessageType, payload []byte) ([]byte, error) {
	if len(payload) > MaxPayload {
		return nil, fmt.Errorf("device: payload of %d bytes exceeds %d", len(payload), MaxPayload)
	}

	frame := make([]byte, HeaderSize+len(payload)+TrailerSize)
	frame[0] = SyncByte0
	frame[1] = SyncByte1
	frame[2] = byte(msgType)
	binary.LittleEndian.PutUint16(frame[3:], uint16(len(payload)))
	copy(frame[HeaderSize:], payload)

	// CRC covers the type byte, the two length bytes and the payload.
	crc := CRC16(frame[2 : HeaderSize+len(payload)])
	binary.LittleEndian.PutUint16(frame[HeaderSize+len(payload):], crc)
	return frame, nil
}

// Component is one widget inside a Screen frame.
type Component struct {
	Kind      ComponentKind
	X, Y      uint16
	W, H      uint16
	Focused   bool
	Selected  bool
	Focusable bool
	Color     uint16
	Scale     uint8
	Primary   string
	Secondary string
}

// Screen is a full screen description from the device.
type Screen struct {
	Width      uint16
	Height     uint16
	FullRedraw bool
	Components []Component
}

// Notice is a transient status line from the device.
type Notice struct {
	Level NoticeLevel
	Text  string
}

// Hello announces the device geometry and protocol version.
type Hello struct {
	Width           uint16
	Height          uint16
	ProtocolVersion uint8
}

// payloadBuilder appends little-endian scalars and length-prefixed strings.
type payloadBuilder struct {
	buf []byte
}

func (b *payloadBuilder) u8(v uint8) { b.buf = append(b.buf, v) }

func (b *payloadBuilder) u16(v uint16) {
	b.buf = append(b.buf, byte(v), byte(v>>8))
}

func (b *payloadBuilder) str(v string) error {
	if len(v) > 255 {
		return fmt.Errorf("device: string of %d bytes exceeds 255", len(v))
	}
	b.buf = append(b.buf, byte(len(v)))
	b.buf = append(b.buf, v...)
	return nil
}

// payloadReader is the bounds-checked counterpart to payloadBuilder.
type payloadReader struct {
	buf []byte
	off int
}

func (r *payloadReader) u8() (uint8, error) {
	if r.off+1 > len(r.buf) {
		return 0, ErrShortPayload
	}
	v := r.buf[r.off]
	r.off++
	return v, nil
}

func (r *payloadReader) u16() (uint16, error) {
	if r.off+2 > len(r.buf) {
		return 0, ErrShortPayload
	}
	v := binary.LittleEndian.Uint16(r.buf[r.off:])
	r.off += 2
	return v, nil
}

func (r *payloadReader) str() (string, error) {
	length, err := r.u8()
	if err != nil {
		return "", err
	}
	if r.off+int(length) > len(r.buf) {
		return "", ErrShortPayload
	}
	v := string(r.buf[r.off : r.off+int(length)])
	r.off += int(length)
	return v, nil
}

// EncodeKey builds the payload for a single key press.
func EncodeKey(code KeyCode, character byte) ([]byte, error) {
	b := &payloadBuilder{}
	b.u8(uint8(code))
	b.u8(character)
	return b.buf, nil
}

// EncodePing builds an empty liveness payload.
func EncodePing() []byte { return nil }

// DecodeHello parses a Hello payload.
func DecodeHello(payload []byte) (Hello, error) {
	r := &payloadReader{buf: payload}
	var h Hello
	var err error
	if h.Width, err = r.u16(); err != nil {
		return h, err
	}
	if h.Height, err = r.u16(); err != nil {
		return h, err
	}
	h.ProtocolVersion, err = r.u8()
	return h, err
}

// DecodeNotice parses a Notice payload.
func DecodeNotice(payload []byte) (Notice, error) {
	r := &payloadReader{buf: payload}
	level, err := r.u8()
	if err != nil {
		return Notice{}, err
	}
	text, err := r.str()
	if err != nil {
		return Notice{}, err
	}
	return Notice{Level: NoticeLevel(level), Text: text}, nil
}

// DecodeScreen parses a Screen payload.
func DecodeScreen(payload []byte) (Screen, error) {
	r := &payloadReader{buf: payload}
	var s Screen

	width, err := r.u16()
	if err != nil {
		return s, err
	}
	height, err := r.u16()
	if err != nil {
		return s, err
	}
	fullRedraw, err := r.u8()
	if err != nil {
		return s, err
	}
	count, err := r.u8()
	if err != nil {
		return s, err
	}

	s.Width = width
	s.Height = height
	s.FullRedraw = fullRedraw != 0
	s.Components = make([]Component, 0, count)

	for i := 0; i < int(count); i++ {
		c, err := decodeComponent(r)
		if err != nil {
			return s, err
		}
		s.Components = append(s.Components, c)
	}
	return s, nil
}

func decodeComponent(r *payloadReader) (Component, error) {
	var c Component

	kind, err := r.u8()
	if err != nil {
		return c, err
	}
	c.Kind = ComponentKind(kind)

	if c.X, err = r.u16(); err != nil {
		return c, err
	}
	if c.Y, err = r.u16(); err != nil {
		return c, err
	}
	if c.W, err = r.u16(); err != nil {
		return c, err
	}
	if c.H, err = r.u16(); err != nil {
		return c, err
	}
	flags, err := r.u8()
	if err != nil {
		return c, err
	}
	if c.Color, err = r.u16(); err != nil {
		return c, err
	}
	if c.Scale, err = r.u8(); err != nil {
		return c, err
	}
	if c.Primary, err = r.str(); err != nil {
		return c, err
	}
	if c.Secondary, err = r.str(); err != nil {
		return c, err
	}

	c.Focused = flags&FlagFocused != 0
	c.Selected = flags&FlagSelected != 0
	c.Focusable = flags&FlagFocusable != 0
	return c, nil
}
