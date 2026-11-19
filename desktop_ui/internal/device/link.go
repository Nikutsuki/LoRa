package device

import "io"

// Transport is a bidirectional byte stream to the device.
//
// Keeping this narrow means the protocol layer can be exercised against an
// in-memory pipe in tests instead of real hardware.
type Transport interface {
	io.Reader
	io.Writer
	io.Closer
}

// Link decodes frames from a Transport and encodes outbound ones.
type Link struct {
	transport Transport
	reader    *FrameReader
}

// NewLink wraps a transport with frame decoding.
func NewLink(transport Transport) *Link {
	return &Link{
		transport: transport,
		reader:    NewFrameReader(),
	}
}

// ReadFrame blocks for the next valid frame, discarding malformed ones.
func (l *Link) ReadFrame() (Frame, error) {
	buf := make([]byte, 256)
	for {
		n, err := l.transport.Read(buf)
		if err != nil {
			return Frame{}, err
		}
		for i := 0; i < n; i++ {
			if frame, ok := l.reader.Feed(buf[i]); ok {
				return frame, nil
			}
		}
	}
}

// SendKey writes one key press to the device.
func (l *Link) SendKey(code KeyCode, character byte) error {
	payload, err := EncodeKey(code, character)
	if err != nil {
		return err
	}
	frame, err := EncodeFrame(MessageKey, payload)
	if err != nil {
		return err
	}
	_, err = l.transport.Write(frame)
	return err
}

// SendPing writes a liveness probe.
func (l *Link) SendPing() error {
	frame, err := EncodeFrame(MessagePing, nil)
	if err != nil {
		return err
	}
	_, err = l.transport.Write(frame)
	return err
}

func (l *Link) SendFileExchangeStart(filename string, filesize uint32) error {
	payload, err := EncodeFileExchangeStart(filename, filesize)
	if err != nil {
		return err
	}
	frame, err := EncodeFrame(MessageFileExchangeStart, payload)
	if err != nil {
		return err
	}
	_, err = l.transport.Write(frame)
	return err
}

func (l *Link) SendFileExchangeData(chunkIndex uint32, chunkData []byte) error {
	payload, err := EncodeFileExchangeData(chunkIndex, chunkData)
	if err != nil {
		return err
	}

	frame, err := EncodeFrame(MessageFileExchangeData, payload)
	if err != nil {
		return err
	}

	_, err = l.transport.Write(frame)

	return err
}

func (l *Link) SendFileExchangeEnd() error {
	frame, err := EncodeFrame(MessageFileExchangeEnd, nil)

	if err != nil {
		return err
	}

	_, err = l.transport.Write(frame)

	return err
}

// Close releases the underlying transport.
func (l *Link) Close() error {
	return l.transport.Close()
}
