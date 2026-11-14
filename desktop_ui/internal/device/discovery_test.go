package device

import (
	"errors"
	"testing"
)

// TestFindDevicePicksFirstResponsivePort verifies probing continues past silent
// ports and stops at the first one that answers.
func TestFindDevicePicksFirstResponsivePort(t *testing.T) {
	silent := errors.New("no response")

	var probed []string
	probe := func(port string, _ ProbeConfig) (Hello, error) {
		probed = append(probed, port)
		if port == "COM7" {
			return Hello{Width: 320, Height: 240, ProtocolVersion: 1}, nil
		}
		return Hello{}, silent
	}

	candidate, rejections, err := findDevice([]string{"COM1", "COM3", "COM7", "COM9"}, DefaultProbeConfig(), probe)
	if err != nil {
		t.Fatalf("findDevice: %v", err)
	}
	if candidate.Port != "COM7" {
		t.Fatalf("port = %q, want COM7", candidate.Port)
	}
	if candidate.Hello.Width != 320 || candidate.Hello.Height != 240 {
		t.Fatalf("hello = %+v", candidate.Hello)
	}

	// Probing must stop once a device answers.
	if len(probed) != 3 {
		t.Fatalf("probed %v, want exactly 3 ports", probed)
	}
	if len(rejections) != 2 {
		t.Fatalf("rejections = %d, want 2", len(rejections))
	}
}

// TestFindDeviceReportsAllRejections ensures the caller can explain every port
// that failed, which --list-ports relies on.
func TestFindDeviceReportsAllRejections(t *testing.T) {
	probe := func(string, ProbeConfig) (Hello, error) {
		return Hello{}, errors.New("no response")
	}

	candidate, rejections, err := findDevice([]string{"COM1", "COM2"}, DefaultProbeConfig(), probe)
	if err == nil {
		t.Fatal("expected an error when no port answers")
	}
	if candidate.Port != "" {
		t.Fatalf("port = %q, want empty", candidate.Port)
	}
	if len(rejections) != 2 {
		t.Fatalf("rejections = %d, want 2", len(rejections))
	}
}

// TestFindDeviceRejectsEmptyList reports a clear error when nothing is attached.
func TestFindDeviceRejectsEmptyList(t *testing.T) {
	probe := func(string, ProbeConfig) (Hello, error) {
		t.Fatal("probe must not be called for an empty list")
		return Hello{}, nil
	}
	if _, _, err := findDevice(nil, DefaultProbeConfig(), probe); err == nil {
		t.Fatal("expected an error for an empty port list")
	}
}

// TestFindDeviceFillsDefaults checks an unset config is normalised.
func TestFindDeviceFillsDefaults(t *testing.T) {
	var seen ProbeConfig
	probe := func(_ string, cfg ProbeConfig) (Hello, error) {
		seen = cfg
		return Hello{}, nil
	}

	if _, _, err := findDevice([]string{"COM1"}, ProbeConfig{}, probe); err != nil {
		t.Fatalf("findDevice: %v", err)
	}
	if seen.Baud != 115200 {
		t.Fatalf("baud = %d, want 115200", seen.Baud)
	}
	if seen.Timeout != DefaultProbeConfig().Timeout {
		t.Fatalf("timeout = %v", seen.Timeout)
	}
}
