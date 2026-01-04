import { initializeApp } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-app.js";
import { getAuth } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-auth.js";
import { getDatabase } from "https://www.gstatic.com/firebasejs/12.6.0/firebase-database.js";

const firebaseConfig = {
  
  apiKey: "AIzaSyCpa4iUnaNVGnT8wt0EM300rss7dPiO2Io",
  authDomain: "sems-66e00.firebaseapp.com",
  databaseURL: "https://sems-66e00-default-rtdb.firebaseio.com", 
  projectId: "sems-66e00",
  storageBucket: "sems-66e00.firebasestorage.app",
  messagingSenderId: "594371012516",
  appId: "1:594371012516:web:2283ea8f4bad34490ec331",
  measurementId: "G-939589BVP3"
};


const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getDatabase(app); 

export { auth, db }