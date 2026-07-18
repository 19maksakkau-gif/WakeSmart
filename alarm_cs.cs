// alarm_cs.cs — умный будильник на C# (WPF)

using System;
using System.Collections.Generic;
using System.IO;
using System.Media;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Threading;
using System.Text.Json;

namespace WakeSmartWPF
{
    public partial class MainWindow : Window
    {
        private List<Alarm> alarms = new List<Alarm>();
        private int snoozeMinutes = 5;
        private bool playing = false;
        private bool stopEvent = false;
        private Alarm currentAlarm = null;
        private DispatcherTimer clockTimer, checkTimer;
        private string settingsFile = "alarms.json";

        // UI elements
        private Label timeLabel;
        private ListBox alarmList;
        private TextBox timeBox, daysBox;
        private TextBox infoBox;
        private Label statusLabel;

        public MainWindow()
        {
            InitializeComponent();
            LoadSettings();
            CreateUI();
            StartClock();
            StartChecker();
        }

        private void CreateUI()
        {
            this.Title = "🌅 WakeSmart — C#";
            this.Width = 700;
            this.Height = 550;
            var grid = new Grid();
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            // Время
            timeLabel = new Label { FontSize = 40, FontWeight = FontWeights.Bold, Foreground = System.Windows.Media.Brushes.Blue, HorizontalAlignment = HorizontalAlignment.Center };
            Grid.SetRow(timeLabel, 0);
            grid.Children.Add(timeLabel);

            // Список
            alarmList = new ListBox();
            alarmList.SelectionChanged += (s, e) => OnSelect();
            Grid.SetRow(alarmList, 1);
            grid.Children.Add(alarmList);

            // Информация
            infoBox = new TextBox { IsReadOnly = true, TextWrapping = TextWrapping.Wrap, Height = 80 };
            Grid.SetRow(infoBox, 2);
            grid.Children.Add(infoBox);

            // Панель управления
            var panel = new StackPanel { Orientation = Orientation.Vertical };
            var topPanel = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(5) };
            topPanel.Children.Add(new Label { Content = "Время:" });
            timeBox = new TextBox { Width = 80, Text = "07:30" };
            topPanel.Children.Add(timeBox);
            topPanel.Children.Add(new Label { Content = "Дни:" });
            daysBox = new TextBox { Width = 150, Text = "пн,вт,ср,чт,пт" };
            topPanel.Children.Add(daysBox);
            panel.Children.Add(topPanel);

            var btnPanel = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(5) };
            var addBtn = new Button { Content = "Добавить", Width = 100 };
            addBtn.Click += (s, e) => AddAlarm();
            var delBtn = new Button { Content = "Удалить", Width = 100 };
            delBtn.Click += (s, e) => RemoveAlarm();
            var stopBtn = new Button { Content = "Остановить", Width = 100 };
            stopBtn.Click += (s, e) => StopAlarm();
            var snoozeBtn = new Button { Content = "Отложить", Width = 100 };
            snoozeBtn.Click += (s, e) => Snooze();
            btnPanel.Children.Add(addBtn);
            btnPanel.Children.Add(delBtn);
            btnPanel.Children.Add(stopBtn);
            btnPanel.Children.Add(snoozeBtn);
            panel.Children.Add(btnPanel);

            statusLabel = new Label { Content = "Готов", Margin = new Thickness(5) };
            panel.Children.Add(statusLabel);
            Grid.SetRow(panel, 3);
            grid.Children.Add(panel);

