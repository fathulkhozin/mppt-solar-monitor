# ☀️ PRO MPPT Solar Charge Controller IoT

Sebuah sistem *dashboard* monitoring dan pengendali *Solar Charge Controller (MPPT)* berbasis Internet of Things (IoT). Proyek ini dibangun dengan antarmuka pengguna *(UI)* premium bernuansa *Glassmorphism Dark Mode* yang sangat memanjakan mata, serta backend yang sangat cepat dan ringan menggunakan **FastAPI**.

---

## ✨ Fitur Utama

- **Real-Time Telemetry**: Memantau tegangan, arus, dan daya *(watt)* baik dari Panel Surya maupun Output secara *real-time* (diperbarui setiap 3 detik).
- **Beautiful Dashboard**: Desain UI modern menggunakan Tailwind CSS dan Chart.js untuk menampilkan grafik historis arus dan tegangan.
- **2-Way Settings Sync**: Mengubah pengaturan alat (Tegangan Maksimal, Arus Maksimal, Tipe Baterai) langsung dari web, yang akan otomatis tersinkronisasi ke memori ESP32 *(EEPROM/Preferences)*.
- **Smart Connection Detection**: Dashboard dapat mendeteksi jika koneksi WiFi ESP32 terputus (jika tidak ada data masuk selama lebih dari 10 detik).
- **Remote OTA Firmware Update**: Mem-flash/mengunggah program baru (`.bin`) ke ESP32 secara nirkabel langsung melalui form di dalam Dashboard.

## 🛠️ Teknologi yang Digunakan

- **Backend**: Python 3, FastAPI, Uvicorn, httpx
- **Frontend**: HTML5, Vanilla JavaScript, Tailwind CSS (via CDN), Chart.js, Lucide Icons
- **Hardware Target**: ESP32-C3 Supermini, Sensor INA219

## 🚀 Cara Menjalankan Server Lokal (Laptop/PC)

1. **Pastikan Python sudah terinstal** di laptop Anda.
2. Buat dan aktifkan *Virtual Environment* (opsional tapi disarankan):
   ```bash
   python -m venv venv
   # Di Windows:
   .\venv\Scripts\activate
   ```
3. Install semua pustaka yang dibutuhkan:
   ```bash
   pip install fastapi uvicorn httpx
   ```
4. Jalankan server FastAPI:
   ```bash
   python server.py
   # Atau bisa menggunakan: uvicorn server:app --host 0.0.0.0 --port 8000
   ```
5. Buka Browser dan akses: `http://localhost:8000` (Gunakan username `fathul` dan password `fathul` untuk login).

## 📡 Konfigurasi Hardware (ESP32)

Pastikan kode Arduino Anda mengirim POST Request (dalam format JSON) ke alamat `http://<IP_LAPTOP>:8000/api/data` setiap beberapa detik sekali.

Format JSON yang diharapkan oleh Server:
```json
{
  "v_plts": 18.5,
  "i_plts": 2.1,
  "batt_pct": 85.0,
  "v_out": 13.8,
  "i_out": 1.5,
  "sol_vmax": 18,
  "sol_imax": 7,
  "bat_type": 0,
  "sys_volt": 12,
  "bat_cap": 50
}
```

## 📸 Tampilan Dashboard

<p align="center">
  <img width="800" alt="Screenshot 1" src="https://github.com/user-attachments/assets/143bb215-8630-4fef-a1f8-b3bfe6f2abdb" />
</p>
<br>
<p align="center">
  <img width="800" alt="Screenshot 2" src="https://github.com/user-attachments/assets/b4c0836c-b75d-48e5-b6a0-c2aff913960f" />
</p>
<br>
<p align="center">
  <img width="800" alt="Screenshot 3" src="https://github.com/user-attachments/assets/50f18236-b4c9-4490-9dc4-18d2767eda58" />
</p>
---
*Dibuat khusus untuk proyek MPPT IoT Fathul (2026).*

---
### 🤖 Instruksi Khusus Lanjutan Proyek (AI Assistant)
Jika Anda membuka repositori ini dari komputer/PC baru dan ingin melanjutkan percakapan pengembangan proyek OptiVolt ini dengan AI Assistant (Antigravity), cukup copy-paste kalimat sakti berikut ke dalam chat AI Anda:

> *"Tolong baca file `docs/MEMORY_CONTEXT.md` di repository github saya, lalu mari kita lanjutkan proyek OptiVolt ini."*
