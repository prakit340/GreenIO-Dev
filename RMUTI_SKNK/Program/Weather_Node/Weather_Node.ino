/*
   Connect to USB connector (4 wire cable)：
  Black wire： GND
  Red wire： USB A  (D+)
  Yellow wire： USB B (D-)
  Green wire： VDD (4.5V -6V)
*/

#include <WiFiClientSecure.h>

#include <HardwareSerial.h>
const long interval = 16000;  //millisecond
unsigned long previousMillis = 0;
HardwareSerial mySerial(1);
uint8_t cc;
String hexString = "";



struct misoWeather {
  uint8_t idType;
  uint8_t securityCode;
  float winDir, temp;
  float hum, winSpeed, gustSpeed;
  float accRainfall, uv, light, baroPress;
  String  CRC;
  String checksum;
  float pm2_5;
};

struct misoWeather dataMiso;
HardwareSerial hwSerial(2);
#define SERIAL1_RXPIN 26
#define SERIAL1_TXPIN 25
unsigned long currentMillis;


void setup() {
  
  Serial.begin(115200);


  Serial.println("Connecting..");
  pinMode(15, OUTPUT); digitalWrite(15, HIGH);

  hwSerial.begin(9600, SERIAL_8N1, SERIAL1_RXPIN, SERIAL1_TXPIN);
  mySerial.begin(9600, SERIAL_8N1, 16, 17);
  previousMillis = millis();
  Serial.println("Start");


}


uint32_t timer;
void loop() {
  if ( millis() - timer > 5000) {     // ทำการอ่านค่าและส่งทุกๆ 60 วินาที
    timer = millis();
    hexString = "";
    Serial.println("recv (HEX): ");
    while (mySerial.available())
    {
      cc = mySerial.read();

      if (cc < 0x10) {
        Serial.print("0");
        hexString +=  String(cc, HEX);
      }
      Serial.print(cc, HEX);
      hexString +=  String(cc, HEX);
    }
    Serial.println("");
    parseHex(hexString);

    previousMillis = currentMillis;

    //
    Serial.println(String(dataMiso.winDir));
    //    TD32_Set_Firebase("Set/temp", String(dataMiso.temp));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/hum", String(dataMiso.hum));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/winSpeed", String(dataMiso.winSpeed));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/gustSpeed", String(dataMiso.gustSpeed));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/gustSpeed", String(dataMiso.gustSpeed));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/accRainfall", String(dataMiso.accRainfall));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/uv", String(dataMiso.uv));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/light", String(dataMiso.light));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/baroPress", String(dataMiso.baroPress));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/pm2.5", String(dataPMS.pm25_env));// ตั้งค่า  Firebase
    //    TD32_Set_Firebase("Set/pm10", String(dataPMS.pm100_env));// ตั้งค่า  Firebase
    //


  }
}


long unsigned int hex2Dec(String hex) {
  String tobeDec = "0x" + hex;
  char str[16];
  tobeDec.toCharArray(str, 16);

  long unsigned int val = strtol(str, NULL, 16);
  //  Serial.println(val);
  return val;
}

int parseWindDir(String hexWind) {

  String hexString = "";
  hexString.concat("0");  hexString.concat(hexWind.charAt(0));
  hexString.concat("0");  hexString.concat(hexWind.charAt(1));
  hexString.concat("0");  hexString.concat(hexWind.charAt(2));
  Serial.println(hexString);
  int number = (int) strtol( &hexString[0], NULL, 16);  //separate BINARAY bit
  // Split them up into r, g, b values
  int r = number >> 16;
  int g = number >> 8 & 0xFF;
  int b = number & 0xFF;
  String st = String(r, BIN);
  String nd = String(g, BIN);
  String rd = String(b, BIN);
  // padding zero
  st = pad0Left(st, "0");
  nd = pad0Left(nd, "0");
  rd = pad0Left(rd, "0");
  char st_array[5];
  char nd_array[5];
  char rd_array[5];
  st.toCharArray(st_array, 5);
  nd.toCharArray(nd_array, 5);
  rd.toCharArray(rd_array, 5);
  char windChars[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //+1
  // follow by the Weather Station  Misol manual 9bits
  windChars[0] = rd_array[0];
  windChars[1] = st_array[0];
  windChars[2] = st_array[1];
  windChars[3] = st_array[2];
  windChars[4] = st_array[3];
  windChars[5] = nd_array[0];
  windChars[6] = nd_array[1];
  windChars[7] = nd_array[2];
  windChars[8] = nd_array[3];

  Serial.print(rd_array[0]);
  Serial.print(st_array[0]); Serial.print(st_array[1]); Serial.print(st_array[2]); Serial.print(st_array[3]);
  Serial.print(nd_array[0]); Serial.print(nd_array[1]); Serial.print(nd_array[2]); Serial.println(nd_array[3]);

  int windDirDec = strtol(windChars, (char**) NULL, 2);

  Serial.println(windDirDec);
  return windDirDec;
}

void parseHex (String hex) {
  String idType = hex.substring(0, 2);
  String securityCode = hex.substring(2, 4);
  String _winDir  = hex.substring(4, 7);
  String _temp  = hex.substring(7, 10);
  String _hum = hex.substring(10, 12);
  String _winSpeed = hex.substring(12, 14);
  String _gustSpeed = hex.substring(14, 16);
  String _accRainfall = hex.substring(16, 20);
  String _uv = hex.substring(20, 24);
  String _light = hex.substring(24, 30);
  String _baroPress = hex.substring(35, 40);
  dataMiso.winDir = parseWindDir(_winDir);
  dataMiso.temp =  (hex2Dec(_temp) - 400)  / 10.0;
  dataMiso.hum = hex2Dec(_hum);
  dataMiso.winSpeed = (hex2Dec(_winSpeed) / 8) * 1.12;
  dataMiso.gustSpeed = hex2Dec(_gustSpeed);
  dataMiso.accRainfall = hex2Dec(_accRainfall);
  dataMiso.uv = hex2Dec(_uv);
  dataMiso.light = hex2Dec(_light) / 10.0;
  dataMiso.baroPress = hex2Dec(_baroPress) / 100.0;
  Serial.println("Debug:");
  //  Serial.print("  idType:");  Serial.println(dataMiso.idType);
  //  Serial.print("  securityCode:");  Serial.println(dataMiso.securityCode);
  Serial.print("  winDir:"); Serial.println(dataMiso.winDir);
  Serial.print("  temp:");  Serial.println(dataMiso.temp);
  Serial.print("  hum:");   Serial.println(dataMiso.hum);
  Serial.print("  winSpeed:");   Serial.println(dataMiso.winSpeed);
  Serial.print("  gustSpeed:");   Serial.println(dataMiso.gustSpeed);
  Serial.print("  accRainfall:");   Serial.println(dataMiso.accRainfall);
  Serial.print("  uv:");   Serial.println(dataMiso.uv);
  Serial.print("  light:");   Serial.println(dataMiso.light);
  Serial.print("  baroPress:");   Serial.println(dataMiso.baroPress);


  Serial.println("end");



}
String pad0Left(String str, String padStr) {

  while (str.length() < 4) {
    str = padStr + str;
  }
  return str;
}
