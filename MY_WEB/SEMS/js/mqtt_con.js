import { updateSensorFromMQTT } from "./function.js";

// ===============================================
// CONFIG MQTT BROKER
// ===============================================
const MQTT_HOST = "19059388a61f4c8286066fda62e74315.s1.eu.hivemq.cloud";
const MQTT_PORT = 8884;
const MQTT_CLIENT_ID = "WebApp_" + Math.random().toString(16).substr(2, 8);

const MQTT_USER = "trinity";      
const MQTT_PASS = "Hung123456789"; 

let client = new Paho.MQTT.Client(MQTT_HOST, MQTT_PORT, MQTT_CLIENT_ID);

// Biến lưu giá trị sensor
let tempValue = null;
let humiValue = null;
let co2Value = null;

// ===============================================
// XỬ LÝ TIN NHẮN MQTT
// ===============================================
function onMessageArrived(message) {
    const topic = message.destinationName;
    const payload = message.payloadString;
    
    console.log("📩 MQTT RX:", topic, "->", payload);

    if (topic.includes("/sensors/")) {
        let data;
        try {
            data = JSON.parse(payload);
        } catch (e) {
            console.error("❌ Payload không phải JSON:", payload);
            data = { value: parseFloat(payload) };
        }

        if (topic.includes("/temp")) {
            tempValue = parseFloat(data.value);
            const tempElement = document.getElementById("tempValue");
            if (tempElement) {
                tempElement.innerText = tempValue.toFixed(1);
            }
            console.log("🌡️ Temperature updated:", tempValue);
        }
        else if (topic.includes("/humi")) {
            humiValue = parseFloat(data.value);
            const humiElement = document.getElementById("humiValue");
            if (humiElement) {
                humiElement.innerText = humiValue.toFixed(1);
            }
            console.log("💧 Humidity updated:", humiValue);
        }
        else if (topic.includes("/co2")) {
            co2Value = parseFloat(data.value);
            const co2Element = document.getElementById("co2Value");
            if (co2Element) {
                co2Element.innerText = Math.round(co2Value);
            }
            console.log("☁️ CO2 updated:", co2Value);
        }

        updateSensorFromMQTT(topic, data);
        console.log("✅ Sensor data processed:", topic, data);
    }
    else if (topic.includes("/reported")) {
        try {
            const data = JSON.parse(payload);
            console.log("📡 Actuator reported:", topic, data);
            
            if (data.success === false) {
                const deviceName = topic.split("/")[3];
                console.error(`❌ Hardware error on ${deviceName}:`, data);
                alert(`⚠️ Device ${deviceName} failed to execute command!`);
            }
        } catch (e) {
            console.error("❌ Reported payload không phải JSON:", payload);
        }
    }
    else if (topic.includes("/actuators/") && !topic.includes("/reported")) {
        try {
            const data = JSON.parse(payload);
            console.log("🔧 Actuator feedback:", topic, data);
        } catch (e) {
            console.error("❌ Actuator payload không phải JSON:", payload);
        }
    }
    else if (topic.includes("smarthome/auto")) {
        try {
            const data = JSON.parse(payload);
            console.log("🤖 Auto mode:", data);
        } catch (e) {
            console.error("❌ Auto mode payload không phải JSON:", payload);
        }
    }
}

// ===============================================
// XỬ LÝ MẤT KẾT NỐI
// ===============================================
function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.log("❌ Mất kết nối MQTT:", responseObject.errorMessage);
        setTimeout(connectMQTT, 5000);
    }
}

// ===============================================
// HÀM KẾT NỐI MQTT
// ===============================================
export function connectMQTT() {
    console.log("🔄 Đang kết nối MQTT...");
    
    // ✅ GÁN CALLBACK TRƯỚC KHI CONNECT
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
    console.log("✅ Đã kết nối MQTT thành công!");
    
    client.subscribe("smarthome/+/sensors/#", {
        onSuccess: () => console.log("✅ Subscribed to sensors"),
        onFailure: (err) => console.error("❌ Subscribe sensors failed:", err)
    });
    
    client.subscribe("smarthome/+/actuators/#", {
        onSuccess: () => console.log("✅ Subscribed to actuators"),
        onFailure: (err) => console.error("❌ Subscribe actuators failed:", err)
    });
    
    client.subscribe("smarthome/+/actuators/+/reported", {
        onSuccess: () => console.log("✅ Subscribed to actuator reported states"),
        onFailure: (err) => console.error("❌ Subscribe reported failed:", err)
    });
    
    client.subscribe("smarthome/auto", {
        onSuccess: () => console.log("✅ Subscribed to auto mode"),
        onFailure: (err) => console.error("❌ Subscribe auto failed:", err)
    });
}

function onFailure(message) {
    console.log("❌ Kết nối MQTT thất bại:", message.errorMessage);
    setTimeout(connectMQTT, 5000);
}

// ===============================================
// HÀM GỬI LỆNH (PUBLISH)
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
        console.error("❌ Chưa kết nối MQTT, không thể gửi lệnh!");
        connectMQTT();
    }
}

export { client, tempValue, humiValue, co2Value };