package device

import (
	"fmt"
	"time"

	"go.bug.st/serial"
)

// SerialConfig describes how to open the device link.
type SerialConfig struct {
	Port  string
	Baud  int
	Debug bool
}

// OpenSerial connects to the device over a real serial port.
func OpenSerial(cfg SerialConfig) (Transport, error) {
	if cfg.Port == "" {
		return nil, fmt.Errorf("device: no serial port specified")
	}
	if cfg.Baud <= 0 {
		cfg.Baud = 115200
	}

	mode := &serial.Mode{
		BaudRate: cfg.Baud,
		DataBits: 8,
		Parity:   serial.NoParity,
		StopBits: serial.OneStopBit,
	}

	port, err := serial.Open(cfg.Port, mode)
	if err != nil {
		return nil, fmt.Errorf("device: open %s: %w", cfg.Port, err)
	}
	if err := port.SetReadTimeout(100 * time.Millisecond); err != nil {
		_ = port.Close()
		return nil, fmt.Errorf("device: configure %s: %w", cfg.Port, err)
	}

	return port, nil
}

// ListPorts enumerates the serial ports available on this machine.
func ListPorts() ([]string, error) {
	ports, err := serial.GetPortsList()
	if err != nil {
		return nil, fmt.Errorf("device: list ports: %w", err)
	}
	return ports, nil
}
