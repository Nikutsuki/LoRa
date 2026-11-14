package device

// Frame is one decoded protocol frame.
type Frame struct {
	Type    MessageType
	Payload []byte
}

// FrameReader incrementally reassembles frames from a byte stream.
//
// It tolerates arbitrary fragmentation and resynchronises on the next sync pair
// after any malformed frame, which matters because the host attaches to a device
// that may already be mid-transmission.
type FrameReader struct {
	state       readState
	msgType     MessageType
	payloadLen  int
	payload     []byte
	payloadIdx  int
	runningCRC  uint16
	expectedCRC uint16
}

type readState uint8

const (
	stateSync0 readState = iota
	stateSync1
	stateType
	stateLengthLow
	stateLengthHigh
	statePayload
	stateCRCLow
	stateCRCHigh
)

// NewFrameReader returns a reader positioned at the start of a frame.
func NewFrameReader() *FrameReader {
	return &FrameReader{state: stateSync0, runningCRC: 0xFFFF}
}

// Reset discards partial state.
func (r *FrameReader) Reset() {
	r.state = stateSync0
	r.msgType = 0
	r.payloadLen = 0
	r.payload = nil
	r.payloadIdx = 0
	r.runningCRC = 0xFFFF
	r.expectedCRC = 0
}

// Feed consumes one byte and returns a completed frame when one is available.
func (r *FrameReader) Feed(b byte) (Frame, bool) {
	switch r.state {
	case stateSync0:
		if b == SyncByte0 {
			r.state = stateSync1
		}

	case stateSync1:
		if b == SyncByte1 {
			r.state = stateType
		} else {
			r.state = stateSync0
		}

	case stateType:
		r.msgType = MessageType(b)
		r.runningCRC = crcStep(0xFFFF, b)
		r.state = stateLengthLow

	case stateLengthLow:
		r.runningCRC = crcStep(r.runningCRC, b)
		r.payloadLen = int(b)
		r.state = stateLengthHigh

	case stateLengthHigh:
		r.runningCRC = crcStep(r.runningCRC, b)
		r.payloadLen |= int(b) << 8
		if r.payloadLen > MaxPayload {
			r.Reset()
			return Frame{}, false
		}
		r.payload = make([]byte, r.payloadLen)
		r.payloadIdx = 0
		if r.payloadLen == 0 {
			r.state = stateCRCLow
		} else {
			r.state = statePayload
		}

	case statePayload:
		r.runningCRC = crcStep(r.runningCRC, b)
		r.payload[r.payloadIdx] = b
		r.payloadIdx++
		if r.payloadIdx >= r.payloadLen {
			r.state = stateCRCLow
		}

	case stateCRCLow:
		r.expectedCRC = uint16(b)
		r.state = stateCRCHigh

	case stateCRCHigh:
		r.expectedCRC |= uint16(b) << 8
		if r.expectedCRC != r.runningCRC {
			r.Reset()
			return Frame{}, false
		}
		frame := Frame{Type: r.msgType, Payload: r.payload}
		r.Reset()
		return frame, true
	}

	return Frame{}, false
}

func crcStep(crc uint16, b byte) uint16 {
	crc ^= uint16(b) << 8
	for i := 0; i < 8; i++ {
		if crc&0x8000 != 0 {
			crc = crc<<1 ^ 0x1021
		} else {
			crc <<= 1
		}
	}
	return crc
}
