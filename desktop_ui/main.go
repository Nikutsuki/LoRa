package main

import (
	"flag"
	"fmt"
	"os"
	"time"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/device"
	"github.com/user/lora-tui/internal/ui"
)

var version = "1.2.0"

func main() {
	storagePath := flag.String("storage", "lora_storage.json", "path to the local state file")
	nickname := flag.String("nick", "", "override the device nickname")
	port := flag.String("port", "", "serial port of the Pico (skips auto-detection)")
	baud := flag.Int("baud", 115200, "serial baud rate")
	autoDetect := flag.Bool("auto", true, "probe serial ports and connect to the device")
	probeTimeout := flag.Duration("probe-timeout", 600*time.Millisecond, "how long each port gets to answer")
	listPorts := flag.Bool("list-ports", false, "list serial ports and exit")
	showVersion := flag.Bool("version", false, "print version and exit")
	flag.Parse()

	if *showVersion {
		fmt.Printf("LoRa Messenger TUI v%s\n", version)
		os.Exit(0)
	}

	if *listPorts {
		printPorts(*baud, *probeTimeout)
		return
	}

	if *port != "" {
		runDeviceMode(*port, *baud)
		return
	}

	if *autoDetect {
		if port, ok := detectPort(*baud, *probeTimeout); ok {
			runDeviceMode(port, *baud)
			return
		}
		fmt.Fprintln(os.Stderr, "No device found; starting offline demo (use --list-ports to inspect).")
	}

	runLocalMode(*storagePath, *nickname)
}

// printPorts lists every serial port, marking whether it speaks our protocol.
func printPorts(baud int, timeout time.Duration) {
	ports, err := device.SortedPorts()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
	if len(ports) == 0 {
		fmt.Println("No serial ports found.")
		return
	}

	cfg := device.DefaultProbeConfig()
	cfg.Baud = baud
	cfg.Timeout = timeout

	candidate, rejected, _ := device.FindDevice(ports, cfg)
	rejections := make(map[string]error, len(rejected))
	for _, r := range rejected {
		rejections[r.Port] = r.Err
	}

	for _, p := range ports {
		if p == candidate.Port {
			fmt.Printf("%s  <- LoRa device\n", p)
			continue
		}
		reason := rejections[p]
		if reason == nil {
			reason = fmt.Errorf("not probed")
		}
		fmt.Printf("%s  (%v)\n", p, reason)
	}
}

// detectPort probes all ports and reports the first one speaking the protocol.
func detectPort(baud int, timeout time.Duration) (string, bool) {
	ports, err := device.SortedPorts()
	if err != nil || len(ports) == 0 {
		return "", false
	}

	cfg := device.DefaultProbeConfig()
	cfg.Baud = baud
	cfg.Timeout = timeout

	candidate, _, err := device.FindDevice(ports, cfg)
	if err != nil {
		return "", false
	}
	return candidate.Port, true
}

// runDeviceMode mirrors the physical device and forwards keys over UART.
func runDeviceMode(port string, baud int) {
	transport, err := device.OpenSerial(device.SerialConfig{Port: port, Baud: baud})
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}

	session := device.Start(transport)
	defer session.Close()

	program := tea.NewProgram(
		ui.NewDeviceModel(session),
		tea.WithAltScreen(),
	)

	if _, err := program.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}

// runLocalMode keeps the original offline demo behaviour.
func runLocalMode(storagePath, nickname string) {
	appController, err := controller.NewAppController(storagePath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
	defer appController.Close()

	if nickname != "" {
		_ = appController.SetNickname(nickname)
	}

	program := tea.NewProgram(
		ui.NewAppModel(appController),
		tea.WithAltScreen(),
	)

	if _, err := program.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}
