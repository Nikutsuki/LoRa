package model

import "time"

func InitialMockState() *AppState {
	now := time.Now()

	identity := DeviceIdentity{
		DeviceID:      "00:1A:4B:82:F0:91:DE:33",
		Nickname:      "pico",
		SigningPubKey: "a4c9b207e99ff81e35d2cba04169720c78a946b5a76985cb2a25597793d5f301",
		BoxPubKey:     "82f1b40ee50cba39488a7c645b20468ea9010bcdef5534149021aaccbb881240",
		Fingerprint:   "8F3C-9A12-4E55-B071",
		IsDevIdentity: true,
	}

	contacts := []*Contact{
		{
			ID:          "contact-alice",
			Nickname:    "Alice",
			DeviceID:    "00:1A:4B:82:F0:91:AA:11",
			Fingerprint: "3F1A-B489-0C21-87ED",
			BoxPubKey:   "11a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6e7f80",
			Status:      "Online",
			LastSeen:    now.Add(-2 * time.Minute),
			Valid:       true,
		},
		{
			ID:          "contact-bob",
			Nickname:    "Bob",
			DeviceID:    "00:1A:4B:82:F0:91:BB:22",
			Fingerprint: "A7D2-11EE-90FC-4421",
			BoxPubKey:   "22b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6e7f8091",
			Status:      "Offline",
			LastSeen:    now.Add(-15 * time.Minute),
			Valid:       true,
		},
		{
			ID:          "contact-charlie",
			Nickname:    "Charlie",
			DeviceID:    "00:1A:4B:82:F0:91:CC:33",
			Fingerprint: "CC40-55B1-2099-AA01",
			BoxPubKey:   "33c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6e7f8091a2",
			Status:      "Online",
			LastSeen:    now.Add(-30 * time.Second),
			Valid:       true,
		},
	}

	messages := map[string][]Message{
		"contact-alice": {
			{
				ID:         "msg-01",
				ContactID:  "contact-alice",
				SenderNick: "Alice",
				IsOutgoing: false,
				Text:       "Hi pico",
				Sequence:   1,
				Timestamp:  now.Add(-10 * time.Minute),
			},
			{
				ID:         "msg-02",
				ContactID:  "contact-alice",
				SenderNick: "pico",
				IsOutgoing: true,
				Text:       "Hi Alice",
				Sequence:   2,
				Timestamp:  now.Add(-9 * time.Minute),
			},
		},
		"contact-bob": {
			{
				ID:         "msg-03",
				ContactID:  "contact-bob",
				SenderNick: "Bob",
				IsOutgoing: false,
				Text:       "Hello",
				Sequence:   1,
				Timestamp:  now.Add(-25 * time.Minute),
			},
		},
	}

	pendingInvite := &Contact{
		ID:          "contact-delta",
		Nickname:    "Delta",
		DeviceID:    "00:1A:4B:82:F0:91:DD:44",
		Fingerprint: "FA91-0022-78B3-33C4",
		BoxPubKey:   "44d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3",
		Status:      "Pending",
		LastSeen:    now.Add(-1 * time.Minute),
		Valid:       true,
	}

	return &AppState{
		Identity:      identity,
		Contacts:      contacts,
		Messages:      messages,
		Settings:      DefaultSettings(),
		PendingInvite: pendingInvite,
		Sequence:      3,
	}
}