            this.Content = grid;
            RefreshList();
        }

        private void StartClock()
        {
            clockTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
            clockTimer.Tick += (s, e) => {
                timeLabel.Content = DateTime.Now.ToString("HH:mm:ss");
            };
            clockTimer.Start();
        }

        private void StartChecker()
        {
            checkTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
            checkTimer.Tick += (s, e) => CheckAlarms();
            checkTimer.Start();
        }

        private void CheckAlarms()
        {
            if (playing) return;
            var now = DateTime.Now;
            foreach (var a in alarms)
            {
                if (!a.Enabled) continue;
                if (now.Hour == a.Hour && now.Minute == a.Minute && now.Second == 0)
                {
                    if (a.Days.Count > 0)
                    {
                        string[] weekdays = { "пн", "вт", "ср", "чт", "пт", "сб", "вс" };
                        string today = weekdays[(int)now.DayOfWeek - 1];
                        if (!a.Days.Contains(today)) continue;
                    }
                    TriggerAlarm(a);
                    break;
                }
            }
        }

        private async void TriggerAlarm(Alarm a)
        {
            currentAlarm = a;
            playing = true;
            stopEvent = false;
            statusLabel.Content = $"🔔 Будильник сработал! {a.Hour:D2}:{a.Minute:D2}";
            await Task.Run(() => {
                int volume = 0;
                int step = a.Volume / (a.GradualMinutes * 60);
                while (volume < a.Volume && !stopEvent)
                {
                    Dispatcher.Invoke(() => statusLabel.Content = $"🔊 Громкость: {volume}%");
                    PlaySound(a.Melody);
                    volume += step * 1;
                    Thread.Sleep(1000);
                }
                while (!stopEvent)
                {
                    PlaySound(a.Melody);
                    Thread.Sleep(2000);
                }
            });
        }

        private void PlaySound(string melody)
        {
            try
            {
                if (melody != "default" && File.Exists(melody))
                {
                    using (var player = new SoundPlayer(melody))
                    {
                        player.PlaySync();
                    }
                }
                else
                {
                    System.Media.SystemSounds.Beep.Play();
                }
            }
            catch { System.Media.SystemSounds.Beep.Play(); }
        }

        private void StopAlarm()
        {
            if (playing)
            {
                stopEvent = true;
                playing = false;
                currentAlarm = null;
                statusLabel.Content = "Будильник остановлен";
            }
        }

        private void Snooze()
        {
            if (currentAlarm != null && playing)
            {
                stopEvent = true;
                playing = false;
                var newTime = DateTime.Now.AddMinutes(snoozeMinutes);
                currentAlarm.Hour = newTime.Hour;
                currentAlarm.Minute = newTime.Minute;
                SaveSettings();
                RefreshList();
                statusLabel.Content = $"Отложено на {snoozeMinutes} минут";
                currentAlarm = null;
            }
        }

        private void AddAlarm()
        {
            var timeStr = timeBox.Text.Trim();
            var parts = timeStr.Split(':');
            if (parts.Length != 2 || !int.TryParse(parts[0], out int hour) || !int.TryParse(parts[1], out int minute) || hour < 0 || hour > 23 || minute < 0 || minute > 59)
            {
                MessageBox.Show("Неверный формат времени");
                return;
            }
            var daysStr = daysBox.Text.Trim().ToLower();
            var days = new List<string>(daysStr.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries));
            foreach (var d in days)
            {
                if (!new[] { "пн", "вт", "ср", "чт", "пт", "сб", "вс" }.Contains(d))
                {
                    MessageBox.Show("Некорректный день: " + d);
                    return;
                }
            }
            var dialog = new Microsoft.Win32.OpenFileDialog { Filter = "Audio|*.mp3;*.wav;*.ogg" };
            string melody = "default";
            if (dialog.ShowDialog() == true) melody = dialog.FileName;

            var a = new Alarm
            {
                Id = new Random().Next(1000, 9999),
                Hour = hour,
                Minute = minute,
                Days = days,
                Melody = melody,
                Volume = 80,
                GradualMinutes = 5,
                Light = true,
                Enabled = true
            };
            alarms.Add(a);
            SaveSettings();
            RefreshList();
            statusLabel.Content = "Добавлен будильник на " + timeStr;
        }

        private void RemoveAlarm()
        {
            int idx = alarmList.SelectedIndex;
            if (idx < 0) return;
            alarms.RemoveAt(idx);
            SaveSettings();
            RefreshList();
            statusLabel.Content = "Будильник удалён";
        }

        private void OnSelect()
        {
            int idx = alarmList.SelectedIndex;
            if (idx < 0) return;
            var a = alarms[idx];
            infoBox.Text = $"Время: {a.Hour:D2}:{a.Minute:D2}\nДни: {string.Join(",", a.Days)}\nМелодия: {(a.Melody == "default" ? "встроенная" : System.IO.Path.GetFileName(a.Melody))}\nГромкость: {a.Volume}%\nНарастание: {a.GradualMinutes} мин";
        }

        private void RefreshList()
        {
            alarmList.Items.Clear();
            foreach (var a in alarms)
            {
                string days = string.Join(",", a.Days);
                if (string.IsNullOrEmpty(days)) days = "все";
                alarmList.Items.Add($"{a.Hour:D2}:{a.Minute:D2} [{days}]");
            }
        }

        private void LoadSettings()
        {
            if (File.Exists(settingsFile))
            {
                string json = File.ReadAllText(settingsFile);
                var obj = JsonSerializer.Deserialize<Dictionary<string, object>>(json);
                snoozeMinutes = Convert.ToInt32(obj["snooze_minutes"]);
                var arr = JsonSerializer.Deserialize<List<Alarm>>(obj["alarms"].ToString());
                alarms = arr ?? new List<Alarm>();
            }
        }

        private void SaveSettings()
        {
            var obj = new Dictionary<string, object>
            {
                ["snooze_minutes"] = snoozeMinutes,
                ["alarms"] = alarms
            };
            string json = JsonSerializer.Serialize(obj);
            File.WriteAllText(settingsFile, json);
        }

        public class Alarm
        {
            public int Id { get; set; }
            public int Hour { get; set; }
            public int Minute { get; set; }
            public List<string> Days { get; set; }
            public string Melody { get; set; }
            public int Volume { get; set; }
            public int GradualMinutes { get; set; }
            public bool Light { get; set; }
            public bool Enabled { get; set; }
        }

        [STAThread]
        static void Main()
        {
            var app = new Application();
            app.Run(new MainWindow());
        }
    }
}
