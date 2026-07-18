// alarm_cpp.cpp — умный будильник на C++ (Qt Widgets)

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>
#include <QDateTime>
#include <QSound>
#include <QThread>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <random>

class AlarmClock : public QMainWindow {
    Q_OBJECT
public:
    AlarmClock(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("🌅 WakeSmart — C++");
        resize(700, 550);
        loadSettings();
        createUI();
        startChecker();
        updateClock();
    }

private slots:
    void addAlarm() {
        QString timeStr = timeEdit->text().trimmed();
        QStringList parts = timeStr.split(':');
        if (parts.size() != 2) {
            QMessageBox::warning(this, "Ошибка", "Введите время в формате ЧЧ:ММ");
            return;
        }
        int hour = parts[0].toInt();
        int minute = parts[1].toInt();
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
            QMessageBox::warning(this, "Ошибка", "Некорректное время");
            return;
        }
        QString daysStr = daysEdit->text().trimmed().toLower();
        QStringList days = daysStr.split(',', Qt::SkipEmptyParts);
        // Проверка дней
        static const QStringList validDays = {"пн","вт","ср","чт","пт","сб","вс"};
        for (const QString &d : days) {
            if (!validDays.contains(d)) {
                QMessageBox::warning(this, "Ошибка", "Некорректный день: " + d);
                return;
            }
        }
        QString melody = QFileDialog::getOpenFileName(this, "Выберите мелодию", "", "Audio (*.mp3 *.wav *.ogg)");
        if (melody.isEmpty()) melody = "default";
        int volume = 80;
        int gradual = 5;
        bool light = true;
        Alarm a;
        a.id = rand() % 9000 + 1000;
        a.hour = hour;
        a.minute = minute;
        a.days = days;
        a.melody = melody;
        a.volume = volume;
        a.gradualMinutes = gradual;
        a.light = light;
        a.enabled = true;
        alarms.append(a);
        saveSettings();
        refreshList();
        statusLabel->setText("Добавлен будильник на " + timeStr);
    }

    void removeAlarm() {
        int row = listWidget->currentRow();
        if (row < 0) return;
        alarms.removeAt(row);
        saveSettings();
        refreshList();
        statusLabel->setText("Будильник удалён");
    }

    void stopAlarm() {
        if (playing) {
            stopEvent = true;
            playing = false;
            currentAlarm = nullptr;
            statusLabel->setText("Будильник остановлен");
        }
    }

    void snooze() {
        if (currentAlarm && playing) {
            stopEvent = true;
            playing = false;
            QDateTime now = QDateTime::currentDateTime();
            now = now.addSecs(snoozeMinutes * 60);
            currentAlarm->hour = now.time().hour();
            currentAlarm->minute = now.time().minute();
            saveSettings();
            refreshList();
            statusLabel->setText(QString("Отложено на %1 минут").arg(snoozeMinutes));
            currentAlarm = nullptr;
        }
    }

    void onSelect() {
        int row = listWidget->currentRow();
        if (row < 0) return;
        Alarm &a = alarms[row];
        QString info = QString("Время: %1:%2\nДни: %3\nМелодия: %4\nГромкость: %5%\nНарастание: %6 мин")
                       .arg(a.hour, 2, 10, QChar('0'))
                       .arg(a.minute, 2, 10, QChar('0'))
                       .arg(a.days.join(','))
                       .arg(a.melody == "default" ? "встроенная" : QFileInfo(a.melody).fileName())
                       .arg(a.volume)
                       .arg(a.gradualMinutes);
        infoText->setPlainText(info);
    }

