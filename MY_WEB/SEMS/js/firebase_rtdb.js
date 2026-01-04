// Simple helper for Firebase Realtime Database using REST with an auth token
// Usage: import { getData, setData, pushData, updateData, deleteData } from './firebase_rtdb.js'

const DB_URL = "https://sems-66e00-default-rtdb.firebaseio.com";
const DB_TOKEN = "v29n4KDwIMtxLjdnIHoKy7w7ovNlPo4M7jDef4c2";

async function request(path, options = {}) {
  const url = `${DB_URL}/${path}.json?auth=${DB_TOKEN}`;
  const res = await fetch(url, options);
  if (!res.ok) {
    const text = await res.text();
    throw new Error(`Firebase RTDB request failed: ${res.status} ${res.statusText} - ${text}`);
  }
  return res.json();
}

export async function getData(path) {
  return request(path, { method: 'GET' });
}

export async function setData(path, data) {
  return request(path, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  });
}

export async function pushData(path, data) {
  return request(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  });
}

export async function updateData(path, data) {
  return request(path, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  });
}

export async function deleteData(path) {
  return request(path, { method: 'DELETE' });
}

export const rtdb = {
  url: DB_URL,
  token: DB_TOKEN,
};

// NOTE: Putting a database token in client-side code is insecure for production.
// Use Firebase Authentication or server-side endpoints for protected writes.
