package ui

import (
	"strings"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/user/lora-tui/internal/controller"
	"github.com/user/lora-tui/internal/theme"
	"github.com/user/lora-tui/internal/ui/components"
	"github.com/user/lora-tui/internal/ui/views"
)

const (
	TotalWidth  = 80
	TotalHeight = 24
)

type AppModel struct {
	controller   controller.Controller
	activeTab    int
	messagesView *views.MessagesView
	contactsView *views.ContactsView
	settingsView *views.SettingsView
	inviteView   *views.InviteView
}

func NewAppModel(c controller.Controller) *AppModel {
	return &AppModel{
		controller:   c,
		activeTab:    0,
		messagesView: views.NewMessagesView(c),
		contactsView: views.NewContactsView(c),
		settingsView: views.NewSettingsView(c),
		inviteView:   views.NewInviteView(c),
	}
}

func (m *AppModel) Init() tea.Cmd {
	return tea.EnterAltScreen
}

func (m *AppModel) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	var cmds []tea.Cmd

	switch msg := msg.(type) {
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c":
			_ = m.controller.Save()
			return m, tea.Quit
		case "f1":
			m.activeTab = 0
			return m, nil
		case "f2":
			m.activeTab = 1
			return m, nil
		case "f3":
			m.activeTab = 2
			return m, nil
		case "f4":
			m.activeTab = 3
			return m, nil
		}
	}

	var navAction string
	var subCmd tea.Cmd

	switch m.activeTab {
	case 0:
		subCmd, navAction = m.messagesView.Update(msg)
	case 1:
		subCmd, navAction = m.contactsView.Update(msg)
	case 2:
		subCmd, navAction = m.settingsView.Update(msg)
	case 3:
		subCmd, navAction = m.inviteView.Update(msg)
	}

	if subCmd != nil {
		cmds = append(cmds, subCmd)
	}

	switch navAction {
	case "navigate:messages":
		m.activeTab = 0
	case "navigate:contacts":
		m.activeTab = 1
	case "navigate:settings":
		m.activeTab = 2
	case "navigate:invite":
		m.activeTab = 3
	}

	return m, tea.Batch(cmds...)
}

func (m *AppModel) View() string {
	th := theme.Default

	header := components.RenderHeader(
		m.activeTab,
		m.controller.GetIdentity(),
		TotalWidth,
		th,
	)

	var content string
	switch m.activeTab {
	case 0:
		content = m.messagesView.View(th)
	case 1:
		content = m.contactsView.View(th)
	case 2:
		content = m.settingsView.View(th)
	case 3:
		content = m.inviteView.View(th)
	}

	statusBar := components.RenderStatusBar(
		m.controller.GetNotice(),
		TotalWidth,
		th,
	)

	allLines := append(
		strings.Split(header, "\n"),
		append(strings.Split(content, "\n"), strings.Split(statusBar, "\n")...)...,
	)

	for len(allLines) < TotalHeight {
		allLines = append(allLines, "")
	}
	if len(allLines) > TotalHeight {
		allLines = allLines[:TotalHeight]
	}

	return strings.Join(allLines, "\n")
}
