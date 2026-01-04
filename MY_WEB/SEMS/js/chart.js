import { db } from './config_firebase.js';

import { ref, get, onValue, query, orderByChild, limitToLast } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js";


function formatTime(t) {
    if (!t) return '';
    // timestamp number or ISO string
    try {
        if (typeof t === 'object' && t !== null && typeof t.toDate === 'function') {
            return t.toDate().toLocaleString();
        }
        const d = new Date(t);
        if (!isNaN(d)) return d.toLocaleString();
    } catch (e) {}
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
                backgroundColor: color,
                borderWidth: 2,
                tension: 0.3,
                pointRadius: 2,
                fill: false
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    display: true,
                    title: { display: true, text: 'Time' }
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

async function loadInitialData(collectionName, valueField, chart) {
    try {
        const q = query(ref(db, collectionName), orderByChild('time'), limitToLast(200));
        const snap = await get(q);
        const labels = [];
        const data = [];
        snap.forEach(child => {
            const d = child.val();
            labels.push(formatTime(d.time));
            data.push(d[valueField]);
        });
        chart.data.labels = labels;
        chart.data.datasets[0].data = data;
        chart.update();
        console.log(`Loaded initial data for ${collectionName}`, { count: labels.length });
    } catch (err) {
        console.error('Error loading initial data for', collectionName, err);
    }
}

function listenRealtime(collectionName, valueField, chart) {
    const q = query(ref(db, collectionName), orderByChild('time'), limitToLast(200));
    onValue(q, (snapshot) => {
        const labels = [];
        const data = [];
        snapshot.forEach(child => {
            const d = child.val();
            labels.push(formatTime(d.time));
            data.push(d[valueField]);
        });
        chart.data.labels = labels;
        chart.data.datasets[0].data = data;
        chart.update();
        console.log(`Realtime update for ${collectionName}`, { count: labels.length });
    }, (err) => {
        console.error('Realtime listener error for', collectionName, err);
    });
}

// Collections assumed: 'temperatureData', 'humidityData', 'co2Data'
// Documents should contain fields: time (timestamp or ISO string/number) and temp/humi/co2 as numeric values.

loadInitialData('temperatureData', 'temp', tempChart);
loadInitialData('humidityData', 'humi', humiChart);
loadInitialData('co2Data', 'co2', co2Chart);

listenRealtime('temperatureData', 'temp', tempChart);
listenRealtime('humidityData', 'humi', humiChart);
listenRealtime('co2Data', 'co2', co2Chart);

// Export charts and helper functions so other modules can reuse them without duplicating code
export { tempChart, humiChart, co2Chart, loadInitialData, listenRealtime };
