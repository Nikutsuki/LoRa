package device

import (
	"errors"
	"sync"
	"time"
)

// ErrClosed reports that the session was shut down.
var ErrClosed = errors.New("device: session closed")

// EventKind identifies a session event delivered to the UI.
type EventKind uint8

const (
	// EventConnected fires once the link is established.
	EventConnected EventKind = iota
	// EventScreen fires whenever a new Screen frame arrives.
	EventScreen
	// EventNotice fires when the device sends a status line.
	EventNotice
	// EventDisconnected fires when the link drops.
	EventDisconnected
)

// Event is a state change the UI needs to react to.
type Event struct {
	Kind   EventKind
	Screen Screen
	Notice Notice
	Err    error
}

// Session drives a device link on a background goroutine.
//
// It owns no UI state: it decodes frames, publishes events on a channel, and
// forwards key presses. That keeps the Bubble Tea model free of concurrency
// concerns.
type Session struct {
	link   *Link
	events chan Event
	done   chan struct{}
	once   sync.Once

	mu         sync.RWMutex
	hello      Hello
	lastScreen Screen
	lastNotice Notice
	connected  bool

	// writeMu serialises outbound frames. The heartbeat and the UI both write,
	// and a transport whose buffer is full blocks on Write (an unbuffered pipe
	// blocks outright), so unchecked concurrent writes can wedge the link.
	writeMu sync.Mutex
}

// Start opens the session on an already connected transport.
//
// The device only transmits when its UI changes, so a freshly attached host must
// ask for a redraw; the initial Ping does that immediately rather than waiting
// for the user to press a key on the physical keyboard. A periodic heartbeat
// keeps doing so, which also recovers if a single frame is ever lost.
func Start(transport Transport) *Session {
	s := &Session{
		link:   NewLink(transport),
		events: make(chan Event, 16),
		done:   make(chan struct{}),
	}
	go s.readLoop()
	go s.heartbeat()
	return s
}

// heartbeatInterval is comfortably slower than the device's poll loop so it
// never floods the link, but fast enough that an idle terminal still refreshes.
const heartbeatInterval = 2 * time.Second

// heartbeat periodically probes the device to request a redraw.
func (s *Session) heartbeat() {
	ticker := time.NewTicker(heartbeatInterval)
	defer ticker.Stop()

	_, _ = s.write(func() error { return s.link.SendPing() })
	for {
		select {
		case <-s.done:
			return
		case <-ticker.C:
			_, _ = s.write(func() error { return s.link.SendPing() })
		}
	}
}

// write runs fn while holding the write lock, abandoning it if the session is
// closed so shutdown never waits on a blocked transport.
func (s *Session) write(fn func() error) (error, bool) {
	select {
	case <-s.done:
		return ErrClosed, false
	default:
	}

	s.writeMu.Lock()
	defer s.writeMu.Unlock()
	return fn(), true
}

// Events is the channel the UI selects on.
func (s *Session) Events() <-chan Event {
	return s.events
}

// Close tears down the link and the read loop.
func (s *Session) Close() error {
	var err error
	s.once.Do(func() {
		close(s.done)
		err = s.link.Close()
	})
	return err
}

// Hello returns the last received device geometry.
func (s *Session) Hello() Hello {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.hello
}

// Screen returns the most recent screen description.
func (s *Session) Screen() Screen {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.lastScreen
}

// Notice returns the most recent status line.
func (s *Session) Notice() Notice {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.lastNotice
}

// Connected reports whether the link is live.
func (s *Session) Connected() bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.connected
}

// SendKey forwards one key press, translating failures into errors the UI can
// surface instead of dropping input silently.
func (s *Session) SendKey(code KeyCode, character byte) error {
	err, _ := s.write(func() error { return s.link.SendKey(code, character) })
	return err
}

// Ping probes liveness.
func (s *Session) Ping() error {
	err, _ := s.write(func() error { return s.link.SendPing() })
	return err
}

func (s *Session) readLoop() {
	defer func() {
		s.mu.Lock()
		s.connected = false
		s.mu.Unlock()
		s.publish(Event{Kind: EventDisconnected})
		close(s.events)
	}()

	for {
		select {
		case <-s.done:
			return
		default:
		}

		frame, err := s.link.ReadFrame()
		if err != nil {
			select {
			case <-s.done:
				return
			default:
			}
			// A serial read timeout is normal; only surface real failures.
			if isTimeout(err) {
				continue
			}
			s.publish(Event{Kind: EventDisconnected, Err: err})
			return
		}

		s.handleFrame(frame)
	}
}

func (s *Session) handleFrame(frame Frame) {
	switch frame.Type {
	case MessageHello:
		hello, err := DecodeHello(frame.Payload)
		if err != nil {
			return
		}
		s.mu.Lock()
		s.hello = hello
		firstConnect := !s.connected
		s.connected = true
		s.mu.Unlock()
		if firstConnect {
			s.publish(Event{Kind: EventConnected})
		}

	case MessageScreen:
		screen, err := DecodeScreen(frame.Payload)
		if err != nil {
			return
		}
		s.mu.Lock()
		s.lastScreen = screen
		s.connected = true
		s.mu.Unlock()
		s.publish(Event{Kind: EventScreen, Screen: screen})

	case MessageNotice:
		notice, err := DecodeNotice(frame.Payload)
		if err != nil {
			return
		}
		s.mu.Lock()
		s.lastNotice = notice
		s.mu.Unlock()
		s.publish(Event{Kind: EventNotice, Notice: notice})

	case MessagePong:
		s.mu.Lock()
		s.connected = true
		s.mu.Unlock()
	}
}

func (s *Session) publish(event Event) {
	select {
	case s.events <- event:
	case <-s.done:
	case <-time.After(50 * time.Millisecond):
		// Dropping a stale frame is preferable to stalling the read loop; the
		// next Screen frame carries the complete state anyway.
	}
}

func isTimeout(err error) bool {
	type timeout interface{ Timeout() bool }
	if t, ok := err.(timeout); ok {
		return t.Timeout()
	}
	return false
}