private:
    struct Alarm {
        int id;
        int hour;
        int minute;
        QStringList days;
        QString melody;
        int volume;
        int gradualMinutes;
        bool light;
        bool enabled;
    };

    QList<Alarm> alarms;
    QListWidget *listWidget;
    QLineEdit *timeEdit;
    QLineEdit *daysEdit;
    QTextEdit *infoText;
    QLabel *timeLabel;
    QLabel *statusLabel;
    int snoozeMinutes = 5;
    bool playing = false;
    bool stopEvent = false;
    Alarm *currentAlarm = nullptr;
    QTimer *clockTimer;
    QTimer *checkTimer;
    QString settingsFile = "alarms.json";

    void createUI() {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        // Часы
        timeLabel = new QLabel(this);
        timeLabel->setStyleSheet("font-size: 40px; font-weight: bold; color: blue;");
        mainLayout->addWidget(timeLabel, 0, Qt::AlignCenter);

        // Панель добавления
        QHBoxLayout *addLayout = new QHBoxLayout();
        addLayout->addWidget(new QLabel("Время:"));
        timeEdit = new QLineEdit("07:30");
        addLayout->addWidget(timeEdit);
        addLayout->addWidget(new QLabel("Дни:"));
        daysEdit = new QLineEdit("пн,вт,ср,чт,пт");
        addLayout->addWidget(daysEdit);
        mainLayout->addLayout(addLayout);

        // Кнопки
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton("Добавить");
        QPushButton *delBtn = new QPushButton("Удалить");
        QPushButton *stopBtn = new QPushButton("Остановить");
        QPushButton *snoozeBtn = new QPushButton("Отложить");
        btnLayout->addWidget(addBtn);
        btnLayout->addWidget(delBtn);
        btnLayout->addWidget(stopBtn);
        btnLayout->addWidget(snoozeBtn);
        mainLayout->addLayout(btnLayout);
        connect(addBtn, &QPushButton::clicked, this, &AlarmClock::addAlarm);
        connect(delBtn, &QPushButton::clicked, this, &AlarmClock::removeAlarm);
        connect(stopBtn, &QPushButton::clicked, this, &AlarmClock::stopAlarm);
        connect(snoozeBtn, &QPushButton::clicked, this, &AlarmClock::snooze);

        // Список
        listWidget = new QListWidget(this);
        mainLayout->addWidget(listWidget);
        connect(listWidget, &QListWidget::currentRowChanged, this, &AlarmClock::onSelect);

        // Информация
        infoText = new QTextEdit(this);
        infoText->setReadOnly(true);
        infoText->setMaximumHeight(80);
        mainLayout->addWidget(infoText);

        // Статус
        statusLabel = new QLabel("Готов", this);
        mainLayout->addWidget(statusLabel);

        refreshList();
    }

    void refreshList() {
        listWidget->clear();
        for (const Alarm &a : alarms) {
            QString days = a.days.join(',');
            if (days.isEmpty()) days = "все";
            QString text = QString("%1:%2 [%3]").arg(a.hour, 2, 10, QChar('0')).arg(a.minute, 2, 10, QChar('0')).arg(days);
            listWidget->addItem(text);
        }
    }

    void updateClock() {
        QDateTime now = QDateTime::currentDateTime();
        timeLabel->setText(now.toString("HH:mm:ss"));
        clockTimer = new QTimer(this);
        connect(clockTimer, &QTimer::timeout, this, [this]() {
            QDateTime now = QDateTime::currentDateTime();
            timeLabel->setText(now.toString("HH:mm:ss"));
        });
        clockTimer->start(1000);
    }

    void startChecker() {
        checkTimer = new QTimer(this);
        connect(checkTimer, &QTimer::timeout, this, &AlarmClock::checkAlarms);
        checkTimer->start(1000);
    }

    void checkAlarms() {
        QDateTime now = QDateTime::currentDateTime();
        if (playing) return; // уже звенит
        for (Alarm &a : alarms) {
            if (!a.enabled) continue;
            if (now.time().hour() == a.hour && now.time().minute() == a.minute && now.time().second() == 0) {
                // Проверка дня
                if (!a.days.isEmpty()) {
                    static const QStringList weekdays = {"пн","вт","ср","чт","пт","сб","вс"};
                    QString today = weekdays[now.date().dayOfWeek() - 1];
                    if (!a.days.contains(today)) continue;
                }
                // Срабатывание
                triggerAlarm(&a);
                break;
            }
        }
    }

    void triggerAlarm(Alarm *alarm) {
        currentAlarm = alarm;
        playing = true;
        stopEvent = false;
        statusLabel->setText(QString("🔔 Будильник сработал! %1:%2").arg(alarm->hour, 2, 10, QChar('0')).arg(alarm->minute, 2, 10, QChar('0')));
        // Постепенное увеличение громкости (симуляция)
        QThread *thread = QThread::create([this, alarm]() {
            int volume = 0;
            int step = alarm->volume / (alarm->gradualMinutes * 60);
            while (volume < alarm->volume && !stopEvent) {
                // Имитация громкости через сообщение
                emit updateStatus(QString("🔊 Громкость: %1%").arg(volume));
                // Воспроизведение звука
                if (alarm->melody != "default" && QFile::exists(alarm->melody)) {
                    QSound::play(alarm->melody);
                } else {
                    // Встроенный звук - системный beep (через QApplication::beep())
                    QApplication::beep();
                }
                volume += step * 1;
                QThread::sleep(1);
            }
            // Продолжаем звонить до остановки
            while (!stopEvent) {
                if (alarm->melody != "default" && QFile::exists(alarm->melody)) {
                    QSound::play(alarm->melody);
                } else {
                    QApplication::beep();
                }
                QThread::sleep(2);
            }
        });
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        connect(this, &AlarmClock::updateStatus, statusLabel, &QLabel::setText);
        thread->start();
    }

signals:
    void updateStatus(const QString &msg);

    void loadSettings() {
        QFile file(settingsFile);
        if (!file.open(QIODevice::ReadOnly)) return;
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return;
        QJsonObject obj = doc.object();
        snoozeMinutes = obj.value("snooze_minutes").toInt(5);
        QJsonArray arr = obj.value("alarms").toArray();
        alarms.clear();
        for (const QJsonValue &v : arr) {
            QJsonObject o = v.toObject();
            Alarm a;
            a.id = o.value("id").toInt();
            a.hour = o.value("hour").toInt();
            a.minute = o.value("minute").toInt();
            a.days = o.value("days").toString().split(',', Qt::SkipEmptyParts);
            a.melody = o.value("melody").toString("default");
            a.volume = o.value("volume").toInt(80);
            a.gradualMinutes = o.value("gradual_minutes").toInt(5);
            a.light = o.value("light").toBool(true);
            a.enabled = o.value("enabled").toBool(true);
            alarms.append(a);
        }
    }

    void saveSettings() {
        QJsonArray arr;
        for (const Alarm &a : alarms) {
            QJsonObject o;
            o["id"] = a.id;
            o["hour"] = a.hour;
            o["minute"] = a.minute;
            o["days"] = a.days.join(',');
            o["melody"] = a.melody;
            o["volume"] = a.volume;
            o["gradual_minutes"] = a.gradualMinutes;
            o["light"] = a.light;
            o["enabled"] = a.enabled;
            arr.append(o);
        }
        QJsonObject obj;
        obj["alarms"] = arr;
        obj["snooze_minutes"] = snoozeMinutes;
        QFile file(settingsFile);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    AlarmClock clock;
    clock.show();
    return app.exec();
}

#include "alarm_cpp.moc"
