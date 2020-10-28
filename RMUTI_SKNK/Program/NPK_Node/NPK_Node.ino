/*
  An example to connect thingcontro.io MQTT over TLS1.2
  With WiFiManager setting Access token and period time send
*/

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESP32Ping.h>

// Modbus
#include <ModbusMaster.h>
#include "REG_CONFIG.h"
#include <HardwareSerial.h>

HardwareSerial rs485(2);

const char *hostname = "Soil_Node";
const char* ping_host = "www.google.com";
#define pingPeriod   5
#define pingCount    5

#define WIFI_AP ""
#define WIFI_PASSWORD ""
#define SoilPIN       33   // Soil Moisture PIN
#define ledPIN        0    // LED status pin
#define btPIN         0    // Boot button pin

String deviceToken = "50dfVj4aG5sZRHUVNWsx";
char thingsboardServer[] = "mqtt.thingcontrol.io";
int PORT = 8883;

String json = "";

//static const char *fingerprint PROGMEM = "69 E5 FE 17 2A 13 9C 7C 98 94 CA E0 B0 A6 CB 68 66 6C CB 77"; // need to update every 3 months
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long startTeleMillis;
unsigned long starSendTeletMillis;
unsigned long currentMillis;
const unsigned long periodCallBack = 1000; //the value is a number of milliseconds
const unsigned long periodSendTelemetry = 60000;  //the value is a number of milliseconds
unsigned long startPingMillis;
unsigned int CountPing = 0;

Ticker ticker;
WiFiManager wifiManager;
ModbusMaster node;
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
String downlink = "";
char *bString;

void tick()
{
  //toggle state
  digitalWrite(ledPIN, !digitalRead(ledPIN));     // set pin to the opposite state
}

