#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/* WIFI */
const char* ssid = "name of wifi";
const char* password = "name of password";

/* GOOGLE SCRIPT */
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbwzJxYPVSVdOtaNOKJlWPKMYiVDJQ6FIXg83f94FcT3Iop8wpK_H0W5k1caM7YC__ZhwA/exec";
WebServer server(80);

/* PINS */
#define DUST_PIN 34
#define DUST_LED 26
#define MQ7_PIN 35
#define MQ135_PIN 32

/* GLOBAL */
float currentPM25 = 0;
float currentCO = 0;
float currentNO2 = 0;
float currentAQI = 0;
String currentStatus = "Initializing...";

unsigned long prevSensor = 0;
unsigned long prevAlert = 0;

const long sensorInterval = 3000;
const long alertInterval = 10000; //10 sec test

/* DASHBOARD */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:Segoe UI;background:#f5f7fa;text-align:center;padding:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:20px;max-width:900px;margin:auto}
.card{background:white;padding:20px;border-radius:12px;box-shadow:0 3px 8px rgba(0,0,0,0.08)}
.big{font-size:42px;font-weight:bold}
.aqi{grid-column:1/-1;background:#f1c40f}
</style>
</head>
<body>

<h1>ESP32 Air Quality Monitor</h1>

<div class="grid">
<div class="card aqi">
<div>PREDICTED AQI</div>
<div class="big" id="aqi">--</div>
<div id="status">--</div>
</div>

<div class="card">
<div>PM2.5</div>
<div class="big" id="pm">--</div>
</div>

<div class="card">
<div>CO</div>
<div class="big" id="co">--</div>
</div>

<div class="card">
<div>NO2</div>
<div class="big" id="no2">--</div>
</div>
</div>

<script>
async function update(){
let r = await fetch('/data');
let d = await r.json();
aqi.innerText=d.aqi.toFixed(1);
status.innerText=d.status;
pm.innerText=d.pm25.toFixed(2);
co.innerText=d.co.toFixed(2);
no2.innerText=d.no2.toFixed(2);
}
setInterval(update,3000);
update();
</script>

</body>
</html>
)rawliteral";

/* SENSOR READ */
float readDust(){
  digitalWrite(DUST_LED, LOW);
  delayMicroseconds(280);
  int raw = analogRead(DUST_PIN);
  delayMicroseconds(40);
  digitalWrite(DUST_LED, HIGH);
  return (raw * 3.3 / 4095.0) * 200.0;
}

float readMQ7(){
  return analogRead(MQ7_PIN) * 3.3 / 4095.0;
}

float readMQ135(){
  return analogRead(MQ135_PIN) * 3.3 / 4095.0;
}

/* TREES */
float tree1(float PM25,float CO,float NO2){
if(PM25<15)return 48.5;
else if(PM25<30)return 55.2;
else return 70.4;
}
float tree2(float PM25,float CO,float NO2){
if(CO<0.35)return 50.1;
else if(CO<0.6)return 62.8;
else return 82.3;
}
float tree3(float PM25,float CO,float NO2){
if(NO2<0.2)return 42.0;
else if(NO2<0.5)return 58.7;
else return 77.5;
}
float tree4(float PM25,float CO,float NO2){
if(PM25<20 && CO<0.4)return 45.5;
else return 66.3;
}
float tree5(float PM25,float CO,float NO2){
if(PM25*CO<6)return 52.1;
else return 73.9;
}

/* MATLAB MODEL */
float predict_AQI(float PM25,float CO,float NO2){

float x1=PM25;
float x2=CO;
float x3=NO2;

float x4=PM25*PM25;
float x5=CO*CO;
float x6=NO2*NO2;
float x7=PM25*CO;
float x8=PM25*NO2;
float x9=CO*NO2;

float poly=
4.1261+
1.0102*x1+
51.937*x2+
0.68965*x3+
0.012652*x4-
5.4665*x5-
0.01871*x6-
0.96006*x7+
0.0022807*x8+
0.25318*x9;

float correction=
(tree1(PM25,CO,NO2)+
tree2(PM25,CO,NO2)+
tree3(PM25,CO,NO2)+
tree4(PM25,CO,NO2)+
tree5(PM25,CO,NO2))/5.0;

return poly+correction;
}

/* EMAIL */
void triggerEmailAlert(float aqi,String status){
if(WiFi.status()!=WL_CONNECTED)return;

WiFiClientSecure client;
client.setInsecure();
HTTPClient http;

String url = String(GOOGLE_SCRIPT_URL) +
             "?aqi=" + String(aqi) +
             "&status=" + status +
             "&key=test";

Serial.println("Sending alert...");
Serial.println(url);

http.begin(client,url);
int code=http.GET();

Serial.print("HTTP Code: ");
Serial.println(code);

http.end();
}

/* SETUP */
void setup(){
Serial.begin(115200);
pinMode(DUST_LED,OUTPUT);
digitalWrite(DUST_LED,HIGH);

WiFi.begin(ssid,password);
while(WiFi.status()!=WL_CONNECTED){
delay(500);
Serial.print(".");
}

Serial.println("\nConnected");
Serial.println(WiFi.localIP());

server.on("/",[]{
server.send(200,"text/html",index_html);
});

server.on("/data",[]{
String json="{";
json+="\"pm25\":"+String(currentPM25)+",";
json+="\"co\":"+String(currentCO)+",";
json+="\"no2\":"+String(currentNO2)+",";
json+="\"aqi\":"+String(currentAQI)+",";
json+="\"status\":\""+currentStatus+"\"}";
server.send(200,"application/json",json);
});

server.begin();
}

/* LOOP */
void loop(){

server.handleClient();
unsigned long now=millis();

if(now-prevSensor>sensorInterval){
prevSensor=now;

currentPM25=readDust();
currentCO=readMQ7();
currentNO2=readMQ135();

currentAQI=predict_AQI(currentPM25,currentCO,currentNO2);

/* TEST */
/*currentAQI=250;*/

if(currentAQI<=50) currentStatus="Good";
else if(currentAQI<=100) currentStatus="Satisfactory";
else if(currentAQI<=200) currentStatus="Moderate";
else if(currentAQI<=300) currentStatus="Poor";
else currentStatus="Severe";

Serial.println("-------------");
Serial.print("PM2.5: ");Serial.println(currentPM25);
Serial.print("CO: ");Serial.println(currentCO);
Serial.print("NO2: ");Serial.println(currentNO2);
Serial.print("AQI: ");Serial.println(currentAQI);
}

/* ALERT */
if(now-prevAlert>alertInterval){
prevAlert=now;

if(currentAQI>200){
Serial.println("ALERT TRIGGERED");
triggerEmailAlert(currentAQI,currentStatus);
}
}
}
