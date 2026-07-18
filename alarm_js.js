// alarm_js.js — умный будильник на JavaScript (Node.js + Electron)

const { app, BrowserWindow, Menu, dialog, ipcMain, clipboard } = require('electron');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');
const settings = require('electron-settings');

let mainWindow;
let alarms = [];
let snoozeMinutes = 5;
let playing = false;
let stopFlag = false;
let currentAlarm = null;
let checkInterval = null;

class Alarm {
    constructor(id, hour, minute, days, melody, volume, gradualMinutes, light, enabled) {
        this.id = id;
        this.hour = hour;
        this.minute = minute;
        this.days = days;
        this.melody = melody;
        this.volume = volume;
        this.gradualMinutes = gradualMinutes;
        this.light = light;
        this.enabled = enabled;
    }
}

function loadSettings() {
    try {
        const data = settings.getSync('alarms') || [];
        alarms = data.map(a => new Alarm(
            a.id, a.hour, a.minute, a.days, a.melody,
            a.volume, a.gradualMinutes, a.light, a.enabled
        ));
        snoozeMinutes = settings.getSync('snoozeMinutes') || 5;
    } catch (e) {
        alarms = [];
    }
}

function saveSettings() {
    settings.setSync('alarms', alarms);
    settings.setSync('snoozeMinutes', snoozeMinutes);
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 700,
        height: 550,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });
    mainWindow.loadFile('index.html'); // предполагается, что есть HTML с интерфейсом
    // Меню
    const menu = Menu.buildFromTemplate([
        {
            label: 'Файл',
            submenu: [
                { role: 'quit' }
            ]
        }
    ]);
    Menu.setApplicationMenu(menu);

    // IPC handlers
    ipcMain.handle('getAlarms', () => {
        return alarms;
    });

    ipcMain.handle('addAlarm', (event, hour, minute, days, melody) => {
        const id = Date.now();
        const alarm = new Alarm(id, hour, minute, days, melody, 80, 5, true, true);
        alarms.push(alarm);
        saveSettings();
        return alarms;
    });

    ipcMain.handle('removeAlarm', (event, id) => {
        alarms = alarms.filter(a => a.id !== id);
        saveSettings();
        return alarms;
    });

    ipcMain.handle('stopAlarm', () => {
        stopAlarm();
        return 'stopped';
    });

    ipcMain.handle('snooze', () => {
        snoozeAlarm();
        return 'snoozed';
    });

    ipcMain.handle('getSnoozeMinutes', () => snoozeMinutes);
    ipcMain.handle('setSnoozeMinutes', (event, val) => {
        snoozeMinutes = val;
        saveSettings();
    });

    // Запуск проверки
    startChecker();
}

function startChecker() {
    checkInterval = setInterval(() => {
        if (playing) return;
        const now = new Date();
        const hour = now.getHours();
        const minute = now.getMinutes();
        const second = now.getSeconds();
        if (second !== 0) return;
        for (let a of alarms) {
            if (!a.enabled) continue;
            if (a.hour === hour && a.minute === minute) {
                // Проверка дня
                if (a.days && a.days.length > 0) {
                    const weekdays = ['пн','вт','ср','чт','пт','сб','вс'];
                    const today = weekdays[now.getDay() === 0 ? 6 : now.getDay() - 1];
                    if (!a.days.includes(today)) continue;
                }
                triggerAlarm(a);
                break;
            }
        }
    }, 1000);
}

function triggerAlarm(alarm) {
    currentAlarm = alarm;
    playing = true;
    stopFlag = false;
    mainWindow.webContents.send('alarm-triggered', alarm);
    // Постепенное увеличение громкости (имитация через сообщения)
    let volume = 0;
    const step = alarm.volume / (alarm.gradualMinutes * 60);
    const playSound = () => {
        if (alarm.melody && alarm.melody !== 'default' && fs.existsSync(alarm.melody)) {
            // Используем системный плеер (для Windows - start, macOS - afplay, Linux - aplay)
            const cmd = process.platform === 'win32' ? 'start' : (process.platform === 'darwin' ? 'afplay' : 'aplay');
            exec(`${cmd} "${alarm.melody}"`);
        } else {
            // beep через консоль
            process.stdout.write('\x07');
        }
    };
    const interval = setInterval(() => {
        if (stopFlag) {
            clearInterval(interval);
            playing = false;
            mainWindow.webContents.send('alarm-stopped');
            return;
        }
        if (volume < alarm.volume) {
            volume += step * 1;
            mainWindow.webContents.send('volume-update', Math.floor(volume));
        }
        playSound();
    }, 1000);
    // Дополнительный звонок после достижения громкости
    setTimeout(() => {
        if (!stopFlag && playing) {
            const secondInterval = setInterval(() => {
                if (stopFlag) {
                    clearInterval(secondInterval);
                    playing = false;
                    mainWindow.webContents.send('alarm-stopped');
                    return;
                }
                playSound();
            }, 2000);
        }
    }, alarm.gradualMinutes * 60 * 1000);
}

function stopAlarm() {
    if (playing) {
        stopFlag = true;
        playing = false;
        currentAlarm = null;
    }
}

function snoozeAlarm() {
    if (currentAlarm && playing) {
        stopFlag = true;
        playing = false;
        const now = new Date();
        now.setMinutes(now.getMinutes() + snoozeMinutes);
        const alarm = currentAlarm;
        alarm.hour = now.getHours();
        alarm.minute = now.getMinutes();
        // обновляем в списке
        const idx = alarms.findIndex(a => a.id === alarm.id);
        if (idx !== -1) alarms[idx] = alarm;
        saveSettings();
        mainWindow.webContents.send('alarm-snoozed', snoozeMinutes);
        currentAlarm = null;
    }
}

app.whenReady().then(() => {
    loadSettings();
    createWindow();
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});