void configModeCallback (WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

class IPAddressParameter : public WiFiManagerParameter
{
  public:
    IPAddressParameter(const char *id, const char *placeholder, IPAddress address)
      : WiFiManagerParameter("")
    {
      init(id, placeholder, address.toString().c_str(), 16, "", WFM_LABEL_BEFORE);
    }

    bool getValue(IPAddress &ip)
    {
      return ip.fromString(WiFiManagerParameter::getValue());
    }
};

class IntParameter : public WiFiManagerParameter
{
  public:
    IntParameter(const char *id, const char *placeholder, long value, const uint8_t length = 10)
      : WiFiManagerParameter("")
    {
      init(id, placeholder, String(value).c_str(), length, "", WFM_LABEL_BEFORE);
    }

    long getValue()
    {
      return String(WiFiManagerParameter::getValue()).toInt();
    }
};

//Setting WiFi
struct Settings
{
  char thingToken[40] = "";
  int periodTime = 30;
  uint32_t ip;
} sett;

// Modbus
struct Meter
{
  String Moisture;
  String N;
  String P;
  String K;
  String pH;
};

Meter Soil[10] ;

void checkButton()
{
  pinMode(btPIN, INPUT);
  // check for button press
  if ( digitalRead(btPIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if ( digitalRead(btPIN) == LOW ) {
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if ( digitalRead(btPIN) == LOW ) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        pinMode(ledPIN, OUTPUT);   digitalWrite(ledPIN, HIGH);
        for (int a = 0; a < 10; a++)
        {
          digitalWrite(ledPIN, LOW);
          delay(80);
          digitalWrite(ledPIN, HIGH);
          delay(80);
        }
        wifiManager.resetSettings();
        ESP.restart();
      }

      // start portal w delay
      Serial.println("Starting config portal");
      wifiManager.setConfigPortalTimeout(120);

      if (!wifiManager.startConfigPortal(hostname)) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
  pinMode(ledPIN, OUTPUT);   digitalWrite(ledPIN, HIGH);
}

void setup()
{
  pinMode(ledPIN, OUTPUT);   digitalWrite(ledPIN, HIGH);

  Serial.begin(115200);

  rs485.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println(F("Starting... Soil Moisture NPK pH Monitor"));
  Serial.println();
  Serial.println(F("***********************************"));

  ticker.attach(1.1, tick);

  EEPROM.begin( 512 );
  EEPROM.get(0, sett);

  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);
  std::vector<const char *> menu = {"wifi", "info", "sep", "restart", "exit"};
  wifiManager.setMenu(menu);
  wifiManager.setClass("invert");
  wifiManager.setConfigPortalTimeout(180); // auto close configportal after n seconds
  //wm.setAPClientCheck(true); // avoid timeout if client connected to softap
  //wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

  WiFiManagerParameter blnk_Text("<b>Thingcontrol.io Setup.</b> <br>");
  sett.thingToken[39] = '\0';   //add null terminator at the end cause overflow
  WiFiManagerParameter thing_Token( "thingtoken", "Access Token",  sett.thingToken, 40);
  IntParameter period_Send( "periodsend", "Period Send(Sec.)",  sett.periodTime);

  wifiManager.addParameter( &blnk_Text );
  wifiManager.addParameter( &thing_Token );
  wifiManager.addParameter( &period_Send );

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  res = wifiManager.autoConnect(hostname); // anonymous ap
  // res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if (!res)
  {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
  }

  if (shouldSaveConfig)
  {
    strncpy(sett.thingToken, thing_Token.getValue(), 40);
    sett.thingToken[39] = '\0';
    sett.periodTime = period_Send.getValue();

    Serial.print("Access Token: ");
    Serial.println(sett.thingToken);
    Serial.print("Period Send : ");
    Serial.println(sett.periodTime, DEC);

    EEPROM.put(0, sett);
    if (EEPROM.commit())
    {
      Serial.println("Settings saved");
    } else
    {
      Serial.println("EEPROM error");
    }
  }

  ticker.detach();
  digitalWrite(ledPIN, HIGH);
  Serial.println("connected...yeey :)");

  //  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, PORT );
  client.setCallback(callback);
  reconnectMqtt();

  Serial.print("Start..");
  startMillis = millis() / 1000; //initial start time
  starSendTeletMillis = millis() / 1000;
  startPingMillis = millis() / 1000;
}

void loop()
{
  checkButton();

  status = WiFi.status();
  if ( status == WL_CONNECTED)
  {
    if ( !client.connected() )
    {
      reconnectMqtt();
    }
    client.loop();
  }

  currentMillis = millis() / 1000; //get the current "time" (actually the number of milliseconds since the program started)

  // Overflow timer check
  if (startMillis > currentMillis)
  {
    startMillis = 0;
  }
  if (starSendTeletMillis > currentMillis)
  {
    starSendTeletMillis = 0;
  }
  if (startPingMillis > currentMillis)
  {
    startPingMillis = 0;
  }

  //check call back to connect Switch
  if (currentMillis - startMillis >= periodCallBack ) //test whether the period has elapsed
  {
    processCalled();
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }

  //send telemetry
  if (currentMillis - starSendTeletMillis >= sett.periodTime)  //test whether the period has elapsed
  {
    readMeter();
    sendtelemetry();
    starSendTeletMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
    pinMode(ledPIN, OUTPUT);
    digitalWrite(ledPIN, LOW);
    delay(80);
    digitalWrite(ledPIN, HIGH);
  }

  //ping check internet
  if (currentMillis - startPingMillis >= pingPeriod)  //test whether the period has elapsed
  {
    if (Ping.ping(ping_host)) {
      Serial.println("Success!!");
      CountPing = 0;
    } else {
      Serial.println("Error :(");
      CountPing++;
    }

    if (CountPing >= pingCount)
    {
      CountPing = 0;
      ESP.restart();
    }
    startPingMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
}


/*
  void getMac()
  {
  Serial.println("OK");
  Serial.print("+deviceToken: ");
  Serial.println(WiFi.macAddress());
  }*/

/*
  void viewActive()
  {
  Serial.println("OK");
  Serial.print("+:WiFi, ");
  Serial.println(WiFi.status());
  if (client.state() == 0)
  {
    Serial.print("+:MQTT, [CONNECT] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
  else
  {
    Serial.print("+:MQTT, [FAILED] [rc = " );
    Serial.print( client.state() );
    Serial.println( " : retrying]" );
  }
  }*/

/*
  void setWiFi()
  {
  Serial.println("OK");
  Serial.println("+:Reconfig WiFi  Restart...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  if (!wifiManager.startConfigPortal("ThingControlCommand"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //    ESP.reset();
    delay(5000);
  }

  Serial.println("OK");
  Serial.println("+:WiFi Connect");

  //  wifiClient.setFingerprint(fingerprint);
  client.setServer( thingsboardServer, PORT );  // secure port over TLS 1.2
  client.setCallback(callback);
  reconnectMqtt();
  }*/

void processCalled()
{
  Serial.println("OK");
  Serial.print("+:");
  Serial.println(downlink);
}

/*
  void processAtt(char jsonAtt[])
  {

  char *aString = jsonAtt;

  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/attributes , "));
  Serial.println(aString);

  client.publish( "v1/devices/me/attributes", aString);
  }*/

void processTele(char jsonTele[])
{

  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/telemetry , "));
  Serial.println(aString);
  client.publish( "v1/devices/me/telemetry", aString);
}

/*
  void processToken()
  {
  char *aString;

  //  aString = cmdHdl.readStringArg();

  Serial.println("OK");
  Serial.print("+:deviceToken , ");
  Serial.println(aString);
  deviceToken = aString;

  reconnectMqtt();
  }*/

/*
  void unrecognized(const char *command)
  {
  Serial.println("ERROR");
  }*/

void callback(char* topic, byte* payload, unsigned int length)
{

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';
  /*
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& data = jsonBuffer.parseObject((char*)json);

    String methodName = String((const char*)data["method"]);
    Serial.println(methodName);

    if (methodName.startsWith("setValue"))
    {
      char json[length + 1];
      strncpy (json, (char*)payload, length);
      json[length] = '\0';

      downlink = json;
    }
    else
    {
      downlink = "";
    }*/
}

void reconnectMqtt()
{
  String token = "";
  token = sett.thingToken;

  if ( client.connect("Thingcontrol_AT", token.c_str(), NULL) )
  {
    Serial.println( F("Connect MQTT Success."));
    client.subscribe("v1/devices/me/rpc/request/+");
  }

}

void readMeter()
{
  unsigned int RAWadc = 0;
  int RAWSoil = 0;
  float Moisture = 0;

  for (int a = 0; a < 20; a++)
  {
    RAWadc += analogRead(SoilPIN);
    delay(10);
  }
  RAWSoil = RAWadc / 20;
  Moisture = mapFloat(RAWSoil, 0.0, 4095.0, 99.0, 0.0);

  for (int i = 0; i < Total_of_Reg_NPK ; i++)
  {
    DATA_METER_NPK[i] = read_Modbus(NPK_ID_meter, Reg_addr_NPK[i]);
  }
  delay(100);

  for (int i = 0; i < Total_of_Reg_pH ; i++)
  {
    DATA_METER_pH[i] = read_Modbus(pH_ID_meter, Reg_addr_pH[i]);
  }
  delay(100);

  Soil[0].Moisture = Moisture;
  Soil[0].N = DATA_METER_NPK[0];
  Soil[0].P = DATA_METER_NPK[1];
  Soil[0].K = DATA_METER_NPK[2];
  Soil[0].pH = DATA_METER_pH[0] / 100.0;
}

long read_Modbus(char addr, uint16_t  REG)
{
  uint8_t j, result;
  uint16_t data;

  node.begin(addr, rs485);

  result = node.readHoldingRegisters(REG, 1);

  if (result == node.ku8MBSuccess)
  {
    data = node.getResponseBuffer(0);

    //Serial.println("Connec modbus Ok.");
    return data;
  } else
  {
    Serial.print("Connec modbus ID: ");
    Serial.print(addr);
    Serial.print(" Sensor fail. REG >>> ");
    Serial.println(REG); // Debug
    delay(100);
    return 0;
  }
}

void sendtelemetry()
{
  String json = "";
  json.concat("{\"moisture\":");
  json.concat(Soil[0].Moisture);
  json.concat(",\"n\":");
  json.concat(Soil[0].N);
  json.concat(",\"p\":");
  json.concat(Soil[0].P);
  json.concat(",\"k\":");
  json.concat(Soil[0].K);
  json.concat(",\"ph\":");
  json.concat(Soil[0].pH);
  json.concat("}");
  Serial.println(json);

  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);


  processTele(char_array);
}

double mapFloat(double val, double in_min, double in_max, double out_min, double out_max) {
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
