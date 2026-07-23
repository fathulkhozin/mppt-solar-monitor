# Panduan Pengujian Skripsi MPPT (Incremental Conductance) dengan Osiloskop

Dokumen ini adalah panduan strategis untuk mempersiapkan Seminar Proposal (Sempro) dan pengambilan data pengujian perangkat keras MPPT (Maximum Power Point Tracking) dengan algoritma Incremental Conductance (INC) menggunakan osiloskop.

## 1. Titik Pengukuran (Test Points) pada Osiloskop

Untuk membandingkan kinerja PLTS secara langsung vs dengan MPPT, Anda perlu meletakkan probe osiloskop pada titik-titik (Test Points) berikut di PCB/Rangkaian Anda:

### TP1: Sinyal PWM (Output ESP32 / Input MOSFET Driver)
- **Tujuan**: Melihat langsung hasil olahan logika **Incremental Conductance (INC)**. Logika INC bekerja dengan menambah atau mengurangi nilai *Duty Cycle* (lebar pulsa).
- **Yang diamati**: Perubahan lebar pulsa kotak (PWM) saat cuaca berubah atau saat beban dinyalakan. Jika cuaca mendung, Anda harus bisa melihat *Duty Cycle* berubah secara otomatis di layar osiloskop.

### Pengujian Khusus Dosen: Pin 5 (LO) dan Pin 7 (HO) IC IR2104
Berdasarkan gambar rangkaian Anda, komponen **U2** adalah IC **IR2104** yang berfungsi sebagai *Half-Bridge Driver*. Dosen Anda meminta menguji Pin 5 dan 7 karena ini adalah **jantung utama penggerak (switching) dari Buck Converter** Anda.
- **Pin 7 (HO - High Output)**: Pin ini terhubung ke *Gate* MOSFET atas (**Q2**). Ini adalah saklar utama penyalur daya dari Panel Surya ke Induktor (L1). Saat HO bernilai tinggi (ON), daya mengalir dari panel ke baterai/beban. Anda harus melihat sinyal kotak (PWM) dengan amplitudo tegangan yang "mengambang" (*floating* / *bootstrap* akibat kapasitor C3 dan dioda D4).
- **Pin 5 (LO - Low Output)**: Pin ini terhubung ke *Gate* MOSFET bawah (**Q3**). Q3 berfungsi sebagai *Synchronous Rectifier* (pengganti dioda Schottky) yang berguna mengalirkan arus sisa induktor saat Q2 mati. 
- **Penting untuk Skripsi (Dead-Time Analysis)**: Saat menguji di osiloskop, tempelkan Probe 1 di Pin 7 (HO) dan Probe 2 di Pin 5 (LO). Anda **harus menunjukkan kepada dosen** bahwa saat HO menyala, LO pasti mati, dan sebaliknya (saling invers). Di layar osiloskop, perlihatkan juga adanya jeda kosong sepersekian mikrodetik saat keduanya mati (*Dead-Time* bawaan IR2104, sekitar 520ns) untuk mencegah terjadinya konsleting (*shoot-through*) antara MOSFET atas dan bawah. Ini adalah poin analisis teknis yang sangat bernilai untuk skripsi elektronika daya!

### TP2: Tegangan Input dari Panel Surya (V_PLTS)
- **Tujuan**: Melihat kurva tegangan asli dari panel surya.
- **Kondisi Tanpa MPPT**: Tegangan panel surya akan "jatuh" (drop) mendekati tegangan aki jika dihubungkan langsung.
- **Kondisi Dengan MPPT (INC)**: Tegangan panel akan dipertahankan pada titik optimalnya (V_MPP, misalnya 17-18V untuk panel 12V), meskipun tegangan aki hanya 12V. Di osiloskop, ini akan terlihat sebagai garis tegangan DC yang stabil di atas batas jatuh. Anda juga bisa mengukur **Voltage Ripple** (riak tegangan) akibat *switching* MOSFET di sini.

