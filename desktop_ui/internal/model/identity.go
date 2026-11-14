package model

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"strings"
)

type DeviceIdentity struct {
	DeviceID      string `json:"device_id"`
	Nickname      string `json:"nickname"`
	SigningPubKey string `json:"signing_pub_key"`
	BoxPubKey     string `json:"box_pub_key"`
	Fingerprint   string `json:"fingerprint"`
	IsDevIdentity bool   `json:"is_dev_identity"`
}

func FormatFingerprint(rawHex string) string {
	clean := strings.ToUpper(strings.ReplaceAll(strings.ReplaceAll(rawHex, "-", ""), ":", ""))
	if len(clean) > 16 {
		clean = clean[:16]
	}
	for len(clean) < 16 {
		clean += "0"
	}
	return fmt.Sprintf("%s-%s-%s-%s", clean[0:4], clean[4:8], clean[8:12], clean[12:16])
}

func GenerateRandomHex(n int) string {
	b := make([]byte, n)
	_, _ = rand.Read(b)
	return hex.EncodeToString(b)
}

func GenerateRandomFingerprint() string {
	return FormatFingerprint(GenerateRandomHex(8))
}

func GenerateRandomDeviceID() string {
	b := make([]byte, 8)
	_, _ = rand.Read(b)
	var parts []string
	for _, byteVal := range b {
		parts = append(parts, fmt.Sprintf("%02X", byteVal))
	}
	return strings.Join(parts, ":")
}
