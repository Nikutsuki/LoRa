package device

import (
	"fmt"
	"sort"
	"time"
)

// ProbeConfig controls how a candidate port is tested.
type ProbeConfig struct {
	Baud    int
	Timeout time.Duration
}

// DefaultProbeConfig waits long enough for one UART round trip at 115200 baud
// without making a full port scan feel slow.
func DefaultProbeConfig() ProbeConfig {
	return ProbeConfig{
		Baud:    115200,
		Timeout: 600 * time.Millisecond,
	}
}

// Candidate is a serial port that answered the handshake.
type Candidate struct {
	Port  string
	Hello Hello
}

// ProbeResult reports why a single port was rejected.
type ProbeResult struct {
	Port string
	Err  error
}

// FindDevice probes each port in turn and returns the first one that speaks the
// protocol.
//
// Detection is behaviour based rather than name based: USB CDC port names are
// assigned by the OS, so matching on "COM" tells us nothing about whether the
// other end is our firmware. Speaking the protocol is the only reliable signal.
func FindDevice(ports []string, cfg ProbeConfig) (Candidate, []ProbeResult, error) {
	return findDevice(ports, cfg, probePort)
}

// prober tests one port, returning the geometry it announced.
type prober func(port string, cfg ProbeConfig) (Hello, error)

// findDevice is the injectable core of FindDevice, so ordering and retry logic
// can be verified without real serial hardware.
func findDevice(ports []string, cfg ProbeConfig, probe prober) (Candidate, []ProbeResult, error) {
	if cfg.Baud <= 0 {
		cfg.Baud = 115200
	}
	if cfg.Timeout <= 0 {
		cfg.Timeout = DefaultProbeConfig().Timeout
	}

	var rejections []ProbeResult
	for _, port := range ports {
		hello, err := probe(port, cfg)
		if err != nil {
			rejections = append(rejections, ProbeResult{Port: port, Err: err})
			continue
		}
		return Candidate{Port: port, Hello: hello}, rejections, nil
	}

	return Candidate{}, rejections, fmt.Errorf("device: no port responded to the protocol handshake (%d tried)", len(ports))
}

// probePort opens one port, sends Ping and waits for Pong or any device frame.
func probePort(port string, cfg ProbeConfig) (Hello, error) {
	transport, err := OpenSerial(SerialConfig{Port: port, Baud: cfg.Baud})
	if err != nil {
		return Hello{}, err
	}
	defer func() { _ = transport.Close() }()

	link := NewLink(transport)
	if err := link.SendPing(); err != nil {
		return Hello{}, fmt.Errorf("send ping: %w", err)
	}

	deadline := time.Now().Add(cfg.Timeout)
	for time.Now().Before(deadline) {
		frame, err := link.ReadFrame()
		if err != nil {
			// A read timeout just means no answer yet; keep waiting until the
			// overall deadline expires.
			if isTimeout(err) {
				continue
			}
			return Hello{}, fmt.Errorf("read: %w", err)
		}

		switch frame.Type {
		case MessageHello:
			hello, err := DecodeHello(frame.Payload)
			if err != nil {
				return Hello{}, fmt.Errorf("decode hello: %w", err)
			}
			return hello, nil
		case MessagePong:
			// Pong confirms the protocol but carries no geometry; the device
			// mirrors a Hello shortly after, so accept the port and let the
			// session fill in the details.
			return Hello{}, nil
		case MessageScreen, MessageNotice:
			return Hello{}, nil
		}
	}

	return Hello{}, fmt.Errorf("no response within %s", cfg.Timeout)
}

// SortedPorts returns the available ports in a stable, human friendly order.
func SortedPorts() ([]string, error) {
	ports, err := ListPorts()
	if err != nil {
		return nil, err
	}
	sort.Strings(ports)
	return ports, nil
}
