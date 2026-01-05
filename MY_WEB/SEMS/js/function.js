import { db } from './config_firebase.js';
import { ref, update, get, query, orderByKey, limitToLast } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js"; 
import { sendMQTTCommand } from "./mqtt_con.js";
import { switchChartsRoom } from './chart.js';

/// ===============================
// SENSOR CACHE THEO PHÒNG
// ===============================
export const sensorCache = {
    kitchen: { temp: null, humi: null, co2: null },
    bedroom: { temp: null, humi: null, co2: null },
    livingroom: { temp: null, humi: null, co2: null }
};

// 1. Theo dõi phòng hiện tại 
let currentRoom = 'kitchen'; 

// Hàm xây dựng đường dẫn cơ sở động
function getActuatorsBasePath() {
    return `smarthome/${currentRoom}/actuators`; 
}

function getSensorsBasePath() {
    return `smarthome/${currentRoom}/sensors`; 
}

// Các phần tử UI 
const fanToggle = document.getElementById('fanToggle');
const fanSpeed = document.getElementById('fanSpeed');
const ledToggle = document.getElementById('ledToggle');
const ledBrightness = document.getElementById('ledBrightness');
const buzzerToggle = document.getElementById('buzzerToggle');
const buzzerVolume = document.getElementById('buzzerVolume');
const roomButtons = document.querySelectorAll('.room-btn');

// ===============================================
// HÀM GHI DỮ LIỆU LÊN FIREBASE (WRITE)
// ===============================================

function handleToggle(deviceName, toggleElement, sliderElement = null) {
    // ✅ KIỂM TRA AUTO MODE TRƯỚC KHI GỬI
    if (isAutoMode) {
        ESP_LOGW(TAG, "⚠️ Cannot control in AUTO mode");
        toggleElement.checked = !toggleElement.checked; // Revert toggle
        alert("Please switch to MANUAL mode to control devices");
        return;
    }

    const isChecked = toggleElement.checked;
    const newState = isChecked ? 'ON' : 'OFF';
    const dbPath = `${getActuatorsBasePath()}/${deviceName}`;

    const currentLevel = (newState === 'OFF') ? 0 : (sliderElement ? Number(sliderElement.value) : 0);

    const updateData = { 
        state: newState,
        level: currentLevel
    };

    // ✅ GỬI MQTT (CHỈ TRONG MANUAL MODE)
    const topic = `smarthome/${currentRoom}/actuators/${deviceName}`;
    const payload = {
        state: newState,
        level: currentLevel
    };
    sendMQTTCommand(topic, payload);
    console.log(`📤 MANUAL control: ${deviceName} → state:${newState}, level:${currentLevel}`);

    // Update Firebase & UI
    update(ref(db, dbPath), updateData)
    .then(() => {
        console.log(`✅ Firebase updated: ${deviceName} in ${currentRoom}`);
        if (sliderElement) {
            sliderElement.disabled = (newState === 'OFF');
            if (newState === 'OFF') {
                sliderElement.value = 0;
            }
        }
    })
    .catch((error) => {
        console.error(`❌ Lỗi Firebase ${deviceName}:`, error);
        toggleElement.checked = !isChecked; 
        alert(`Lỗi khi cập nhật ${deviceName}`);
    });
}

function handleRangeChange(deviceName, rangeElement, toggleElement = null) {
    // ✅ KIỂM TRA AUTO MODE
    if (isAutoMode) {
        console.warn("⚠️ Cannot control in AUTO mode");
        // Revert slider value from Firebase
        syncControlsFromFirebase();
        return;
    }

    const newLevel = parseInt(rangeElement.value); 
    const dbPath = `${getActuatorsBasePath()}/${deviceName}`;

    let newState = 'OFF';
    if (newLevel > 0) {
        newState = 'ON';
        if (toggleElement && !toggleElement.checked) {
            toggleElement.checked = true;
        }
    } else {
        newState = 'OFF';
        if (toggleElement) {
            toggleElement.checked = false;
        }
    }

    const updateData = { 
        state: newState,
        level: newLevel
    };

    // ✅ GỬI MQTT (MANUAL MODE ONLY)
    const topic = `smarthome/${currentRoom}/actuators/${deviceName}`;
    const payload = {
        state: newState,
        level: newLevel
    };
    sendMQTTCommand(topic, payload);
    console.log(`📤 MANUAL control: ${deviceName} → state:${newState}, level:${newLevel}`);

    update(ref(db, dbPath), updateData)
    .then(() => {
        console.log(`✅ Firebase updated: ${deviceName} Level=${newLevel}`);
        if (toggleElement) {
            toggleElement.disabled = false;
        }
    })
    .catch((error) => {
        console.error(`❌ Lỗi Firebase ${deviceName}:`, error);
        alert(`Lỗi khi cập nhật ${deviceName} level.`);
    });
}


