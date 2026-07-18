// alarm_java.java — умный будильник на Java (Swing)

import javax.swing.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.time.*;
import java.time.format.*;
import java.util.*;
import java.util.List;
import com.google.gson.*; // требуется Gson
import javax.sound.sampled.*;
import java.net.URL;

public class AlarmJava extends JFrame {
    private static final String SETTINGS_FILE = "alarms.json";
    private List<Alarm> alarms = new ArrayList<>();
    private int snoozeMinutes = 5;
    private boolean playing = false;
    private boolean stopEvent = false;
    private Alarm currentAlarm = null;
    private Timer clockTimer, checkTimer;

    // GUI components
    private JLabel timeLabel;
    private JList<String> alarmList;
    private DefaultListModel<String> listModel;
    private JTextArea infoArea;
    private JLabel statusLabel;
    private JTextField timeField, daysField;

    public AlarmJava() {
        setTitle("🌅 WakeSmart — Java");
        setSize(700, 550);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setLayout(new BorderLayout());

        loadSettings();
        createMenuBar();
        createUI();
        startClock();
        startChecker();
    }

    private void createMenuBar() {
        JMenuBar menuBar = new JMenuBar();
        JMenu fileMenu = new JMenu("Файл");
        JMenuItem exit = new JMenuItem("Выход");
        exit.addActionListener(e -> { saveSettings(); System.exit(0); });
        fileMenu.add(exit);
        menuBar.add(fileMenu);
        setJMenuBar(menuBar);
    }

    private void createUI() {
        // Север: время
        JPanel north = new JPanel();
        timeLabel = new JLabel();
        timeLabel.setFont(new Font("Arial", Font.BOLD, 40));
        timeLabel.setForeground(Color.BLUE);
        north.add(timeLabel);
        add(north, BorderLayout.NORTH);

        // Центр: список и информация
        JPanel center = new JPanel(new BorderLayout());
        listModel = new DefaultListModel<>();
        alarmList = new JList<>(listModel);
        alarmList.addListSelectionListener(e -> {
            if (!e.getValueIsAdjusting()) onSelect();
        });
        JScrollPane listScroll = new JScrollPane(alarmList);
        center.add(listScroll, BorderLayout.CENTER);

        infoArea = new JTextArea(4, 30);
        infoArea.setEditable(false);
        center.add(new JScrollPane(infoArea), BorderLayout.SOUTH);
        add(center, BorderLayout.CENTER);

        // Юг: управление
        JPanel south = new JPanel(new BorderLayout());
        JPanel topSouth = new JPanel(new FlowLayout());
        topSouth.add(new JLabel("Время:"));
        timeField = new JTextField("07:30", 8);
        topSouth.add(timeField);
        topSouth.add(new JLabel("Дни:"));
        daysField = new JTextField("пн,вт,ср,чт,пт", 15);
        topSouth.add(daysField);
        south.add(topSouth, BorderLayout.NORTH);

        JPanel btnPanel = new JPanel();
        JButton addBtn = new JButton("Добавить");
        JButton delBtn = new JButton("Удалить");
        JButton stopBtn = new JButton("Остановить");
        JButton snoozeBtn = new JButton("Отложить");
        btnPanel.add(addBtn);
        btnPanel.add(delBtn);
        btnPanel.add(stopBtn);
        btnPanel.add(snoozeBtn);
        south.add(btnPanel, BorderLayout.CENTER);

        statusLabel = new JLabel("Готов");
        south.add(statusLabel, BorderLayout.SOUTH);
        add(south, BorderLayout.SOUTH);

        // Обработчики
        addBtn.addActionListener(e -> addAlarm());
        delBtn.addActionListener(e -> removeAlarm());
        stopBtn.addActionListener(e -> stopAlarm());
        snoozeBtn.addActionListener(e -> snooze());

        refreshList();
    }