### TP3: Tegangan Output ke Baterai (V_OUT)
- **Tujuan**: Membuktikan bahwa topologi *Buck Converter* bekerja menurunkan tegangan V_PLTS menjadi tegangan pengisian aki yang aman (misal 13.8V - 14.4V).
- **Yang diamati**: Bentuk sinyal DC yang rata dan stabil. Gunakan mode *AC Coupling* pada osiloskop untuk melihat riak noise (ripple). Semakin kecil riaknya, semakin bagus desain induktor dan kapasitor Anda.

---

## 2. Skenario Pengujian Data untuk Skripsi

Dosen penguji akan sangat menyukai data yang bersifat komparatif (perbandingan). Siapkan 3 skenario pengambilan gambar/data osiloskop ini:

> [!IMPORTANT]
> **Skenario 1: Sistem Tanpa MPPT (Direct Coupled)**
> Hubungkan Panel Surya langsung ke Baterai/Aki (atau melalui dioda penahan saja). 
> **Hasil di Osiloskop**: Tegangan Panel (V_PLTS) akan terseret turun menyamai tegangan aki (misal 12.5V). Ukur arusnya. Hitung Daya = V x I. Ini adalah "Daya Konvensional".

> [!TIP]
> **Skenario 2: Sistem Dengan MPPT (Logika INC)**
> Hubungkan melalui alat OptiVolt Anda. 
> **Hasil di Osiloskop**: Tunjukkan bahwa V_PLTS tetap berada di kisaran 17V-18V (titik puncak daya), sementara V_OUT menyesuaikan baterai (13V). Hitung Daya = V_PLTS x I_PLTS. Tunjukkan bahwa Daya dengan MPPT **lebih besar** daripada Skenario 1. Ini adalah inti dari skripsi Anda!

> [!NOTE]
> **Skenario 3: Uji Respon Dinamis (Dynamic Response)**
> Saat sistem berjalan dan osiloskop merekam, tutup sebagian panel surya dengan kardus tebal (simulasi awan).
> **Hasil di Osiloskop**: Anda akan melihat grafik tegangan dan sinyal PWM bergeser secara adaptif (*tracking*) mencari titik tegangan baru. Saat kardus dilepas, grafik akan kembali naik. Gambar kurva fluktuasi ini sangat disukai dosen sebagai bukti bahwa algoritma **Incremental Conductance** benar-benar berjalan secara *Real-Time*, bukan sekadar *buck converter* biasa.

---

## 3. Persiapan Seminar Proposal (Sempro)

Agar Sempro Anda mulus, pastikan proposal Anda menekankan pada poin-poin "kebaruan" (*novelty*) atau penerapan praktis ini:

### Rekomendasi Judul Skripsi
1. *"Rancang Bangun Maximum Power Point Tracker (MPPT) Berbasis Algoritma Incremental Conductance dengan Monitoring IoT Real-Time Terintegrasi"*
2. *"Analisis Perbandingan Kinerja Panel Surya Menggunakan MPPT Logika Incremental Conductance dan Sistem Direct-Coupled dengan Pemantauan Berbasis Web"*

### Rumusan Masalah yang Harus Dijawab
1. Bagaimana merancang topologi *buck converter* yang mampu dikendalikan oleh algoritma Incremental Conductance pada ESP32?
2. Bagaimana perbandingan efisiensi dan daya keluaran yang dihasilkan panel surya dengan dan tanpa algoritma INC?
3. Bagaimana mengintegrasikan sistem MPPT ke dalam ekosistem IoT untuk pemantauan data jarak jauh (Web SCADA)?

### Alat yang Perlu Dibawa Saat Pengujian / Sidang
- Osiloskop Digital (minimal 2 channel).
- Multimeter Digital (untuk kalibrasi dan validasi sensor INA219).
- Tang Ampere DC (jika ada, untuk validasi arus tinggi).
- Aki/Baterai 12V dan Panel Surya (atau *Variable DC Power Supply* sebagai pengganti matahari jika di dalam ruangan).