function renderSensorsFromCache(room) {
    const data = sensorCache[room];
    if (!data) return;

    document.getElementById('tempValue').innerText =
        data.temp !== null ? data.temp.toFixed(1) : '--';

    document.getElementById('humiValue').innerText =
        data.humi !== null ? data.humi.toFixed(1) : '--';

    document.getElementById('co2Value').innerText =
        data.co2 !== null ? Math.round(data.co2) : '--';
}
// ===============================================
// HÀM CẬP NHẬT SENSOR TỪ MQTT (ĐƯỢC GỌI TỪ mqtt_con.js)
// ===============================================
export function updateSensorFromMQTT(topic, data) {
    // topic: smarthome/bedroom/sensors/temp
    const parts = topic.split("/");
    const room = parts[1];
    const sensorName = parts[3]; // "temp", "humi", "co2"
    if (!['temp', 'humi', 'co2'].includes(sensorName)) {
    console.warn(`⚠️ Unknown sensor: ${sensorName}`);
    return;
    }
    // ✅ LƯU VÀO BIẾN GLOBAL


            // đảm bảo room tồn tại
        if (!sensorCache[room]) {
            sensorCache[room] = { temp: null, humi: null, co2: null };
        }

        // lưu theo đúng phòng
        sensorCache[room][sensorName] = parseFloat(data.value);

    console.log(`📦 Cache updated [${room}] ${sensorName} = ${data.value}`);
 

    // 1️⃣ GHI VÀO FIREBASE THEO CẤU TRÚC TIME-SERIES
    // Đường dẫn: smarthome/bedroom/sensors/temp/{timestamp}
    const timestamp = data.ts || Date.now();
    const dbPath = `smarthome/${room}/sensors/${sensorName}/${timestamp}`;
    
    update(ref(db, dbPath), {
        value: data.value
    })
    .then(() => {
        console.log(`✅ Firebase saved: ${sensorName} at ${timestamp}`);
    })
    .catch((error) => {
        console.error(`❌ Firebase error:`, error);
    });

    // 2️⃣ Update UI realtime (CHỈ KHI ĐÚNG PHÒNG ĐANG XEM)
    if (room === currentRoom) {
    const el = document.getElementById(
        sensorName === "temp" ? "tempValue" :
        sensorName === "humi" ? "humiValue" :
        "co2Value"
    );

    if (el) {
        el.innerText =
            sensorName === "co2"
            ? Math.round(data.value)
            : parseFloat(data.value).toFixed(1);
    }
}
}

// ===============================================
// HÀM ĐỒNG BỘ UI TỪ FIREBASE (READ)
// ===============================================

async function syncControlsFromFirebase() {
    const actuatorsPath = getActuatorsBasePath();
    try {
        const snapshot = await get(ref(db, actuatorsPath));
        const actuatorsData = snapshot.val();

        if (!actuatorsData) {
            console.log(`Không có dữ liệu actuators cho phòng ${currentRoom}`);
            return;
        }

        const devices = [
            { name: 'fan', toggle: fanToggle, slider: fanSpeed },
            { name: 'led', toggle: ledToggle, slider: ledBrightness },
            { name: 'buzzer', toggle: buzzerToggle, slider: buzzerVolume }
        ];
        
        devices.forEach(device => {
            const data = actuatorsData[device.name];
            if (data && device.toggle && device.slider) {
                // Đồng bộ Toggle (State)
                const isChecked = data.state === 'ON';
                device.toggle.checked = isChecked;

                // Đồng bộ Slider (Level)
                device.slider.value = data.level || 0;
                
                // Vô hiệu hóa slider nếu thiết bị tắt
                device.slider.disabled = !isChecked;
            }
        });

        console.log(`✅ Đồng bộ trạng thái UI thành công cho phòng: ${currentRoom}`);

    } catch (error) {
        console.error(`Lỗi đồng bộ UI cho phòng ${currentRoom}:`, error);
    }
}

async function syncSensorsFromFirebase() {
    const sensorsPath = getSensorsBasePath();

    try {
        const sensors = [
            { name: 'temp', elementId: 'tempValue' },
            { name: 'humi', elementId: 'humiValue' },
            { name: 'co2', elementId: 'co2Value' }
        ];

        // ✅ ĐỌC GIÁ TRỊ MỚI NHẤT TỪ TIME-SERIES
        for (const sensor of sensors) {
            const sensorPath = `${sensorsPath}/${sensor.name}`;
            
            // Query: lấy 1 record mới nhất (orderByKey + limitToLast)
            const snapshot = await get(
                query(
                    ref(db, sensorPath),
                    orderByKey(),
                    limitToLast(1)
                )
            );

            if (snapshot.exists()) {
                const data = snapshot.val();
                // data = { "1767513090000": { value: 27.60 } }
                
                const latestTimestamp = Object.keys(data)[0];
                const latestValue = data[latestTimestamp].value;

                // đảm bảo cache tồn tại
            if (!sensorCache[currentRoom]) {
                sensorCache[currentRoom] = { temp: null, humi: null, co2: null };
            }

            sensorCache[currentRoom][sensor.name] = parseFloat(latestValue);

                // Cập nhật UI
                const el = document.getElementById(sensor.elementId);
                if (el) {
                    if (sensor.name === "temp" || sensor.name === "humi") {
                        el.innerText = parseFloat(latestValue).toFixed(1);
                    } else {
                        el.innerText = Math.round(latestValue);
                    }
                }
                
                console.log(`✅ Sensor ${sensor.name} synced: ${latestValue} (ts: ${latestTimestamp})`);
            } else {
                console.log(`ℹ️ Không có dữ liệu cho ${sensor.name}`);
            }
        }

    } catch (error) {
        console.error(`❌ Lỗi đọc sensors cho phòng ${currentRoom}:`, error);
    }
}