    private void startClock() {
        clockTimer = new Timer(1000, e -> {
            LocalDateTime now = LocalDateTime.now();
            timeLabel.setText(now.format(DateTimeFormatter.ofPattern("HH:mm:ss")));
        });
        clockTimer.start();
    }

    private void startChecker() {
        checkTimer = new Timer(1000, e -> checkAlarms());
        checkTimer.start();
    }

    private void checkAlarms() {
        if (playing) return;
        LocalDateTime now = LocalDateTime.now();
        for (Alarm a : alarms) {
            if (!a.enabled) continue;
            if (now.getHour() == a.hour && now.getMinute() == a.minute && now.getSecond() == 0) {
                // Проверка дня
                if (!a.days.isEmpty()) {
                    String[] weekdays = {"пн","вт","ср","чт","пт","сб","вс"};
                    String today = weekdays[now.getDayOfWeek().getValue() - 1];
                    if (!a.days.contains(today)) continue;
                }
                triggerAlarm(a);
                break;
            }
        }
    }

    private void triggerAlarm(Alarm alarm) {
        currentAlarm = alarm;
        playing = true;
        stopEvent = false;
        statusLabel.setText("🔔 Будильник сработал! " + String.format("%02d:%02d", alarm.hour, alarm.minute));
        // Запуск потока для звонка
        new Thread(() -> {
            int volume = 0;
            int step = alarm.volume / (alarm.gradualMinutes * 60);
            while (volume < alarm.volume && !stopEvent) {
                statusLabel.setText("🔊 Громкость: " + volume + "%");
                // Воспроизведение звука
                playSound(alarm.melody);
                volume += step * 1;
                try { Thread.sleep(1000); } catch (InterruptedException ignored) {}
            }
            while (!stopEvent) {
                playSound(alarm.melody);
                try { Thread.sleep(2000); } catch (InterruptedException ignored) {}
            }
        }).start();
    }

    private void playSound(String melody) {
        try {
            if (!melody.equals("default") && new File(melody).exists()) {
                AudioInputStream audioIn = AudioSystem.getAudioInputStream(new File(melody));
                Clip clip = AudioSystem.getClip();
                clip.open(audioIn);
                clip.start();
            } else {
                // Системный beep через Toolkit
                Toolkit.getDefaultToolkit().beep();
            }
        } catch (Exception e) {
            Toolkit.getDefaultToolkit().beep();
        }
    }

    private void stopAlarm() {
        if (playing) {
            stopEvent = true;
            playing = false;
            currentAlarm = null;
            statusLabel.setText("Будильник остановлен");
        }
    }

    private void snooze() {
        if (currentAlarm != null && playing) {
            stopEvent = true;
            playing = false;
            LocalDateTime now = LocalDateTime.now().plusMinutes(snoozeMinutes);
            currentAlarm.hour = now.getHour();
            currentAlarm.minute = now.getMinute();
            saveSettings();
            refreshList();
            statusLabel.setText("Отложено на " + snoozeMinutes + " минут");
            currentAlarm = null;
        }
    }

