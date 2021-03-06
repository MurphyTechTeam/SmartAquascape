#include <AntaresESP32HTTP.h>

#define ACCESSKEY "9fb52249c593c66d:b719370dce7d93b9"       // Ganti dengan access key akun Antares anda
#define WIFISSID "Ayangto"         // Ganti dengan SSID WiFi anda
#define PASSWORD "ywsh5677"    // Ganti dengan password WiFi anda

#define applicationName "smartAquascape"   // Ganti dengan application name Antares yang telah dibuat
#define deviceName "Aquascape-001"     // Pantau data monitoring
#define TimeMonitoring "TimeMonitoring" //Rentang Waktu Pertukaran Data Monitoring
#define stsFeeder "Aquascape-Feeder"
#define manFeeder "Manual-Feeder"
#define auFeeder "Auto-Feeder"

AntaresESP32HTTP antares(ACCESSKEY);    // Buat objek antares API

String data;
struct tm timeinfo;
struct timeval tv;
char buf[64], timeLogic[64];
int sts_FeederControl = 2; //default Manual Control
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;
unsigned long sts_time = 1;
unsigned long previousTimeMonitoring = 0;

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  antares.setDebug(true);
  antares.wifiConnection(WIFISSID,PASSWORD);  // Mencoba untuk menyambungkan ke WiFi
  
  //inisialisasi variabel
  Serial.println("getting the bloody time, one moment");
  while (!getLocalTime(&timeinfo)) {
      Serial.print(".");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      delay(500);
  }
}

String receiveData() {
  data = "";
  while(Serial2.available() > 0){
    delay(100);
    char d = Serial2.read();
    data += d;
  }
  return data;
}

int getTimeNow() {
  char fmt[64];
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return 0;
  }
  strftime(fmt,sizeof(fmt), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  snprintf(buf, sizeof(buf), fmt, tv.tv_usec);
  return 1;
}

int getTimeLogical() {
  char fmt[64];
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return 0;
  }
  strftime(fmt,sizeof(fmt), "%H%M", &timeinfo);
  snprintf(timeLogic, sizeof(timeLogic), fmt, tv.tv_usec);
  return 1;
} 

void sendAntares(String data, int id) {
  getTimeNow();
  Serial.println(buf);
  Serial.println("Kirim Data Ke Antares...");
  if(id == 1) {
    antares.add("header", 1);
    antares.add("data", data.c_str());
    antares.add("WaktuReq", buf);
  }
  
  antares.send(applicationName, deviceName);
  data = "";
}

void dataMonitoring() { //get data monitoring Arduino
  Serial.print("Get data dari Arduino");
  Serial2.print("monitoring");
  receiveData();
  delay(50);
  if (data.length() > 0){
    Serial.println(data);
    sendAntares(data, 1);
  } else {
    Serial.println("Tidak dapat data dari Arduino");
  }
}

void getTimeMonitoring() {
  //Lakukan request Data Time Monitoring di db
  //Tangkap Respon Request Data Monitoring
  antares.get(applicationName, TimeMonitoring);
  if(antares.getInt("header") == 4) { //header 4 trigger by sensor
    sts_time = antares.getInt("waktuPengiriman"); //Dalam menit  
  }
}

void MonitoringSync() {
  unsigned long Mon_Sync = millis();
  unsigned long TimeSync = sts_time * 60000; //Konversi menit to milisecond
  Serial.println(Mon_Sync);
  if ((unsigned long)(Mon_Sync - previousTimeMonitoring) >= TimeSync) {
    Serial.println("Data Monitoring Sync");
    dataMonitoring();
    Serial.println("Monitoring Sync Time: " + String(sts_time));
    getTimeMonitoring();
    previousTimeMonitoring = Mon_Sync;
  }
}

void statusFeeder() { //status feeder
  //Tangkap status Feeder
  antares.get(applicationName, stsFeeder);
  if(antares.getInt("header") == 7) {
    sts_FeederControl = antares.getInt("statusControl");
  }
}

void runFeeder() { //Jalankan feeder di arduino nya.
  Serial2.print("feeder");
}

void manualFeeder() {
  int manual = 0; //Default Feeder Mati
  //Tangkap Respon Request feeder aktif atau tidak
  antares.get(applicationName, manFeeder);
  if(antares.getInt("header") == 9) { 
    manual = antares.getInt("statusFeeder");  
  }

  if(manual == 1) { //Jika button ditekan, Feeder Aktif
    runFeeder(); //Jalankan feeder
    antares.add("header", 9); //Rubah Status Feeder di database to disable
    antares.add("statusFeeder", 0);
    antares.send(applicationName, manFeeder);
  }
}

void autoFeeder() {
  getTimeLogical();
  String waktuPertama = "0700", waktuKedua = "1400", waktuKetiga = "2100"; //Waktu pengiriman pertama dan kedua
  antares.get(applicationName, auFeeder); //Tangkap status Feeder
  if(antares.getInt("header") == 12) {
    waktuPertama = antares.getString("waktuPertama");
    waktuKedua = antares.getString("waktuKedua");
    waktuKetiga = antares.getString("waktuKetiga");
  }
  //bandingkan data waktu dengan time sekarang
  String waktuSekarang(timeLogic);
  Serial.println(waktuSekarang);
  if(waktuSekarang.equals(waktuPertama)) { //Jika waktu sama makan jalankan feeder
    runFeeder();
  } else if(waktuSekarang.equals(waktuKedua)) {
    runFeeder();
  } else if(waktuSekarang.equals(waktuKetiga)) {
    runFeeder();
  }
}

void loop() {
  MonitoringSync();
  statusFeeder();
  if(sts_FeederControl == 1) { //Status aktif untuk Auto controlling
    Serial.println("Auto Feeder");
    autoFeeder();
  } else if(sts_FeederControl == 2) { //Status aktif untuk Manual controlling
    manualFeeder();
    Serial.println("Manual Feeder");
  }
}