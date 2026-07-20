/*
  =====================================================================
  PRO MPPT SOLAR CHARGE CONTROLLER IoT - v8.0 (Firebase Cloud Edition)
  Microcontroller : ESP32-C3 Supermini (ESP32 Core v3.x)
  Features        : Incremental Conductance, 4 Buttons, 3 LEDs, 
                    Remote Load Control, 2-Way SCADA, Firebase RTDB.
  =====================================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>

// --- FIREBASE LIBRARY ---
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// ======================= DEFINISI PIN ESP32-C3 =======================
#define I2C_SDA       8
#define I2C_SCL       9
#define PIN_PWM_IN    2
#define PIN_SD_EN     3
#define PIN_LOAD      4

// 4 Push Buttons (Aktif LOW)
#define BTN_UP        5
#define BTN_DOWN      6
#define BTN_ENTER     7
#define BTN_BACK      10 

// 3 LED Indikator
#define LED_NIGHT     1  // Kuning (Malam/Standby)
#define LED_BULK      20 // Merah (MPPT Aktif / Bulk)
#define LED_FLOAT     21 // Hijau (Baterai Penuh / Float)

// ======================= KONFIGURASI JARINGAN =======================
const char* WIFI_SSID     = "pandawa5";
const char* WIFI_PASSWORD = "pandawa5";

// ======================= KONFIGURASI FIREBASE =======================
// Masukkan kredensial dari Vercel / Firebase Console Anda
#define API_KEY "AIzaSyBFhRGNAZhEYPMtnNMzMvwGtKQPjgd_TIk"
#define DATABASE_URL "mppt-plts-default-rtdb.asia-southeast1.firebasedatabase.app" 

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

const int POST_INTERVAL = 3000;             

// ======================= OBJEK GLOBAL =======================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_INA219 ina_solar(0x40); 
Adafruit_INA219 ina_bat(0x41);   
Preferences preferences;
WebServer server(80);

// ======================= VARIABEL SISTEM =======================
bool isSetupDone = false;
int sol_vMax = 18, sol_iMax = 5, batType = 0, sysVoltage = 12, batCapacity = 50;

bool loadStatus = false;
float bulkVoltage = 14.4, floatVoltage = 13.8;
int currentDutyCycle = 0;

float sol_V = 0, sol_A = 0, sol_W = 0;
float bat_V = 0, bat_A = 0, bat_W = 0, load_W = 0;
float bat_pct = 0.0;

enum ChargeStage { STAGE_NIGHT, STAGE_BULK, STAGE_FLOAT };
ChargeStage currentStage = STAGE_NIGHT;

enum AppState { STATE_SETUP, STATE_MAIN };
AppState currentState = STATE_MAIN;

int setupStep = 0, mainPage = 0, menuIndex = 0;
unsigned long lastBtnPress = 0, lastSensorRead = 0;
const int debounceDelay = 250; 

// ======================= DEKLARASI FUNGSI =======================
void initBatteryProfile();
void readSensors();
void sendTelemetryData();
void runMPPTAlgorithm();
void readInputs();
void renderSetupWizard();
void renderMainDisplay();
void updateLEDs();

// =====================================================================
//                             SETUP AWAL
// =====================================================================
void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_SD_EN, OUTPUT);
  pinMode(PIN_LOAD, OUTPUT);
  digitalWrite(PIN_SD_EN, LOW); 
  digitalWrite(PIN_LOAD, LOW);  
  
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  pinMode(LED_NIGHT, OUTPUT);
  pinMode(LED_BULK, OUTPUT);
  pinMode(LED_FLOAT, OUTPUT);
  updateLEDs(); 

  ledcAttach(PIN_PWM_IN, 40000, 10); 
  ledcWrite(PIN_PWM_IN, 0); 

  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { Serial.println("OLED Failed!"); for(;;); }
  display.setTextColor(SSD1306_WHITE);

  // --- HARDWARE FACTORY RESET (Tahan Tombol BACK saat Boot) ---
  if (digitalRead(BTN_BACK) == LOW) {
    display.clearDisplay(); display.setTextSize(1);
    display.setCursor(15, 30); display.print("FACTORY RESET..."); display.display();
    preferences.begin("mppt_data", false); preferences.clear();
    delay(2000); ESP.restart();
  }

  // --- SPLASH SCREEN ---
  display.clearDisplay(); 
  display.setTextSize(2);    
  display.setCursor(16, 24); 
  display.print("OptiVolt"); 
  display.display();
  delay(2000);

  // Inisialisasi Sensor
  ina_solar.begin(); 
  ina_solar.setCalibration_32V_32A(); 
  ina_bat.begin();
  ina_bat.setCalibration_32V_32A();

  // --- LOAD NVM DATA ---
  preferences.begin("mppt_data", false);
  isSetupDone = preferences.getBool("setup_done", false);

  if (!isSetupDone) {
    currentState = STATE_SETUP; setupStep = 0;
  } else {
    sol_vMax = preferences.getInt("sol_vmax", 18); sol_iMax = preferences.getInt("sol_imax", 5);
    batType = preferences.getInt("bat_type", 0); sysVoltage = preferences.getInt("sys_volt", 12);
    batCapacity = preferences.getInt("bat_cap", 50);
    currentState = STATE_MAIN; initBatteryProfile();
  }

  // --- WIFI CONNECT ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  int wifi_timeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_timeout < 20) {
    Serial.print(".");
    delay(500);
    wifi_timeout++;
  }
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    
    // --- FIREBASE INIT ---
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    
    // Firebase Auth (Anonymous/No Auth for this use-case if rules are open, but setup as anonymous to be safe)
    if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("Firebase Auth Success");
      signupOK = true;
    } else {
      Serial.printf("Firebase Auth Error: %s\n", config.signer.signupError.message.c_str());
    }
    
    config.token_status_callback = tokenStatusCallback; 
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
  }

  // --- OTA LOCAL SERVER ---
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    String html = "<html><body><h2>SPECTRA MPPT OTA</h2><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update' required><br><br><input type='submit' value='Update Firmware'></form></body></html>";
    server.send(200, "text/html", html);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "UPDATE GAGAL!" : "SUKSES! Rebooting...");
    delay(1000); ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) { if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial); } 
    else if (upload.status == UPLOAD_FILE_WRITE) { if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial); } 
    else if (upload.status == UPLOAD_FILE_END) { Update.end(true); }
  });
  server.begin();
}

// =====================================================================
//                             LOOP UTAMA
// =====================================================================
void loop() {
  server.handleClient(); 
  
  readInputs();
  readSensors();

  display.clearDisplay();
  if (currentState == STATE_SETUP) {
    renderSetupWizard();
  } else {
    runMPPTAlgorithm();
    sendTelemetryData(); 
    renderMainDisplay();
  }
  display.display();
}

// ======================= LOGIKA FIREBASE (CLOUD SCADA) =======================
void sendTelemetryData() {
  if (millis() - sendDataPrevMillis > POST_INTERVAL || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();
    
    if (Firebase.ready() && signupOK) {
      
      // 1. Kumpulkan Data Telemetri
      FirebaseJson telemetryJson;
      telemetryJson.set("v_plts", sol_V);
      telemetryJson.set("i_plts", sol_A);
      telemetryJson.set("batt_pct", bat_pct);
      telemetryJson.set("v_out", bat_V);
      telemetryJson.set("i_out", bat_A);
      telemetryJson.set("load_status", loadStatus);
      
      // Kirim Data Telemetri ke Firebase
      if (Firebase.RTDB.setJSON(&fbdo, "/optivolt/telemetry", &telemetryJson)) {
        Serial.println("Telemetry Updated to Firebase");
      } else {
        Serial.println(fbdo.errorReason());
      }
      
      // 2. Baca Command dari Web SCADA (Contoh: Toggle Load)
      if (Firebase.RTDB.getJSON(&fbdo, "/optivolt/commands")) {
        FirebaseJson &cmdJson = fbdo.jsonObject();
        FirebaseJsonData jsonData;
        
        // Baca status beban
        cmdJson.get(jsonData, "load_cmd");
        if (jsonData.success) {
          bool cmdFromWeb = jsonData.boolValue;
          if (loadStatus != cmdFromWeb) {
             loadStatus = cmdFromWeb;
             digitalWrite(PIN_LOAD, loadStatus ? HIGH : LOW);
             Serial.println(">>> STATUS BEBAN DIEKSEKUSI DARI FIREBASE SCADA <<<");
          }
        }
        
        // Baca notifikasi update setting
        cmdJson.get(jsonData, "settings_updated");
        if (jsonData.success && jsonData.boolValue == true) {
            // Ambil data settings baru dari database
            if (Firebase.RTDB.getJSON(&fbdo, "/optivolt/settings")) {
                FirebaseJson &setJson = fbdo.jsonObject();
                FirebaseJsonData sd;
                
                Serial.println(">>> MENERIMA SETTING PROFIL BATERAI BARU DARI CLOUD <<<");
                
                setJson.get(sd, "sol_vmax"); if(sd.success) sol_vMax = sd.intValue;
                setJson.get(sd, "sol_imax"); if(sd.success) sol_iMax = sd.intValue;
                setJson.get(sd, "bat_type"); if(sd.success) batType = sd.intValue;
                setJson.get(sd, "sys_volt"); if(sd.success) sysVoltage = sd.intValue;
                setJson.get(sd, "bat_cap"); if(sd.success) batCapacity = sd.intValue;
                
                // Simpan ke memory
                preferences.putInt("sol_vmax", sol_vMax);
                preferences.putInt("sol_imax", sol_iMax);
                preferences.putInt("bat_type", batType);
                preferences.putInt("sys_volt", sysVoltage);
                preferences.putInt("bat_cap", batCapacity);
                initBatteryProfile(); 
                
                // Matikan flag update di Firebase
                Firebase.RTDB.setBool(&fbdo, "/optivolt/commands/settings_updated", false);
            }
        }
      }
      
    }
  }
}

// ======================= BACA SENSOR =======================
void readSensors() {
  if (millis() - lastSensorRead > 500) {
    sol_V = ina_solar.getBusVoltage_V();
    sol_A = ina_solar.getCurrent_mA() / 1000.0; 
    sol_W = sol_V * sol_A;

    bat_V = ina_bat.getBusVoltage_V();
    bat_A = ina_bat.getCurrent_mA() / 1000.0;
    bat_W = bat_V * bat_A;
    
    load_W = (bat_A < 0) ? (bat_V * abs(bat_A)) : 0;
    
    float minV = 0, maxV = bulkVoltage;
    float mult = (sysVoltage == 24) ? 2.0 : 1.0;
    if (batType == 0) minV = 11.5 * mult;
    else if (batType == 1) minV = 9.0 * mult;
    else minV = 10.0 * mult;
    
    bat_pct = ((bat_V - minV) / (maxV - minV)) * 100.0;
    if (bat_pct < 0) bat_pct = 0.0; if (bat_pct > 100) bat_pct = 100.0;

    lastSensorRead = millis();
  }
}

// ======================= LOGIKA MPPT & LED KONTROL =======================
void initBatteryProfile() {
  float multiplier = (sysVoltage == 24) ? 2.0 : 1.0;
  if (batType == 0) { bulkVoltage = 14.4 * multiplier; floatVoltage = 13.8 * multiplier; } 
  else if (batType == 1) { bulkVoltage = 12.6 * multiplier; floatVoltage = 12.4 * multiplier; } 
  else if (batType == 2) { bulkVoltage = 14.4 * multiplier; floatVoltage = 13.6 * multiplier; }
}

void updateLEDs() {
  digitalWrite(LED_NIGHT, (currentStage == STAGE_NIGHT) ? HIGH : LOW);
  digitalWrite(LED_BULK, (currentStage == STAGE_BULK) ? HIGH : LOW);
  digitalWrite(LED_FLOAT, (currentStage == STAGE_FLOAT) ? HIGH : LOW);
}

void runMPPTAlgorithm() {
  if (sol_V < bat_V + 1.0) {
    currentStage = STAGE_NIGHT; currentDutyCycle = 0;
    digitalWrite(PIN_SD_EN, LOW); ledcWrite(PIN_PWM_IN, 0); 
    updateLEDs(); return;
  }
  
  digitalWrite(PIN_SD_EN, HIGH);

  if (bat_V < bulkVoltage && currentStage != STAGE_FLOAT) {
    currentStage = STAGE_BULK;
    
    static float last_V = 0;
    static float last_I = 0;
    float dV = sol_V - last_V;
    float dI = sol_A - last_I;
    float epsilon = 0.05; 
    
    if (abs(dV) < epsilon) { 
      if (dI > epsilon) currentDutyCycle -= 2;      
      else if (dI < -epsilon) currentDutyCycle += 2; 
    } else {
      float I_V = sol_A / sol_V;          
      float dI_dV = dI / dV;              
      if (abs(dI_dV + I_V) > 0.05) { 
        if (dI_dV > -I_V) currentDutyCycle -= 2; 
        else currentDutyCycle += 2;              
      }
    }
    last_V = sol_V; last_I = sol_A;
  } 
  else {
    currentStage = STAGE_FLOAT;
    if (bat_V > floatVoltage) currentDutyCycle--; 
    else if (bat_V < floatVoltage) currentDutyCycle++;
  }

  if (currentDutyCycle > 920) currentDutyCycle = 920;
  if (currentDutyCycle < 0) currentDutyCycle = 0;
  
  ledcWrite(PIN_PWM_IN, currentDutyCycle);
  updateLEDs(); 
}

// ======================= INPUT HANDLING (4 BUTTONS) =======================
void handleSetupScroll(int dir) {
  switch(setupStep) {
    case 1: sol_vMax += dir; if(sol_vMax > 50) sol_vMax = 18; if(sol_vMax < 18) sol_vMax = 50; break;
    case 2: sol_iMax += dir; if(sol_iMax > 20) sol_iMax = 1; if(sol_iMax < 1) sol_iMax = 20; break;
    case 3: batType += dir; if(batType > 2) batType = 0; if(batType < 0) batType = 2; break;
    case 4: sysVoltage += (dir * 12); if(sysVoltage > 24) sysVoltage = 12; if(sysVoltage < 12) sysVoltage = 24; break;
    case 5: batCapacity += (dir * 5); if(batCapacity > 200) batCapacity = 5; if(batCapacity < 5) batCapacity = 200; break;
    case 6: menuIndex += dir; if(menuIndex > 1) menuIndex = 0; if(menuIndex < 0) menuIndex = 1; break;
  }
}

void readInputs() {
  if (millis() - lastBtnPress > debounceDelay) {
    
    // --- TOMBOL UP ---
    if (digitalRead(BTN_UP) == LOW) {
      if (currentState == STATE_SETUP) handleSetupScroll(1);
      else if (currentState == STATE_MAIN && mainPage == 4) { menuIndex--; if(menuIndex < 0) menuIndex = 1; }
      lastBtnPress = millis();
    }
    
    // --- TOMBOL DOWN ---
    else if (digitalRead(BTN_DOWN) == LOW) {
      if (currentState == STATE_SETUP) handleSetupScroll(-1);
      else if (currentState == STATE_MAIN && mainPage == 4) { menuIndex++; if(menuIndex > 1) menuIndex = 0; }
      lastBtnPress = millis();
    }
    
    // --- TOMBOL ENTER ---
    else if (digitalRead(BTN_ENTER) == LOW) {
      if (currentState == STATE_SETUP) {
        if (setupStep == 0) { setupStep = 1; }
        else if (setupStep < 6) { setupStep++; } 
        else {
          if (menuIndex == 0) {
            preferences.putBool("setup_done", true); preferences.putInt("sol_vmax", sol_vMax);
            preferences.putInt("sol_imax", sol_iMax); preferences.putInt("bat_type", batType);
            preferences.putInt("sys_volt", sysVoltage); preferences.putInt("bat_cap", batCapacity);
            initBatteryProfile(); currentState = STATE_MAIN; mainPage = 0;
          } else { setupStep = 0; }
        }
      } 
      else if (currentState == STATE_MAIN) {
        if (mainPage == 4) { 
          if (menuIndex == 0) { currentState = STATE_SETUP; setupStep = 1; } 
          else if (menuIndex == 1) {
            display.clearDisplay(); display.setCursor(15, 30); display.print("MERESET SISTEM..."); display.display();
            preferences.clear(); delay(1500); ESP.restart(); 
          }
        }
        else if (mainPage == 2) { 
          loadStatus = !loadStatus; 
          digitalWrite(PIN_LOAD, loadStatus ? HIGH : LOW);
        }
      }
      lastBtnPress = millis();
    }
    
    // --- TOMBOL BACK / NEXT PAGE ---
    else if (digitalRead(BTN_BACK) == LOW) {
      if (currentState == STATE_SETUP) {
        if (setupStep > 1) setupStep--; 
      }
      else if (currentState == STATE_MAIN) {
        mainPage = (mainPage + 1) % 5; 
      }
      lastBtnPress = millis();
    }
  }
}

// ======================= UI RENDERING (100% BUTTON UI) =======================
void renderSetupWizard() {
  display.setTextSize(1); display.setCursor(0, 0);
  
  if (setupStep == 0) { 
    display.println("== SETUP WIZARD =="); 
    display.setCursor(30, 30); 
    display.println("Tekan ENTER"); 
  } 
  else if (setupStep == 1) { display.println("1. SOLAR V_MAX"); display.setTextSize(2); display.setCursor(40, 25); display.print(sol_vMax); display.print("V"); }
  else if (setupStep == 2) { display.println("2. SOLAR I_MAX"); display.setTextSize(2); display.setCursor(40, 25); display.print(sol_iMax); display.print("A"); }
  else if (setupStep == 3) { display.println("3. TIPE BATERAI"); display.setTextSize(2); display.setCursor(0, 25); if (batType == 0) display.print("SLA/Lead"); else if (batType == 1) display.print("Li-ion"); else display.print("LiFePO4"); }
  else if (setupStep == 4) { display.println("4. VOLTASE SISTEM"); display.setTextSize(2); display.setCursor(40, 25); display.print(sysVoltage); display.print("V"); }
  else if (setupStep == 5) { display.println("5. KAPASITAS BAT (Ah)"); display.setTextSize(2); display.setCursor(35, 25); display.print(batCapacity); display.print("Ah"); }
  else if (setupStep == 6) { 
    display.println("SIMPAN PENGATURAN?"); 
    display.setCursor(20, 30); 
    if (menuIndex == 0) display.print("> [YES]    [NO]"); 
    else display.print("  [YES]  > [NO]"); 
  }
}

void renderMainDisplay() {
  display.setTextSize(1);
  if (mainPage == 0) {
    display.setCursor(0, 0); display.print("SOLAR"); display.setCursor(80, 0); display.print("BAT("); display.print((int)bat_pct); display.print("%)");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(0, 15); display.print(sol_V, 1); display.print(" V"); display.setCursor(0, 25); display.print(sol_A, 1); display.print(" A"); display.setCursor(0, 35); display.print(sol_W, 0); display.print(" W");
    display.setCursor(80, 15); display.print(bat_V, 1); display.print(" V"); display.setCursor(80, 25); display.print(bat_A, 1); display.print(" A");
    display.drawLine(0, 48, 128, 48, SSD1306_WHITE);
    display.setCursor(0, 52); display.print("Stat: ");
    if (currentStage == STAGE_NIGHT) display.print("NIGHT/SLEEP"); else if (currentStage == STAGE_BULK) display.print("INC BULK"); else display.print("FLOAT CHARGE");
  } 
  else if (mainPage == 1) {
    display.setCursor(0, 0); display.print("== STATISTIK =="); display.setCursor(0, 15); display.print("Target V: "); display.print(bulkVoltage, 1); display.print(" V");
    display.setCursor(0, 25); display.print("W Maks  : "); display.print(sol_W, 0); display.print(" W");
    display.setCursor(0, 35); display.print("WiFi IP : "); if(WiFi.status() == WL_CONNECTED) display.print(WiFi.localIP().toString()); else display.print("Offline");
  } 
  else if (mainPage == 2) {
    display.setCursor(0, 0); display.print("== LOAD CONTROL =="); display.setCursor(0, 20); display.print("Beban (ENTER): ");
    if (loadStatus) display.print("ON"); else display.print("OFF");
    display.setCursor(0, 35); display.print("Daya Beban : "); display.print(load_W, 1); display.print(" W");
  } 
  else if (mainPage == 3) {
    display.setCursor(0, 0); display.print("== INFO KONFIG ==");
    display.setCursor(0, 15); display.print("Solar: "); display.print(sol_vMax); display.print("V / "); display.print(sol_iMax); display.print("A");
    display.setCursor(0, 25); display.print("Tipe : "); 
    if (batType == 0) display.print("SLA"); else if (batType == 1) display.print("Li-ion"); else display.print("LiFePO4");
    display.print(" "); display.print(sysVoltage); display.print("V");
    display.setCursor(0, 35); display.print("Kapasitas: "); display.print(batCapacity); display.print(" Ah");
  }
  else if (mainPage == 4) {
    display.setCursor(0, 0); display.print("== SETTINGS ==");
    if (menuIndex == 0) { display.setCursor(0, 15); display.print("> Edit Konfigurasi"); display.setCursor(0, 25); display.print("  Factory Reset"); }
    else { display.setCursor(0, 15); display.print("  Edit Konfigurasi"); display.setCursor(0, 25); display.print("> Factory Reset"); }
  }
}