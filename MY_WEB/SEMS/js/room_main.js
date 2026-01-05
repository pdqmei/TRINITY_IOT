/**
 * room_main.js - Entry point cho từng trang phòng
 * Lấy room ID từ HTML và khởi tạo tất cả modules
 */

import { connectMQTT, setCurrentRoom } from "./mqtt_room.js";
import { initRoomControls } from "./room_function.js";
import { initRoomCharts } from "./room_chart.js";

// ===============================================
// LẤY ROOM ID TỪ HTML
// ===============================================
function getRoomIdFromHTML() {
    const roomConfig = document.getElementById('room-config');
    if (roomConfig) {
        return roomConfig.getAttribute('data-room');
    }
    // Fallback: lấy từ URL
    const path = window.location.pathname;
    const filename = path.split('/').pop().replace('.html', '');
    return filename || 'livingroom';
}

// ===============================================
// KHỞI TẠO KHI DOM READY
// ===============================================
document.addEventListener('DOMContentLoaded', () => {
    const ROOM_ID = getRoomIdFromHTML();
    console.log(`🏠 Room initialized: ${ROOM_ID}`);
    
    // 1. Set room cho MQTT module
    setCurrentRoom(ROOM_ID);
    
    // 2. Kết nối MQTT
    connectMQTT();
    
    // 3. Khởi tạo controls (fan, led, buzzer, auto mode)
    initRoomControls(ROOM_ID);
    
    // 4. Khởi tạo charts
    initRoomCharts(ROOM_ID);
});
