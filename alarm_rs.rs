// alarm_rs.rs — умный будильник на Rust (консоль + звук через системный beep)

use std::collections::HashMap;
use std::fs;
use std::io::{self, Write, BufRead};
use std::time::{Duration, Instant};
use std::thread;
use serde::{Serialize, Deserialize};
use std::process::Command;

const SETTINGS_FILE: &str = "alarms.json";

#[derive(Serialize, Deserialize, Clone)]
struct Alarm {
    id: u32,
    hour: u8,
    minute: u8,
    days: Vec<String>,
    melody: String,
    volume: u8,
    gradual_minutes: u8,
    light: bool,
    enabled: bool,
}

struct Manager {
    alarms: Vec<Alarm>,
    snooze_minutes: u8,
    playing: bool,
    stop_flag: bool,
    current_alarm: Option<Alarm>,
}

impl Manager {
    fn new() -> Self {
        let mut m = Manager {
            alarms: Vec::new(),
            snooze_minutes: 5,
            playing: false,
            stop_flag: false,
            current_alarm: None,
        };
        m.load();
        m
    }

    fn load(&mut self) {
        if let Ok(content) = fs::read_to_string(SETTINGS_FILE) {
            if let Ok(data) = serde_json::from_str::<HashMap<String, serde_json::Value>>(&content) {
                if let Some(snooze) = data.get("snooze_minutes").and_then(|v| v.as_u64()) {
                    self.snooze_minutes = snooze as u8;
                }
                if let Some(arr) = data.get("alarms").and_then(|v| v.as_array()) {
                    for item in arr {
                        if let Ok(a) = serde_json::from_value::<Alarm>(item.clone()) {
                            self.alarms.push(a);
                        }
                    }
                }
            }
        }
    }

    fn save(&self) {
        let mut map = HashMap::new();
        map.insert("snooze_minutes".to_string(), serde_json::json!(self.snooze_minutes));
        map.insert("alarms".to_string(), serde_json::json!(self.alarms));
        let json = serde_json::to_string_pretty(&map).unwrap();
        fs::write(SETTINGS_FILE, json).unwrap();
    }

    fn add(&mut self, alarm: Alarm) {
        self.alarms.push(alarm);
        self.save();
    }

    fn remove(&mut self, index: usize) {
        if index < self.alarms.len() {
            self.alarms.remove(index);
            self.save();
        }
    }

    fn list(&self) -> String {
        let mut s = String::new();
        for (i, a) in self.alarms.iter().enumerate() {
            let days = if a.days.is_empty() { "все".to_string() } else { a.days.join(",") };
            s.push_str(&format!("  [{}] {:02}:{:02} ({})\n", i, a.hour, a.minute, days));
        }
        s
    }

    fn trigger_alarm(&mut self, index: usize) {
        if index >= self.alarms.len() { return; }
        let alarm = self.alarms[index].clone();
        self.current_alarm = Some(alarm.clone());
        self.playing = true;
        self.stop_flag = false;
        println!("🔔 Будильник сработал! {:02}:{:02}", alarm.hour, alarm.minute);
        let stop_flag = self.stop_flag.clone();
        thread::spawn(move || {
            let mut volume = 0;
            let step = alarm.volume as f32 / (alarm.gradual_minutes as f32 * 60.0);
            while volume < alarm.volume && !stop_flag {
                println!("🔊 Громкость: {}%", volume);
                play_sound(&alarm.melody);
                volume = (volume as f32 + step * 1.0) as u8;
                thread::sleep(Duration::from_secs(1));
            }
            while !stop_flag {
                play_sound(&alarm.melody);
                thread::sleep(Duration::from_secs(2));
            }
        });
    }

    fn stop(&mut self) {
        if self.playing {
            self.stop_flag = true;
            self.playing = false;
            self.current_alarm = None;
            println!("Будильник остановлен.");
        }
    }

    fn snooze(&mut self) {
        if let Some(ref mut alarm) = self.current_alarm {
            if self.playing {
                self.stop();
                let now = chrono::Local::now() + chrono::Duration::minutes(self.snooze_minutes as i64);
                alarm.hour = now.hour() as u8;
                alarm.minute = now.minute() as u8;
                // обновляем в списке
                if let Some(pos) = self.alarms.iter().position(|a| a.id == alarm.id) {
                    self.alarms[pos] = alarm.clone();
                }
                self.save();
                println!("Отложено на {} минут", self.snooze_minutes);
                self.current_alarm = None;
            }
        }
    }
}

