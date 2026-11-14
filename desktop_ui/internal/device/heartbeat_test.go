package device

import (
	"net"
	"testing"
	"time"
)

// blockingTransport accepts writes but never lets them finish, modelling a port
// whose buffer is full (or a link that has stalled).
type blockingTransport struct {
	block chan struct{}
	done  chan struct{}
}

func newBlockingTransport() *blockingTransport {
	return &blockingTransport{block: make(chan struct{}), done: make(chan struct{})}
}

func (t *blockingTransport) Read(b []byte) (int, error) {
	<-t.done
	return 0, net.ErrClosed
}

func (t *blockingTransport) Write(b []byte) (int, error) {
	<-t.block
	return 0, net.ErrClosed
}

func (t *blockingTransport) Close() error {
	select {
	case <-t.done:
	default:
		close(t.done)
	}
	select {
	case <-t.block:
	default:
		close(t.block)
	}
	return nil
}

// TestHeartbeatDoesNotBlockSessionStart ensures a stalled write cannot prevent
// the session from starting or from reporting state.
//
// Regression guard: an unsynchronised heartbeat that blocked inside Write starved
// the read loop and hung the whole session, which presented as the UI sitting on
// "waiting for device screen" forever.
func TestHeartbeatDoesNotBlockSessionStart(t *testing.T) {
	transport := newBlockingTransport()
	session := Start(transport)
	defer session.Close()

	// The session must remain usable even though every write is stuck forever.
	select {
	case <-time.After(300 * time.Millisecond):
	case <-session.Events():
	}

	if session.Connected() {
		t.Fatal("session should not report connected without a Hello")
	}
	_ = session.Hello()
	_ = session.Screen()
	_ = session.Notice()
}

// TestCloseIsPromptWithStalledWrites ensures shutdown does not wait on a blocked
// transport.
func TestCloseIsPromptWithStalledWrites(t *testing.T) {
	transport := newBlockingTransport()
	session := Start(transport)

	closed := make(chan struct{})
	go func() {
		_ = session.Close()
		close(closed)
	}()

	select {
	case <-closed:
	case <-time.After(2 * time.Second):
		t.Fatal("Close blocked on a stalled transport")
	}
}

// TestSendKeyReturnsErrAfterClose documents the closed-session contract.
func TestSendKeyReturnsErrAfterClose(t *testing.T) {
	transport := newBlockingTransport()
	session := Start(transport)
	_ = session.Close()

	if err := session.SendKey(KeyEnter, 0); err != ErrClosed {
		t.Fatalf("err = %v, want ErrClosed", err)
	}
	if err := session.Ping(); err != ErrClosed {
		t.Fatalf("err = %v, want ErrClosed", err)
	}
}
