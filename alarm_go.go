// alarm_go.go — умный будильник на Go (консоль + звук через системные утилиты)

package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

type Alarm struct {
	ID              int      `json:"id"`
	Hour            int      `json:"hour"`
	Minute          int      `json:"minute"`
	Days            []string `json:"days"`
	Melody          string   `json:"melody"`
	Volume          int      `json:"volume"`
	GradualMinutes  int      `json:"gradual_minutes"`
	Light           bool     `json:"light"`
	Enabled         bool     `json:"enabled"`
}

type Manager struct {
	Alarms       []Alarm `json:"alarms"`
	SnoozeMinutes int     `json:"snooze_minutes"`
	playing      bool
	stopEvent    chan bool
	currentAlarm *Alarm
	settingsFile string
}

func NewManager() *Manager {
	m := &Manager{
		SnoozeMinutes: 5,
		stopEvent:     make(chan bool),
		settingsFile:  "alarms.json",
	}
	m.load()
	return m
}

func (m *Manager) load() {
	file, err := os.ReadFile(m.settingsFile)
	if err != nil {
		return
	}
	var data map[string]interface{}
	json.Unmarshal(file, &data)
	if snooze, ok := data["snooze_minutes"].(float64); ok {
		m.SnoozeMinutes = int(snooze)
	}
	if alarmsData, ok := data["alarms"].([]interface{}); ok {
		for _, v := range alarmsData {
			b, _ := json.Marshal(v)
			var a Alarm
			json.Unmarshal(b, &a)
			m.Alarms = append(m.Alarms, a)
		}
	}
}

func (m *Manager) save() {
	data := map[string]interface{}{
		"snooze_minutes": m.SnoozeMinutes,
		"alarms":         m.Alarms,
	}
	file, _ := json.MarshalIndent(data, "", "  ")
	os.WriteFile(m.settingsFile, file, 0644)
}

func (m *Manager) add(alarm Alarm) {
	m.Alarms = append(m.Alarms, alarm)
	m.save()
}

func (m *Manager) remove(index int) {
	m.Alarms = append(m.Alarms[:index], m.Alarms[index+1:]...)
	m.save()
}

func (m *Manager) list() string {
	var sb strings.Builder
	for i, a := range m.Alarms {
		days := strings.Join(a.Days, ",")
		if days == "" {
			days = "все"
		}
		sb.WriteString(fmt.Sprintf("  [%d] %02d:%02d (%s)\n", i, a.Hour, a.Minute, days))
	}
	return sb.String()
}

func (m *Manager) triggerAlarm(index int) {
	if index < 0 || index >= len(m.Alarms) {
		return
	}
	a := &m.Alarms[index]
	m.currentAlarm = a
	m.playing = true
	m.stopEvent = make(chan bool)
	fmt.Printf("🔔 Будильник сработал! %02d:%02d\n", a.Hour, a.Minute)
	go func() {
		volume := 0
		step := a.Volume / (a.GradualMinutes * 60)
		for volume < a.Volume && !m.isStopped() {
			fmt.Printf("🔊 Громкость: %d%%\n", volume)
			m.playSound(a.Melody)
			volume += step * 1
			time.Sleep(1 * time.Second)
		}
		for !m.isStopped() {
			m.playSound(a.Melody)
			time.Sleep(2 * time.Second)
		}
		fmt.Println("Звонок остановлен.")
	}()
}

func (m *Manager) playSound(melody string) {
	if melody != "default" && fileExists(melody) {
		// Используем системный плеер
		cmd := exec.Command("afplay", melody) // для macOS; для Linux можно "aplay", для Windows "start"
		cmd.Run()
	} else {
		// Системный beep: вывод управляющего символа
		fmt.Print("\a")
	}
}

func (m *Manager) isStopped() bool {
	select {
	case <-m.stopEvent:
		return true
	default:
		return false
	}
}

func (m *Manager) stop() {
	if m.playing {
		close(m.stopEvent)
		m.playing = false
		m.currentAlarm = nil
	}
}