    private void addAlarm() {
        String timeStr = timeField.getText().trim();
        String[] parts = timeStr.split(":");
        if (parts.length != 2) {
            JOptionPane.showMessageDialog(this, "Неверный формат времени");
            return;
        }
        int hour, minute;
        try {
            hour = Integer.parseInt(parts[0]);
            minute = Integer.parseInt(parts[1]);
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59) throw new NumberFormatException();
        } catch (NumberFormatException e) {
            JOptionPane.showMessageDialog(this, "Некорректное время");
            return;
        }
        String daysStr = daysField.getText().trim().toLowerCase();
        List<String> days = Arrays.asList(daysStr.split(","));
        for (String d : days) {
            if (!Arrays.asList("пн","вт","ср","чт","пт","сб","вс").contains(d.trim())) {
                JOptionPane.showMessageDialog(this, "Некорректный день: " + d);
                return;
            }
        }
        // Выбор мелодии
        JFileChooser chooser = new JFileChooser();
        String melody = "default";
        if (chooser.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            melody = chooser.getSelectedFile().getAbsolutePath();
        }
        Alarm a = new Alarm();
        a.id = new Random().nextInt(9000) + 1000;
        a.hour = hour;
        a.minute = minute;
        a.days = days;
        a.melody = melody;
        a.volume = 80;
        a.gradualMinutes = 5;
        a.light = true;
        a.enabled = true;
        alarms.add(a);
        saveSettings();
        refreshList();
        statusLabel.setText("Добавлен будильник на " + timeStr);
    }

    private void removeAlarm() {
        int idx = alarmList.getSelectedIndex();
        if (idx < 0) return;
        alarms.remove(idx);
        saveSettings();
        refreshList();
        statusLabel.setText("Будильник удалён");
    }

    private void onSelect() {
        int idx = alarmList.getSelectedIndex();
        if (idx < 0) return;
        Alarm a = alarms.get(idx);
        String info = String.format("Время: %02d:%02d\nДни: %s\nМелодия: %s\nГромкость: %d%%\nНарастание: %d мин",
                a.hour, a.minute, String.join(",", a.days),
                a.melody.equals("default") ? "встроенная" : new File(a.melody).getName(),
                a.volume, a.gradualMinutes);
        infoArea.setText(info);
    }

    private void refreshList() {
        listModel.clear();
        for (Alarm a : alarms) {
            String days = String.join(",", a.days);
            if (days.isEmpty()) days = "все";
            String text = String.format("%02d:%02d [%s]", a.hour, a.minute, days);
            listModel.addElement(text);
        }
    }

    private void loadSettings() {
        try {
            String content = new String(Files.readAllBytes(Paths.get(SETTINGS_FILE)));
            Gson gson = new Gson();
            JsonObject obj = gson.fromJson(content, JsonObject.class);
            snoozeMinutes = obj.get("snooze_minutes").getAsInt();
            JsonArray arr = obj.get("alarms").getAsJsonArray();
            for (JsonElement el : arr) {
                JsonObject o = el.getAsJsonObject();
                Alarm a = new Alarm();
                a.id = o.get("id").getAsInt();
                a.hour = o.get("hour").getAsInt();
                a.minute = o.get("minute").getAsInt();
                a.days = Arrays.asList(o.get("days").getAsString().split(","));
                a.melody = o.get("melody").getAsString();
                a.volume = o.get("volume").getAsInt();
                a.gradualMinutes = o.get("gradual_minutes").getAsInt();
                a.light = o.get("light").getAsBoolean();
                a.enabled = o.get("enabled").getAsBoolean();
                alarms.add(a);
            }
        } catch (Exception e) {
            // файла нет — создаём пустой список
        }
    }

    private void saveSettings() {
        try {
            JsonArray arr = new JsonArray();
            for (Alarm a : alarms) {
                JsonObject o = new JsonObject();
                o.addProperty("id", a.id);
                o.addProperty("hour", a.hour);
                o.addProperty("minute", a.minute);
                o.addProperty("days", String.join(",", a.days));
                o.addProperty("melody", a.melody);
                o.addProperty("volume", a.volume);
                o.addProperty("gradual_minutes", a.gradualMinutes);
                o.addProperty("light", a.light);
                o.addProperty("enabled", a.enabled);
                arr.add(o);
            }
            JsonObject obj = new JsonObject();
            obj.add("alarms", arr);
            obj.addProperty("snooze_minutes", snoozeMinutes);
            Files.write(Paths.get(SETTINGS_FILE), obj.toString().getBytes());
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    static class Alarm {
        int id, hour, minute, volume, gradualMinutes;
        List<String> days;
        String melody;
        boolean light, enabled;
    }

    public static void main(String[] args) throws Exception {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        SwingUtilities.invokeLater(() -> new AlarmJava().setVisible(true));
    }
}