// ===============================================
// GÁN SỰ KIỆN CHO CÁC PHẦN TỬ UI
// ===============================================

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

// ===============================================
// XỬ LÝ SỰ KIỆN CHUYỂN ROOM
// ===============================================

roomButtons.forEach(button => {
    button.addEventListener('click', async (event) => { // ✅ Thêm async
        const newRoom = event.target.getAttribute('data-room');
        if (newRoom && newRoom !== currentRoom) {
            currentRoom = newRoom;
            renderSensorsFromCache(currentRoom);
            // Cập nhật giao diện (CSS active class)
            roomButtons.forEach(btn => btn.classList.remove('active'));
            event.target.classList.add('active');

            // ✅ Đồng bộ chart cho phòng mới
            await switchChartsRoom(newRoom);

            // Đồng bộ lại trạng thái điều khiển cho phòng mới từ Firebase
            syncControlsFromFirebase(); 
            syncSensorsFromFirebase();
        }
    });
});

// Chạy đồng bộ lần đầu tiên khi tải trang
syncControlsFromFirebase();
syncSensorsFromFirebase();

// ===============================================
// XỬ LÝ AUTO MODE
// ===============================================

const autoModeToggle = document.getElementById('autoModeToggle');
let isAutoMode = true; // Mặc định AUTO mode BẬT

// Load trạng thái từ localStorage khi tải trang
document.addEventListener('DOMContentLoaded', () => {
    const savedMode = localStorage.getItem('autoMode');
    if (savedMode !== null) {
        isAutoMode = (savedMode === 'true');
        autoModeToggle.checked = isAutoMode;
    }
    updateAutoModeUI(isAutoMode);
});

// Lắng nghe sự kiện thay đổi toggle
if (autoModeToggle) {
    autoModeToggle.addEventListener('change', (e) => {
        isAutoMode = e.target.checked;
        localStorage.setItem('autoMode', isAutoMode);
        updateAutoModeUI(isAutoMode);
        
        if (isAutoMode) {
            enableAutoMode();
        } else {
            disableAutoMode();
        }
    });
}

function updateAutoModeUI(isActive) {
    const modeLabel = document.querySelector('.mode-label');
    if (modeLabel) {
        if (isActive) {
            modeLabel.textContent = 'AUTO';
            modeLabel.style.color = '#ffffff';
        } else {
            modeLabel.textContent = 'MANUAL';
            modeLabel.style.color = '#ffcccc';
        }
    }
}

function enableAutoMode() {
    console.log('✅ AUTO MODE: Enabled - Devices controlled by sensors');
    
    // ✅ GỬI MQTT LÊN BROKER
    const topic = 'smarthome/auto';
    const payload = {
        state: 'ON',
    };
    sendMQTTCommand(topic, payload);
    console.log(`📤 MQTT sent: ${topic} → state: ON`);
    
    // Vô hiệu hóa các controls thủ công
    const controls = [fanToggle, fanSpeed, ledToggle, ledBrightness, buzzerToggle, buzzerVolume];
    controls.forEach(control => {
        if (control) {
            control.disabled = true;
            // Thêm visual feedback
            if (control.parentElement) {
                control.parentElement.style.opacity = '0.5';
            }
        }
    });
}

function disableAutoMode() {
    console.log('⚠️ MANUAL MODE: User can control devices manually');
    
    // ✅ GỬI MQTT LÊN BROKER
    const topic = 'smarthome/auto';
    const payload = {
        state: 'OFF',
    };
    sendMQTTCommand(topic, payload);
    console.log(`📤 MQTT sent: ${topic} → state: OFF`);
    
    // Kích hoạt lại các controls thủ công
    const controls = [fanToggle, fanSpeed, ledToggle, ledBrightness, buzzerToggle, buzzerVolume];
    controls.forEach(control => {
        if (control) {
            control.disabled = false;
            // Xóa visual feedback
            if (control.parentElement) {
                control.parentElement.style.opacity = '1';
            }
        }
    });
}

// Export để có thể sử dụng ở file khác nếu cần
export { isAutoMode, enableAutoMode, disableAutoMode };