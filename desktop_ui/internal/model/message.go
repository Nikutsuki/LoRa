package model

import "time"

type Message struct {
	ID         string    `json:"id"`
	ContactID  string    `json:"contact_id"`
	SenderNick string    `json:"sender_nick"`
	IsOutgoing bool      `json:"is_outgoing"`
	Text       string    `json:"text"`
	Sequence   uint32    `json:"sequence"`
	Timestamp  time.Time `json:"timestamp"`
}

const MaxPlainTextLength = 96