fn play_sound(melody: &str) {
    if melody != "default" && std::path::Path::new(melody).exists() {
        // Попытка воспроизвести через системный плеер
        let _ = Command::new("afplay").arg(melody).status(); // macOS
        // для Linux: aplay, для Windows: start
    } else {
        print!("\x07"); // системный beep
        io::stdout().flush().unwrap();
    }
}

fn main() {
    let mut manager = Manager::new();
    let stdin = io::stdin();
    let mut reader = stdin.lock();

    println!("🌅 WakeSmart — Rust Edition");
    println!("Текущее время: {}", chrono::Local::now().format("%H:%M:%S"));
    println!("Команды: set <ЧЧ:ММ> [дни], list, remove <индекс>, stop, snooze, exit");

    // Запуск проверки будильников в отдельном потоке
    let manager_clone = std::cell::RefCell::new(manager);
    thread::spawn(move || {
        loop {
            thread::sleep(Duration::from_secs(1));
            let now = chrono::Local::now();
            let mut mgr = manager_clone.borrow_mut();
            for i in 0..mgr.alarms.len() {
                let a = &mgr.alarms[i];
                if !a.enabled { continue; }
                if now.hour() == a.hour as u32 && now.minute() == a.minute as u32 && now.second() == 0 {
                    if !a.days.is_empty() {
                        let weekdays = ["пн","вт","ср","чт","пт","сб","вс"];
                        let today = weekdays[now.weekday().num_days_from_monday() as usize];
                        if !a.days.contains(&today.to_string()) {
                            continue;
                        }
                    }
                    mgr.trigger_alarm(i);
                    break;
                }
            }
        }
    });

    loop {
        print!("> ");
        io::stdout().flush().unwrap();
        let mut line = String::new();
        if reader.read_line(&mut line).is_err() { break; }
        let line = line.trim();
        let parts: Vec<&str> = line.splitn(2, ' ').collect();
        let cmd = parts[0];
        let arg = if parts.len() > 1 { parts[1] } else { "" };

        match cmd {
            "set" => {
                // формат: set 07:30 пн,вт,ср,чт,пт
                let sub: Vec<&str> = arg.splitn(2, ' ').collect();
                if sub.is_empty() { println!("Укажите время"); continue; }
                let time_str = sub[0];
                let days_str = if sub.len() > 1 { sub[1] } else { "" };
                let time_parts: Vec<&str> = time_str.split(':').collect();
                if time_parts.len() != 2 {
                    println!("Неверный формат времени");
                    continue;
                }
                let hour: u8 = time_parts[0].parse().unwrap_or(0);
                let minute: u8 = time_parts[1].parse().unwrap_or(0);
                if hour > 23 || minute > 59 {
                    println!("Некорректное время");
                    continue;
                }
                let mut days = Vec::new();
                if !days_str.is_empty() {
                    for d in days_str.split(',') {
                        let d = d.trim();
                        if !["пн","вт","ср","чт","пт","сб","вс"].contains(&d) {
                            println!("Некорректный день: {}", d);
                            continue;
                        }
                        days.push(d.to_string());
                    }
                }
                let alarm = Alarm {
                    id: rand::random::<u32>(),
                    hour,
                    minute,
                    days,
                    melody: "default".to_string(),
                    volume: 80,
                    gradual_minutes: 5,
                    light: true,
                    enabled: true,
                };
                let mut mgr = manager_clone.borrow_mut();
                mgr.add(alarm);
                println!("Будильник добавлен.");
            }
            "list" => {
                let mgr = manager_clone.borrow();
                print!("{}", mgr.list());
            }
            "remove" => {
                let idx: usize = arg.parse().unwrap_or(999);
                let mut mgr = manager_clone.borrow_mut();
                if idx < mgr.alarms.len() {
                    mgr.remove(idx);
                    println!("Удалён.");
                } else {
                    println!("Неверный индекс");
                }
            }
            "stop" => {
                let mut mgr = manager_clone.borrow_mut();
                mgr.stop();
            }
            "snooze" => {
                let mut mgr = manager_clone.borrow_mut();
                mgr.snooze();
            }
            "exit" => {
                let mgr = manager_clone.borrow();
                mgr.save();
                println!("До свидания!");
                break;
            }
            _ => println!("Неизвестная команда"),
        }
    }
}
