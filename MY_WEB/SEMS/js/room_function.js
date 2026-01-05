/**
 * room_function.js - Controls cho từng phòng riêng biệt
 * Đơn giản hơn function.js gốc - không cần switch room
 */

import { db } from './config_firebase.js';
import { ref, update, get, query, orderByKey, limitToLast } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js";
import { sendMQTTCommand, getCurrentRoom, setOnActuatorUpdate } from "./mqtt_room.js";

// ===============================================
// BIẾN MODULE
// ===============================================
let currentRoom = 'livingroom';
let isAutoMode = true;

// ✅ Phòng có ESP32 thật (các phòng khác giả lập)
const REAL_ESP_ROOM = 'livingroom';

// UI Elements
let fanToggle, fanSpeed, ledToggle, ledBrightness, buzzerToggle, buzzerVolume, autoModeToggle;

// Debounce timers
let debounceTimers = {};

// ===============================================
// DEBOUNCE HELPER
// ===============================================
function debounce(key, callback, delay = 300) {
    if (debounceTimers[key]) {
        clearTimeout(debounceTimers[key]);
    }
    debounceTimers[key] = setTimeout(callback, delay);
}

// ===============================================
// KIỂM TRA PHÒNG CÓ ESP32 THẬT
// ===============================================
function isRealESPRoom() {
    return currentRoom === REAL_ESP_ROOM;
}

// ===============================================
// KHỞI TẠO CONTROLS
// ===============================================
export function initRoomControls(roomId) {
    currentRoom = roomId;
    
    // Lấy UI elements
    fanToggle = document.getElementById('fanToggle');
    fanSpeed = document.getElementById('fanSpeed');
    ledToggle = document.getElementById('ledToggle');
    ledBrightness = document.getElementById('ledBrightness');
    buzzerToggle = document.getElementById('buzzerToggle');
    buzzerVolume = document.getElementById('buzzerVolume');
    autoModeToggle = document.getElementById('autoModeToggle');
    
    // Setup event listeners
    setupDeviceListeners();
    setupAutoModeListener();
    
    // Sync từ Firebase
    syncControlsFromFirebase();
    syncSensorsFromFirebase();
    
    // Listen actuator updates từ MQTT
    setOnActuatorUpdate(handleActuatorUpdate);
    
    console.log(`✅ Controls initialized for room: ${roomId}`);
}

// ===============================================
// DEVICE CONTROLS
// ===============================================
function setupDeviceListeners() {
    if (fanToggle && fanSpeed) {
        fanToggle.addEventListener('change', () => handleToggle('fan', fanToggle, fanSpeed));
        fanSpeed.addEventListener('input', () => handleRangeChange('fan', fanSpeed, fanToggle));
    }
    
    if (ledToggle && ledBrightness) {
        ledToggle.addEventListener('change', () => handleToggle('led', ledToggle, ledBrightness));
        ledBrightness.addEventListener('input', () => handleRangeChange('led', ledBrightness, ledToggle));
    }
    
    if (buzzerToggle && buzzerVolume) {
        buzzerToggle.addEventListener('change', () => handleToggle('buzzer', buzzerToggle, buzzerVolume));
        buzzerVolume.addEventListener('input', () => handleRangeChange('buzzer', buzzerVolume, buzzerToggle));
    }
}

function handleToggle(deviceName, toggleElement, sliderElement = null) {
    if (isAutoMode) {
        toggleElement.checked = !toggleElement.checked;
        alert("Please switch to MANUAL mode to control devices");
        return;
    }

    const isChecked = toggleElement.checked;
    const newState = isChecked ? 'ON' : 'OFF';
    const currentLevel = (newState === 'OFF') ? 0 : (sliderElement ? Number(sliderElement.value) : 0);

    // ✅ Chỉ gửi MQTT nếu phòng có ESP32 thật
    if (isRealESPRoom()) {
        const topic = `smarthome/${currentRoom}/actuators/${deviceName}`;
        sendMQTTCommand(topic, { state: newState, level: currentLevel });
    } else {
        console.log(`🎭 Simulated ${deviceName}: ${newState}, level=${currentLevel}`);
    }

    // Update Firebase (tất cả phòng)
    const dbPath = `smarthome/${currentRoom}/actuators/${deviceName}`;
    update(ref(db, dbPath), { state: newState, level: currentLevel })
        .then(() => {
            if (sliderElement) {
                sliderElement.disabled = (newState === 'OFF');
                if (newState === 'OFF') sliderElement.value = 0;
            }
        })
        .catch(console.error);
}

