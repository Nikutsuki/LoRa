package model

import "time"

type Contact struct {
	ID          string    `json:"id"`
	Nickname    string    `json:"nickname"`
	DeviceID    string    `json:"device_id"`
	Fingerprint string    `json:"fingerprint"`
	BoxPubKey   string    `json:"box_pub_key"`
	Status      string    `json:"status"`
	LastSeen    time.Time `json:"last_seen"`
	Valid       bool      `json:"valid"`
}

func (c *Contact) DisplayFingerprint() string {
	if c.Fingerprint != "" {
		return c.Fingerprint
	}
	return "UNKNOWN"
}

func (c *Contact) DisplayTitle() string {
	if c == nil || !c.Valid || c.Nickname == "" {
		return "-"
	}
	return c.Nickname
}

func (c *Contact) DisplaySubtitle() string {
	if c == nil || !c.Valid {
		return ""
	}
	if c.Status != "" {
		return c.Fingerprint + "  |  " + c.Status
	}
	return c.Fingerprint
}
