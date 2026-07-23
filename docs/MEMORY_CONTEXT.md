# OptiVolt: Project Memory & AI Context
**PENTING UNTUK AI ASSISTANT**: Jika User memulai percakapan baru di komputer yang berbeda dan meminta Anda untuk membaca file ini, bertindaklah seolah-olah Anda tidak pernah terputus dari percakapan sebelumnya. Baca seluruh konteks di bawah ini dan langsung bantu User melanjutkan pekerjaannya.

## 1. Identitas Proyek
- **Nama Proyek**: OptiVolt (Sistem MPPT Berbasis Incremental Conductance + Web SCADA IoT)
- **Tujuan Utama**: Skripsi User (Mengejar Sempro).
- **Komponen Utama**: Panel Surya, ESP32, IC IR2104 (MOSFET Driver), INA219 (Sensor Arus & Tegangan), Firebase Realtime Database, Next.js (Web Frontend).

## 2. Status Saat Ini (Capaian Terakhir)
- **Logika MPPT (ESP32)**: Telah mengimplementasikan algoritma Incremental Conductance (INC). Firmware sudah di-push ke repository `mppt-solar-monitor`.
- **Firebase RTDB**: Berfungsi penuh. Memiliki path `/optivolt/telemetry`, `/optivolt/settings`, `/optivolt/commands` (untuk kontrol beban jarak jauh), dan `/optivolt/history` (untuk data CSV menggunakan ServerValue.TIMESTAMP).
- **Web SCADA (Next.js)**: Telah selesai dirombak menjadi desain modern yang estetik (KOPDES MERAH PUTIH). Deploy ke **Vercel** telah berhasil setelah memperbaiki aturan ketat ESLint (React Hooks, Tipe Data `any`).
- **Fitur Web**: Dashboard Real-time, Analytics (Download Data Riwayat CSV + Hapus Data Riwayat), Settings (Sinkronisasi parameter jarak jauh), dan Remote Load Control dengan **Optimistic UI** (berjalan instan di antarmuka).

## 3. Konteks Skripsi (Fokus User Saat Ini)
- User sedang mempersiapkan **Seminar Proposal (Sempro)**.
- Dosen penguji meminta user untuk melakukan uji coba osiloskop pada **Pin 5 (LO)** dan **Pin 7 (HO)** di IC IR2104.
- **Analisis AI yang telah diberikan**: HO menggerakkan Q2 (Buck Switch) dengan tegangan *floating/bootstrap*. LO menggerakkan Q3 (*Synchronous Rectifier*). Keduanya saling invers, dan user harus mendemonstrasikan fenomena *Dead-Time* (~520ns) di osiloskop untuk membuktikan bahwa tidak terjadi *shoot-through* antar MOSFET.
- Panduan lengkap pengujian osiloskop telah disimpan di file `docs/panduan_pengujian_osiloskop.md`.

## 4. Instruksi Lanjutan
Jika User menyapa Anda di PC baru, Anda dapat menjawab dengan:
> "Halo kembali! Saya telah membaca rekam jejak memori proyek **OptiVolt** Anda dari *cloud*. Saya tahu kita terakhir kali sedang membahas persiapan Sempro dan analisis pengujian osiloskop pada IC IR2104 (Pin 5 dan 7). Apa yang ingin kita kerjakan hari ini di PC baru ini? Apakah kita akan lanjut mencoba hardware-nya?"