func (m *Manager) snooze() {
	if m.currentAlarm != nil && m.playing {
		m.stop()
		now := time.Now().Add(time.Duration(m.SnoozeMinutes) * time.Minute)
		m.currentAlarm.Hour = now.Hour()
		m.currentAlarm.Minute = now.Minute()
		m.save()
		fmt.Printf("Отложено на %d минут\n", m.SnoozeMinutes)
		m.currentAlarm = nil
	}
}

func fileExists(path string) bool {
	_, err := os.Stat(path)
	return !os.IsNotExist(err)
}

func main() {
	manager := NewManager()
	scanner := bufio.NewScanner(os.Stdin)
	fmt.Println("🌅 WakeSmart — Go Edition")
	fmt.Println("Текущее время:", time.Now().Format("15:04:05"))
	fmt.Println("Команды: set <ЧЧ:ММ> [дни], list, remove <индекс>, stop, snooze, exit")
	go func() {
		// Проверка будильников каждую секунду
		for {
			now := time.Now()
			for i, a := range manager.Alarms {
				if !a.Enabled {
					continue
				}
				if now.Hour() == a.Hour && now.Minute() == a.Minute && now.Second() == 0 {
					if len(a.Days) > 0 {
						weekdays := []string{"пн", "вт", "ср", "чт", "пт", "сб", "вс"}
						today := weekdays[now.Weekday()-1]
						found := false
						for _, d := range a.Days {
							if d == today {
								found = true
								break
							}
						}
						if !found {
							continue
						}
					}
					manager.triggerAlarm(i)
				}
			}
			time.Sleep(1 * time.Second)
		}
	}()

	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}
		line := strings.TrimSpace(scanner.Text())
		parts := strings.SplitN(line, " ", 2)
		cmd := parts[0]
		var arg string
		if len(parts) > 1 {
			arg = parts[1]
		}
		switch cmd {
		case "set":
			// формат: set 07:30 пн,вт,ср,чт,пт
			subParts := strings.SplitN(arg, " ", 2)
			timeStr := subParts[0]
			daysStr := ""
			if len(subParts) > 1 {
				daysStr = subParts[1]
			}
			timeParts := strings.Split(timeStr, ":")
			if len(timeParts) != 2 {
				fmt.Println("Неверный формат времени. Используйте ЧЧ:ММ")
				continue
			}
			hour, _ := strconv.Atoi(timeParts[0])
			minute, _ := strconv.Atoi(timeParts[1])
			if hour < 0 || hour > 23 || minute < 0 || minute > 59 {
				fmt.Println("Некорректное время")
				continue
			}
			var days []string
			if daysStr != "" {
				days = strings.Split(daysStr, ",")
				for _, d := range days {
					d = strings.TrimSpace(d)
					if !contains([]string{"пн","вт","ср","чт","пт","сб","вс"}, d) {
						fmt.Println("Некорректный день:", d)
						continue
					}
				}
			}
			melody := "default"
			// можно запросить мелодию отдельно (упрощённо)
			alarm := Alarm{
				ID:             time.Now().Nanosecond(),
				Hour:           hour,
				Minute:         minute,
				Days:           days,
				Melody:         melody,
				Volume:         80,
				GradualMinutes: 5,
				Light:          true,
				Enabled:        true,
			}
			manager.add(alarm)
			fmt.Println("Будильник добавлен.")
		case "list":
			fmt.Println(manager.list())
		case "remove":
			idx, err := strconv.Atoi(arg)
			if err != nil || idx < 0 || idx >= len(manager.Alarms) {
				fmt.Println("Неверный индекс")
				continue
			}
			manager.remove(idx)
			fmt.Println("Удалён.")
		case "stop":
			manager.stop()
			fmt.Println("Будильник остановлен.")
		case "snooze":
			manager.snooze()
		case "exit":
			manager.save()
			fmt.Println("До свидания!")
			return
		default:
			fmt.Println("Неизвестная команда")
		}
	}
}

func contains(slice []string, item string) bool {
	for _, s := range slice {
		if s == item {
			return true
		}
	}
	return false
}
