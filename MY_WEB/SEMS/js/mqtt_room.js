/**
 * mqtt_room.js - MQTT module cho từng phòng riêng biệt
 * KHÔNG THAY ĐỔI LOGIC MQTT GỐC - Chỉ thêm room filter
 */

import { db } from './config_firebase.js';
import { ref, update } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js";

// ===============================================
// CONFIG MQTT BROKER (GIỐNG HỆT mqtt_con.js)
// ===============================================
const MQTT_HOST = "19059388a61f4c8286066fda62e74315.s1.eu.hivemq.cloud";
const MQTT_PORT = 8884;
const MQTT_CLIENT_ID = "WebApp_" + Math.random().toString(16).substr(2, 8);
const MQTT_USER = "trinity";
const MQTT_PASS = "Hung123456789";

let client = new Paho.MQTT.Client(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID);

// ===============================================
// ROOM ID - SET TỪ room_main.js
// ===============================================
let currentRoom = 'livingroom';

export function setCurrentRoom(roomId) {
    currentRoom = roomId;
    console.log(`✅ MQTT room set to: ${currentRoom}`);
}

export function getCurrentRoom() {
    return currentRoom;
}

// ===============================================
// CALLBACK - CẬP NHẬT UI
// ===============================================
let onSensorUpdate = null;
let onActuatorUpdate = null;
let onAutoModeUpdate = null;

export function setOnSensorUpdate(callback) {
    onSensorUpdate = callback;
}

export function setOnActuatorUpdate(callback) {
    onActuatorUpdate = callback;
}

export function setOnAutoModeUpdate(callback) {
    onAutoModeUpdate = callback;
}

// ===============================================
// XỬ LÝ TIN NHẮN MQTT
// ===============================================
function onMessageArrived(message) {
    const topic = message.destinationName;
    const payload = message.payloadString;

    // Parse room từ topic: smarthome/{room}/sensors/temp
    const parts = topic.split("/");
    const msgRoom = parts[1];

    // ✅ CHỈ XỬ LÝ MESSAGE TỪ ROOM HIỆN TẠI
    if (msgRoom !== currentRoom && msgRoom !== 'auto') {
        return; // Bỏ qua message từ phòng khác
    }

    // Xử lý sensor data
    if (topic.includes("/sensors/")) {
        let data;
        try {
            data = JSON.parse(payload);
        } catch (e) {
            data = { value: parseFloat(payload) };
        }

        const sensorName = parts[3]; // temp, humi, co2

        // Update UI
        if (sensorName === "temp") {
            const el = document.getElementById("tempValue");
            if (el) el.innerText = parseFloat(data.value).toFixed(1);
        }
        else if (sensorName === "humi") {
            const el = document.getElementById("humiValue");
            if (el) el.innerText = parseFloat(data.value).toFixed(1);
        }
        else if (sensorName === "co2") {
            const el = document.getElementById("co2Value");
            if (el) el.innerText = Math.round(data.value);
        }

        // Callback cho chart
        if (onSensorUpdate) {
            onSensorUpdate(sensorName, data.value);
        }

        // Lưu Firebase
        const timestamp = Date.now();
        const dbPath = `smarthome/${currentRoom}/sensors/${sensorName}/${timestamp}`;
        update(ref(db, dbPath), { value: data.value }).catch(console.error);
    }
    // Xử lý actuator reported
    else if (topic.includes("/reported")) {
        try {
            const data = JSON.parse(payload);
            const deviceName = parts[3]; // fan, led, buzzer

            if (onActuatorUpdate) {
                onActuatorUpdate(deviceName, data);
            }
        } catch (e) {
            console.error("❌ Reported parse error:", e);
        }
    }
    // Xử lý auto mode
    else if (topic === "smarthome/auto") {
        // Auto mode là global, không filter theo room
        try {
            const data = JSON.parse(payload);
            console.log("🤖 Auto mode:", data);
            // Notify listeners (e.g., UI) about global auto mode change
            if (onAutoModeUpdate) {
                try {
                    // Provide boolean and full object for flexibility
                    onAutoModeUpdate({ isAuto: (data.state === 'ON'), raw: data });
                } catch (e) {
                    console.error('❌ onAutoModeUpdate callback error', e);
                }
            }
        } catch (e) {
            console.error("❌ Auto mode parse error:", e);
        }
    }
}

// ===============================================
// XỬ LÝ MẤT KẾT NỐI
// ===============================================
function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.log("❌ MQTT disconnected:", responseObject.errorMessage);
        setTimeout(connectMQTT, 5000);
    }
}

// ===============================================
// KẾT NỐI MQTT
// ===============================================
export function connectMQTT() {
    console.log("🔄 Connecting MQTT...");

    client.onConnectionLost = onConnectionLost;
    client.onMessageArrived = onMessageArrived;

    client.connect({
        onSuccess: onConnect,
        onFailure: onFailure,
        userName: MQTT_USER,
        password: MQTT_PASS,
        useSSL: true,
        keepAliveInterval: 60,
        cleanSession: true
    });
}

function onConnect() {
    console.log("✅ MQTT connected!");

    // Subscribe TẤT CẢ rooms (để nhận nếu user switch trang)
    client.subscribe("smarthome/+/sensors/#");
    client.subscribe("smarthome/+/actuators/#");
    client.subscribe("smarthome/+/actuators/+/reported");
    client.subscribe("smarthome/auto");

    console.log(`✅ Subscribed - Current room: ${currentRoom}`);
}

function onFailure(message) {
    console.log("❌ MQTT connect failed:", message.errorMessage);
    setTimeout(connectMQTT, 5000);
}

// ===============================================
// GỬI LỆNH MQTT
// ===============================================
export function sendMQTTCommand(topic, messageObj) {
    if (client.isConnected()) {
        const payload = JSON.stringify(messageObj);

        let message = new Paho.MQTT.Message(payload);
        message.destinationName = topic;
        message.qos = 1;

        client.send(message);
        console.log("📤 MQTT TX:", topic, "->", payload);
    } else {
        console.error("❌ MQTT not connected!");
        connectMQTT();
    }
}

export { client };
