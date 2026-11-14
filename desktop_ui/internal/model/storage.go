package model

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
)

type AppState struct {
	Identity      DeviceIdentity       `json:"identity"`
	Contacts      []*Contact           `json:"contacts"`
	Messages      map[string][]Message `json:"messages"`
	Settings      AppSettings          `json:"settings"`
	PendingInvite *Contact             `json:"pending_invite"`
	Sequence      uint32               `json:"sequence"`
	mu            sync.RWMutex
}

type StorageManager struct {
	filePath string
	mu       sync.Mutex
}

func NewStorageManager(filePath string) *StorageManager {
	if filePath == "" {
		filePath = "lora_storage.json"
	}
	return &StorageManager{filePath: filePath}
}

func (sm *StorageManager) Load() (*AppState, error) {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	data, err := os.ReadFile(sm.filePath)
	if os.IsNotExist(err) {
		initial := InitialMockState()
		_ = sm.saveInternal(initial)
		return initial, nil
	} else if err != nil {
		return nil, fmt.Errorf("read storage error: %w", err)
	}

	var state AppState
	if err := json.Unmarshal(data, &state); err != nil {
		return nil, fmt.Errorf("json parse error: %w", err)
	}

	var cleanContacts []*Contact
	for _, c := range state.Contacts {
		low := strings.ToLower(c.Nickname)
		if strings.Contains(low, "relay") || strings.Contains(low, "gateway") {
			continue
		}
		cleanContacts = append(cleanContacts, c)
	}
	state.Contacts = cleanContacts

	if state.Messages == nil {
		state.Messages = make(map[string][]Message)
	}
	return &state, nil
}

func (sm *StorageManager) Save(state *AppState) error {
	sm.mu.Lock()
	defer sm.mu.Unlock()
	return sm.saveInternal(state)
}

func (sm *StorageManager) saveInternal(state *AppState) error {
	state.mu.RLock()
	defer state.mu.RUnlock()

	data, err := json.MarshalIndent(state, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal error: %w", err)
	}

	dir := filepath.Dir(sm.filePath)
	if dir != "" && dir != "." {
		_ = os.MkdirAll(dir, 0755)
	}

	tmpFile := sm.filePath + ".tmp"
	if err := os.WriteFile(tmpFile, data, 0644); err != nil {
		return fmt.Errorf("write error: %w", err)
	}

	if err := os.Rename(tmpFile, sm.filePath); err != nil {
		_ = os.Remove(sm.filePath)
		return os.Rename(tmpFile, sm.filePath)
	}

	return nil
}

func (sm *StorageManager) GetFilePath() string {
	return sm.filePath
}
