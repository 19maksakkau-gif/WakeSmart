

### 1. `alarm_python.py`

```python
# alarm_python.py — умный будильник на Python (Tkinter GUI)

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import threading
import time
import json
import os
import datetime
import random
from playsound import playsound  # для воспроизведения звука

class AlarmClock:
    def __init__(self, root):
        self.root = root
        self.root.title("🌅 WakeSmart — Python")
        self.root.geometry("600x500")
        self.alarms = []  # список словарей с полями: hour, minute, days, melody, gradual_minutes, volume, light
        self.snooze_minutes = 5
        self.current_alarm = None
        self.running = False
        self.playing = False
        self.stop_event = threading.Event()
        self.settings_file = "alarms.json"
        self.load_settings()
        self.create_widgets()
        self.update_clock()
        self.start_checker()

    def create_widgets(self):
        # Верхняя панель с часами
        self.time_label = tk.Label(self.root, font=("Arial", 40), fg="blue")
        self.time_label.pack(pady=10)

        # Панель управления
        control_frame = tk.Frame(self.root)
        control_frame.pack(pady=5)
        tk.Label(control_frame, text="Время (ЧЧ:ММ):").grid(row=0, column=0, padx=5)
        self.time_entry = tk.Entry(control_frame, width=10)
        self.time_entry.grid(row=0, column=1, padx=5)
        self.time_entry.insert(0, "07:30")
        tk.Label(control_frame, text="Дни (пн,вт,ср,чт,пт,сб,вс):").grid(row=0, column=2, padx=5)
        self.days_entry = tk.Entry(control_frame, width=20)
        self.days_entry.grid(row=0, column=3, padx=5)
        self.days_entry.insert(0, "пн,вт,ср,чт,пт")

        btn_frame = tk.Frame(self.root)
        btn_frame.pack(pady=5)
        tk.Button(btn_frame, text="Добавить будильник", command=self.add_alarm).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Удалить выбранный", command=self.remove_alarm).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Остановить звонок", command=self.stop_alarm).pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Отложить (Snooze)", command=self.snooze).pack(side=tk.LEFT, padx=5)

        # Список будильников
        self.alarm_listbox = tk.Listbox(self.root, height=10)
        self.alarm_listbox.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.alarm_listbox.bind('<<ListboxSelect>>', self.on_select)

        # Информация о выбранном
        self.info_text = tk.Text(self.root, height=4, state=tk.DISABLED)
        self.info_text.pack(fill=tk.X, padx=10, pady=5)

        # Статус
        self.status = tk.Label(self.root, text="Готов", anchor=tk.W)
        self.status.pack(fill=tk.X, padx=10)

        self.refresh_list()

    def update_clock(self):
        now = datetime.datetime.now()
        self.time_label.config(text=now.strftime("%H:%M:%S"))
        self.root.after(1000, self.update_clock)

    def add_alarm(self):
        time_str = self.time_entry.get().strip()
        try:
            hour, minute = map(int, time_str.split(':'))
            if not (0 <= hour < 24 and 0 <= minute < 60):
                raise ValueError
        except:
            messagebox.showerror("Ошибка", "Введите время в формате ЧЧ:ММ")
            return
        days_str = self.days_entry.get().strip().lower()
        days = [d.strip() for d in days_str.split(',') if d.strip()]
        # Проверка дней
        valid_days = {'пн','вт','ср','чт','пт','сб','вс'}
        for d in days:
            if d not in valid_days:
                messagebox.showerror("Ошибка", f"Некорректный день: {d}")
                return
        melody = filedialog.askopenfilename(filetypes=[("Audio files", "*.mp3 *.wav *.ogg")])
        if not melody:
            melody = "default"  # встроенный звук
        volume = 80  # по умолчанию
        gradual = 5  # минут
        light = True
        alarm = {
            'id': random.randint(1000, 9999),
            'hour': hour,
            'minute': minute,
            'days': days,
            'melody': melody,
            'volume': volume,
            'gradual_minutes': gradual,
            'light': light,
            'enabled': True
        }
        self.alarms.append(alarm)
        self.save_settings()
        self.refresh_list()
        self.status.config(text=f"Будильник добавлен на {time_str}")

    def remove_alarm(self):
        selection = self.alarm_listbox.curselection()
        if not selection:
            return
        index = selection[0]
        del self.alarms[index]
        self.save_settings()
        self.refresh_list()
        self.status.config(text="Будильник удалён")

    def refresh_list(self):
        self.alarm_listbox.delete(0, tk.END)
        for i, a in enumerate(self.alarms):
            days = ','.join(a['days']) if a['days'] else 'все'
            text = f"{a['hour']:02d}:{a['minute']:02d} [{days}]"
            self.alarm_listbox.insert(tk.END, text)

    def on_select(self, event):
        selection = self.alarm_listbox.curselection()
        if selection:
            index = selection[0]
            a = self.alarms[index]
            info = f"Время: {a['hour']:02d}:{a['minute']:02d}\nДни: {', '.join(a['days']) if a['days'] else 'все'}\nМелодия: {os.path.basename(a['melody']) if a['melody'] != 'default' else 'встроенная'}\nГромкость: {a['volume']}%\nНарастание: {a['gradual_minutes']} мин"
            self.info_text.config(state=tk.NORMAL)
            self.info_text.delete(1.0, tk.END)
            self.info_text.insert(tk.END, info)
            self.info_text.config(state=tk.DISABLED)

    def start_checker(self):
        def check():
            while True:
                now = datetime.datetime.now()
                for a in self.alarms:
                    if a['enabled'] and now.hour == a['hour'] and now.minute == a['minute'] and now.second == 0:
                        # Проверка дня
                        if a['days']:
                            weekdays_rus = ['пн','вт','ср','чт','пт','сб','вс']
                            today = weekdays_rus[now.weekday()]
                            if today not in a['days']:
                                continue
                        # Срабатывание будильника
                        self.trigger_alarm(a)
                time.sleep(1)
        threading.Thread(target=check, daemon=True).start()

    def trigger_alarm(self, alarm):
        self.current_alarm = alarm
        self.playing = True
        self.stop_event.clear()
        self.status.config(text=f"🔔 Будильник сработал! {alarm['hour']:02d}:{alarm['minute']:02d}")
        # Постепенное увеличение громкости (имитация)
        def play():
            volume = 0
            step = alarm['volume'] / (alarm['gradual_minutes'] * 60)  # увеличение за время
            while volume < alarm['volume'] and not self.stop_event.is_set():
                # В реальности здесь управление громкостью системы, но мы просто выводим сообщение
                self.status.config(text=f"🔊 Громкость: {int(volume)}%")
                # Воспроизведение звука (можно использовать playsound с громкостью, но это сложно)
                if alarm['melody'] != 'default' and os.path.exists(alarm['melody']):
                    playsound(alarm['melody'])
                else:
                    # Встроенный звук: просто beep
                    print('\a', end='', flush=True)
                volume += step * 1  # упрощённо
                time.sleep(1)
            # После достижения громкости продолжаем играть до остановки
            while not self.stop_event.is_set():
                if alarm['melody'] != 'default' and os.path.exists(alarm['melody']):
                    playsound(alarm['melody'])
                else:
                    print('\a', end='', flush=True)
                time.sleep(2)
        threading.Thread(target=play, daemon=True).start()

    def stop_alarm(self):
        if self.playing:
            self.stop_event.set()
            self.playing = False
            self.current_alarm = None
            self.status.config(text="Будильник остановлен")

    def snooze(self):
        if self.current_alarm and self.playing:
            self.stop_event.set()
            self.playing = False
            # Перенести будильник на snooze_minutes минут позже
            now = datetime.datetime.now()
            new_time = now + datetime.timedelta(minutes=self.snooze_minutes)
            self.current_alarm['hour'] = new_time.hour
            self.current_alarm['minute'] = new_time.minute
            self.save_settings()
            self.refresh_list()
            self.status.config(text=f"Отложено на {self.snooze_minutes} минут")
            self.current_alarm = None

    def load_settings(self):
        if os.path.exists(self.settings_file):
            with open(self.settings_file, 'r') as f:
                data = json.load(f)
                self.alarms = data.get('alarms', [])
                self.snooze_minutes = data.get('snooze_minutes', 5)

    def save_settings(self):
        data = {
            'alarms': self.alarms,
            'snooze_minutes': self.snooze_minutes
        }
        with open(self.settings_file, 'w') as f:
            json.dump(data, f, indent=2)

if __name__ == "__main__":
    root = tk.Tk()
    app = AlarmClock(root)
    root.mainloop()
