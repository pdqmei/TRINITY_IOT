/**
 * room_chart.js - Charts cho từng phòng riêng biệt
 * Đơn giản hơn chart.js gốc - chỉ vẽ chart cho 1 room
 */

import { db } from './config_firebase.js';
import { ref, query, orderByKey, limitToLast, get, onValue } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js";

// ===============================================
// BIẾN MODULE
// ===============================================
let currentRoom = 'livingroom';
let tempChart, humiChart, co2Chart;

const MAX_POINTS = 20;  // Số điểm dữ liệu tối đa

const CHART_CONFIG = {
    temp: {
        label: 'Temperature (°C)',
        borderColor: 'rgb(255, 99, 132)',
        backgroundColor: 'rgba(255, 99, 132, 0.2)',
        min: 0,
        max: 50
    },
    humi: {
        label: 'Humidity (%)',
        borderColor: 'rgb(54, 162, 235)',
        backgroundColor: 'rgba(54, 162, 235, 0.2)',
        min: 0,
        max: 100
    },
    co2: {
        label: 'CO2 (ppm)',
        borderColor: 'rgb(75, 192, 192)',
        backgroundColor: 'rgba(75, 192, 192, 0.2)',
        min: 0,
        max: 3000
    }
};

// ===============================================
// KHỞI TẠO CHARTS
// ===============================================
export function initRoomCharts(roomId) {
    currentRoom = roomId;
    
    // Tạo charts
    tempChart = createChart('tempChart', CHART_CONFIG.temp);
    humiChart = createChart('humiChart', CHART_CONFIG.humi);
    co2Chart = createChart('co2Chart', CHART_CONFIG.co2);
    
    // Load dữ liệu từ Firebase
    loadInitialData();
    
    // Listen realtime updates
    setupRealtimeListeners();
    
    console.log(`📊 Charts initialized for room: ${roomId}`);
}

function createChart(canvasId, config) {
    const ctx = document.getElementById(canvasId);
    if (!ctx) {
        console.warn(`Canvas ${canvasId} not found`);
        return null;
    }
    
    return new Chart(ctx.getContext('2d'), {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: config.label,
                data: [],
                borderColor: config.borderColor,
                backgroundColor: config.backgroundColor,
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 3
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: false
                }
            },
            scales: {
                x: {
                    display: true,
                    ticks: {
                        maxTicksLimit: 5,
                        color: '#aaa'
                    },
                    grid: {
                        color: 'rgba(255,255,255,0.1)'
                    }
                },
                y: {
                    display: true,
                    min: config.min,
                    max: config.max,
                    ticks: {
                        color: '#aaa'
                    },
                    grid: {
                        color: 'rgba(255,255,255,0.1)'
                    }
                }
            },
            animation: {
                duration: 300
            }
        }
    });
}

// ===============================================
// LOAD DỮ LIỆU TỪ FIREBASE
// ===============================================
async function loadInitialData() {
    const sensors = ['temp', 'humi', 'co2'];
    const charts = { temp: tempChart, humi: humiChart, co2: co2Chart };
    
    for (const sensor of sensors) {
        const path = `smarthome/${currentRoom}/sensors/${sensor}`;
        try {
            const snapshot = await get(query(ref(db, path), orderByKey(), limitToLast(MAX_POINTS)));
            
            if (snapshot.exists()) {
                const data = snapshot.val();
                const labels = [];
                const values = [];
                
                Object.keys(data).sort().forEach(key => {
                    const timestamp = data[key].timestamp || key;
                    labels.push(formatTime(timestamp));
                    values.push(data[key].value);
                });
                
                if (charts[sensor]) {
                    charts[sensor].data.labels = labels;
                    charts[sensor].data.datasets[0].data = values;
                    charts[sensor].update('none');
                }
            }
        } catch (error) {
            console.error(`❌ Load ${sensor} data error:`, error);
        }
    }
}

// ===============================================
// REALTIME LISTENERS
// ===============================================
function setupRealtimeListeners() {
    const sensors = ['temp', 'humi', 'co2'];
    const charts = { temp: tempChart, humi: humiChart, co2: co2Chart };
    const valueIds = { temp: 'tempValue', humi: 'humiValue', co2: 'co2Value' };
    
    sensors.forEach(sensor => {
        const path = `smarthome/${currentRoom}/sensors/${sensor}`;
        const latestQuery = query(ref(db, path), orderByKey(), limitToLast(1));
        
        onValue(latestQuery, (snapshot) => {
            if (!snapshot.exists()) return;
            
            const data = snapshot.val();
            const latestKey = Object.keys(data)[0];
            const value = data[latestKey].value;
            const timestamp = data[latestKey].timestamp || latestKey;
            
            // Update value display
            const valueEl = document.getElementById(valueIds[sensor]);
            if (valueEl) {
                if (sensor === "temp" || sensor === "humi") {
                    valueEl.innerText = parseFloat(value).toFixed(1);
                } else {
                    valueEl.innerText = Math.round(value);
                }
            }
            
            // Update chart
            const chart = charts[sensor];
            if (chart) {
                addDataPoint(chart, formatTime(timestamp), value);
            }
        });
    });
}

// ===============================================
// HELPER FUNCTIONS
// ===============================================
function addDataPoint(chart, label, value) {
    // Tránh duplicate point
    const lastLabel = chart.data.labels[chart.data.labels.length - 1];
    if (lastLabel === label) return;
    
    chart.data.labels.push(label);
    chart.data.datasets[0].data.push(value);
    
    // Giới hạn số điểm
    if (chart.data.labels.length > MAX_POINTS) {
        chart.data.labels.shift();
        chart.data.datasets[0].data.shift();
    }
    
    chart.update('none');
}

function formatTime(timestamp) {
    if (!timestamp) return '';
    
    // Nếu là timestamp dạng YYYYMMDD_HHMMSS
    if (typeof timestamp === 'string' && timestamp.includes('_')) {
        const timePart = timestamp.split('_')[1];
        if (timePart && timePart.length >= 4) {
            return `${timePart.slice(0,2)}:${timePart.slice(2,4)}`;
        }
    }
    
    // Nếu là Unix timestamp
    if (typeof timestamp === 'number') {
        const date = new Date(timestamp * 1000);
        return date.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });
    }
    
    return timestamp;
}

// ===============================================
// UPDATE CHART TỪ MQTT (export cho mqtt_room.js)
// ===============================================
export function updateChartFromMQTT(sensorName, value) {
    const charts = { temp: tempChart, humi: humiChart, co2: co2Chart };
    const chart = charts[sensorName];
    
    if (chart) {
        const now = new Date();
        const timeLabel = now.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });
        addDataPoint(chart, timeLabel, value);
    }
}
