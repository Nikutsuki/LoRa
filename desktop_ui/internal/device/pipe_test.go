package device

import (
	"io"
	"net"
	"sync"
)

// orderedConn buffers writes so callers never block, while preserving the order
// in which those writes were issued.
//
// Two properties matter here, and net.Pipe offers neither on its own:
//
//   - Writes must not block. A serial driver buffers, so a writer makes progress
//     even when nobody is reading yet. With a bare net.Pipe the session's
//     heartbeat Ping would block and starve the read loop.
//   - Writes must stay ordered. Buffering each write on its own goroutine makes
//     the writer non-blocking but lets frames overtake each other, so a Hello
//     can arrive after the Screen that followed it.
//
// A single drain goroutine over a FIFO channel satisfies both.
type orderedConn struct {
	net.Conn

	queue     chan []byte
	stop      chan struct{}
	closeOnce sync.Once
	wg        sync.WaitGroup
}

func newOrderedConn(conn net.Conn) *orderedConn {
	c := &orderedConn{
		Conn:  conn,
		queue: make(chan []byte, 256),
		stop:  make(chan struct{}),
	}
	c.wg.Add(1)
	go c.drain()
	return c
}

// drain forwards queued writes to the underlying connection in FIFO order.
//
// A write to the peer can block indefinitely when the peer has stopped reading,
// so each write is bounded by the connection deadline rather than pinning the
// goroutine forever.
func (c *orderedConn) drain() {
	defer c.wg.Done()
	for {
		select {
		case buf := <-c.queue:
			if _, err := c.Conn.Write(buf); err != nil {
				return
			}
		case <-c.stop:
			return
		}
	}
}

func (c *orderedConn) Write(b []byte) (int, error) {
	buf := make([]byte, len(b))
	copy(buf, b)

	select {
	case c.queue <- buf:
		return len(b), nil
	case <-c.stop:
		return 0, io.ErrClosedPipe
	}
}

func (c *orderedConn) Close() error {
	c.closeOnce.Do(func() {
		close(c.stop)
		_ = c.Conn.Close()
	})
	return nil
}

// dialFakeDevice builds a buffered, order-preserving pair standing in for the
// host and the device.
func dialFakeDevice() (net.Conn, net.Conn) {
	hostSide, deviceSide := net.Pipe()
	return newOrderedConn(hostSide), newOrderedConn(deviceSide)
}
