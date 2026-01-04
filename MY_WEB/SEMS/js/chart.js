import { db } from './config_firebase.js';
import { ref, get, onChildAdded, off, query, orderByKey, limitToLast} from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js";

//================================================
// GLOBAL VARIABLES
// ===============================================
let currentRoom = 'bedroom'; // Mặc định phòng ban đầu  


class SensorChartController {
    constructor({ db, chart, sensor }) {
        this.db = db;
        this.chart = chart;
        this.sensor = sensor;

        this.room = null;
        this.ref = null;
        this.unsubscribe = null;
        this.lastKey = null;
    }

    clearChart() {
        this.chart.data.labels.length = 0;
        this.chart.data.datasets[0].data.length = 0;
        this.chart.update('none');
    }

    async loadInitial(room) {
        const path = `smarthome/${room}/sensors/${this.sensor}`;

        const q = query(
            ref(this.db, path),
            orderByKey(),
            limitToLast(10)
        );

        const snap = await get(q);
        if (!snap.exists()) return;

        snap.forEach(child => {
            this.chart.data.labels.push(formatTime(child.key));
            this.chart.data.datasets[0].data.push(child.val().value);
            this.lastKey = child.key;
        });

        this.chart.update('none');
    }

    attachRealtime(room) {
        const path = `smarthome/${room}/sensors/${this.sensor}`;
        this.ref = ref(this.db, path);

        const q = query(this.ref, orderByKey(), limitToLast(1));

        this.unsubscribe = onChildAdded(q, snap => {
            const key = snap.key;
            const value = snap.val().value;

            if (key === this.lastKey) return;

            this.lastKey = key;

            this.chart.data.labels.push(formatTime(key));
            this.chart.data.datasets[0].data.push(value);

            if (this.chart.data.labels.length > 10) {
                this.chart.data.labels.shift();
                this.chart.data.datasets[0].data.shift();
            }

            this.chart.update();
        });
    }

    detach() {
        if (this.ref) {
            off(this.ref);
            this.ref = null;
            this.unsubscribe = null;
            this.lastKey = null;
        }
    }

    async switchRoom(room) {
        if (room === this.room) return;

        this.detach();
        this.clearChart();

        this.room = room;

        await this.loadInitial(room);
        this.attachRealtime(room);
    }
}

// ===============================================
// HELPER FUNCTIONS
// ===============================================

function formatTime(t) {
    if (!t) return '';
    try {
        // Xử lý Firebase timestamp object
        if (typeof t === 'object' && t !== null && typeof t.toDate === 'function') {
            return t.toDate().toLocaleString();
        }
        // Xử lý timestamp number (milliseconds)
        const d = new Date(Number(t));
        if (!isNaN(d)) {
            return d.toLocaleTimeString('vi-VN', {
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit'
            });
        }
    } catch (e) {
        console.error('Error formatting time:', e);
    }
    return String(t);
}

function createLineChart(ctx, label, color) {
    return new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: label,
                data: [],
                borderColor: color,
                backgroundColor: 'transparent', // ✅ Không fill màu
                borderWidth: 2,
                tension: 0.3,
                pointRadius: 3,
                pointBackgroundColor: color,
                pointBorderColor: '#fff',
                pointBorderWidth: 1,
                fill: false // ✅ Tắt fill
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    display: true,
                    position: 'top'
                }
            },
            scales: {
                x: {
                    display: true,
                    title: { display: true, text: 'Time' },
                    ticks: {
                        maxTicksLimit: 10
                    }
                },
                y: {
                    display: true
                }
            }
        }
    });
}
// Get canvas contexts
const tempCtx = document.getElementById('temperatureChart');
const humiCtx = document.getElementById('humidityChart');
const co2Ctx = document.getElementById('co2Chart');

const tempChart = createLineChart(tempCtx, 'Temperature (°C)', 'rgba(255,99,132,1)');
const humiChart = createLineChart(humiCtx, 'Humidity (%)', 'rgba(54,162,235,1)');
const co2Chart = createLineChart(co2Ctx, 'CO2 (ppm)', 'rgba(75,192,192,1)');

// ===============================================
// CONTROLLERS + ROOM SWITCH API
// ===============================================
const tempCtrl = new SensorChartController({ db, chart: tempChart, sensor: 'temp' });
const humiCtrl = new SensorChartController({ db, chart: humiChart, sensor: 'humi' });
const co2Ctrl = new SensorChartController({ db, chart: co2Chart, sensor: 'co2' });

export async function switchChartsRoom(newRoom) {
    currentRoom = newRoom;
    console.log(`📊 Charts switched to room: ${newRoom}`);

    await Promise.all([
        tempCtrl.switchRoom(newRoom),
        humiCtrl.switchRoom(newRoom),
        co2Ctrl.switchRoom(newRoom)
    ]);
}

// ===============================================
// KHỞI TẠO KHI TẢI TRANG
// ===============================================
console.log('📊 Initializing charts for room:', currentRoom);
switchChartsRoom(currentRoom);

// Export charts and controllers
export { tempChart, humiChart, co2Chart, tempCtrl, humiCtrl, co2Ctrl, currentRoom };