function handleRangeChange(deviceName, rangeElement, toggleElement = null) {
    if (isAutoMode) {
        syncControlsFromFirebase();
        return;
    }

    const newLevel = parseInt(rangeElement.value);
    const newState = newLevel > 0 ? 'ON' : 'OFF';
    
    // Update UI ngay lập tức
    if (toggleElement) {
        toggleElement.checked = (newLevel > 0);
    }

    // Debounce gửi MQTT và Firebase (chỉ gửi sau 300ms không thay đổi)
    debounce(`range_${deviceName}`, () => {
        // ✅ Chỉ gửi MQTT nếu phòng có ESP32 thật
        if (isRealESPRoom()) {
            const topic = `smarthome/${currentRoom}/actuators/${deviceName}`;
            sendMQTTCommand(topic, { state: newState, level: newLevel });
        } else {
            console.log(`🎭 Simulated ${deviceName}: ${newState}, level=${newLevel}`);
        }

        // Update Firebase (tất cả phòng)
        const dbPath = `smarthome/${currentRoom}/actuators/${deviceName}`;
        update(ref(db, dbPath), { state: newState, level: newLevel }).catch(console.error);
        
        console.log(`📤 ${deviceName}: ${newState}, level=${newLevel}`);
    }, 300);
}

// ===============================================
// AUTO MODE
// ===============================================
function setupAutoModeListener() {
    // Load từ localStorage
    const savedMode = localStorage.getItem('autoMode');
    if (savedMode !== null) {
        isAutoMode = (savedMode === 'true');
        if (autoModeToggle) autoModeToggle.checked = isAutoMode;
    }
    updateAutoModeUI(isAutoMode);

    if (autoModeToggle) {
        autoModeToggle.addEventListener('change', (e) => {
            isAutoMode = e.target.checked;
            localStorage.setItem('autoMode', isAutoMode);
            updateAutoModeUI(isAutoMode);
            
            // Gửi MQTT
            sendMQTTCommand('smarthome/auto', { state: isAutoMode ? 'ON' : 'OFF' });
        });
    }
}

function updateAutoModeUI(isActive) {
    const modeLabel = document.querySelector('.mode-label');
    const modeIcon = document.querySelector('.mode-icon i');
    
    if (modeLabel) {
        modeLabel.textContent = isActive ? 'AUTO' : 'MANUAL';
    }
    
    if (modeIcon) {
        if (isActive) {
            modeIcon.className = 'fas fa-robot';
            modeIcon.parentElement.style.color = '#00ff88';
        } else {
            modeIcon.className = 'fas fa-hand-pointer';
            modeIcon.parentElement.style.color = '#ff6b6b';
        }
    }

    // Enable/disable controls
    const controls = [fanToggle, fanSpeed, ledToggle, ledBrightness, buzzerToggle, buzzerVolume];
    controls.forEach(control => {
        if (control) {
            control.disabled = isActive;
            if (control.parentElement) {
                control.parentElement.style.opacity = isActive ? '0.5' : '1';
            }
        }
    });
}

// ===============================================
// SYNC TỪ FIREBASE
// ===============================================
async function syncControlsFromFirebase() {
    const actuatorsPath = `smarthome/${currentRoom}/actuators`;
    try {
        const snapshot = await get(ref(db, actuatorsPath));
        const data = snapshot.val();

        if (!data) return;

        const devices = [
            { name: 'fan', toggle: fanToggle, slider: fanSpeed },
            { name: 'led', toggle: ledToggle, slider: ledBrightness },
            { name: 'buzzer', toggle: buzzerToggle, slider: buzzerVolume }
        ];
        
        devices.forEach(device => {
            const deviceData = data[device.name];
            if (deviceData && device.toggle && device.slider) {
                const isChecked = deviceData.state === 'ON';
                device.toggle.checked = isChecked;
                device.slider.value = deviceData.level || 0;
                device.slider.disabled = !isChecked || isAutoMode;
            }
        });
    } catch (error) {
        console.error("❌ Sync controls error:", error);
    }
}

async function syncSensorsFromFirebase() {
    const sensorsPath = `smarthome/${currentRoom}/sensors`;
    const sensors = [
        { name: 'temp', elementId: 'tempValue' },
        { name: 'humi', elementId: 'humiValue' },
        { name: 'co2', elementId: 'co2Value' }
    ];

    try {
        for (const sensor of sensors) {
            const sensorPath = `${sensorsPath}/${sensor.name}`;
            const snapshot = await get(query(ref(db, sensorPath), orderByKey(), limitToLast(1)));

            if (snapshot.exists()) {
                const data = snapshot.val();
                const latestKey = Object.keys(data)[0];
                const value = data[latestKey].value;

                const el = document.getElementById(sensor.elementId);
                if (el) {
                    if (sensor.name === "temp" || sensor.name === "humi") {
                        el.innerText = parseFloat(value).toFixed(1);
                    } else {
                        el.innerText = Math.round(value);
                    }
                }
            }
        }
    } catch (error) {
        console.error("❌ Sync sensors error:", error);
    }
}

// ===============================================
// HANDLE ACTUATOR UPDATE TỪ MQTT
// ===============================================
function handleActuatorUpdate(deviceName, data) {
    // Update UI khi nhận reported từ ESP32
    const devices = {
        'fan': { toggle: fanToggle, slider: fanSpeed },
        'led': { toggle: ledToggle, slider: ledBrightness },
        'buzzer': { toggle: buzzerToggle, slider: buzzerVolume }
    };

    const device = devices[deviceName];
    if (device && device.toggle && device.slider) {
        device.toggle.checked = (data.state === 'ON');
        device.slider.value = data.level || 0;
        device.slider.disabled = (data.state !== 'ON') || isAutoMode;
    }
}
