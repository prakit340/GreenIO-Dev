/*

   #################An example to connect thingcontro.io MQTT over TLS1.2##################

*/
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <MCP23S17.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESP32Ping.h>

#define WIFI_AP ""
#define WIFI_PASSWORD ""
#define ledPIN        0    // LED status pin
#define btPIN         0    // Boot button pin

const char *hostname = "Water_Control";
const char* ping_host = "www.google.com";
#define pingPeriod   60
#define pingCount    5

String deviceToken = "mBqFnwLw6sLUsg3lIv3M";  //Sripratum@thingcontrio.io
char thingsboardServer[] = "mqtt.thingcontrol.io";

String inputString = "";
char json[200];

//static const char *fingerprint PROGMEM = "69 E5 FE 17 2A 13 9C 7C 98 94 CA E0 B0 A6 CB 68 66 6C CB 77"; // need to update every 3 months
unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long startTeleMillis;
unsigned long starSendTeletMillis;
unsigned long currentMillis;
const unsigned long periodCallBack = 1000;  //the value is a number of milliseconds
const unsigned long periodSendTelemetry = 10000;  //the value is a number of milliseconds
unsigned long startPingMillis;
unsigned int CountPing = 0;

MCP mcp(0, 5);  // MCP23S17 SS Pin is GPIO 5 and Address is 0
WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);
Ticker ticker;
WiFiManager wifiManager;

int status = WL_IDLE_STATUS;
String downlink = "";
char *bString;
int PORT = 8883;

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
  uint32_t ip;
} sett;

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

  mcp.begin();

  for (int i = 1; i <= 10; i++)
  {
    mcp.pinMode(i, LOW); // LOW for OUTPUT and HIGH for INPUT
  }
  for (int i = 1; i <= 10; i++)
  {
    mcp.digitalWrite(i, HIGH); // LOW for On and HIGH for Off
  }

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

  wifiManager.addParameter( &blnk_Text );
  wifiManager.addParameter( &thing_Token );

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

    Serial.print("Access Token: ");
    Serial.println(sett.thingToken);

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
  //startMillis = millis();  //initial start time
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

  if (startPingMillis > currentMillis)
  {
    startPingMillis = 0;
  }

  //check call back to connect Switch
  /*if (currentMillis - startMillis >= periodCallBack)  //test whether the period has elapsed
    {
    processCalled();
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
    }*/

  //send
  /*if (currentMillis - starSendTeletMillis >= periodSendTelemetry)  //test whether the period has elapsed
    {
    sendtelemetry();
    starSendTeletMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
    }*/

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

/*
  void processCalled()
  {
  Serial.println("OK");
  Serial.print("+:");
  Serial.println(downlink);
  }*/

/*
  void processAtt(char jsonAtt[])
  {

  char *aString = jsonAtt;

  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/attributes , "));
  Serial.println(aString);

  client.publish( "v1/devices/me/attributes", aString);
  }
*/

/*
  void processTele(char jsonTele[])
  {

  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/ , "));
  Serial.println(aString);
  client.publish( "v1/devices/me/telemetry", aString);
  }*/

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
  }
*/

/*
  void unrecognized(const char *command)
  {
  Serial.println("ERROR");
  }*/

void callback(char* topic, byte* payload, unsigned int length)
{
  int OutID = 0;

  inputString = "";

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    inputString += (char)payload[i];
  }
  Serial.println();

  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, inputString);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  JsonObject root = doc.as<JsonObject>();
  String Method = root["method"];
  boolean params = root["params"];

  Serial.print("Method: ");
  Serial.print(Method);
  Serial.print("\tParams: \t");
  Serial.println(params);

  OutID = Method.substring(Method.length() - 1, Method.length()).toInt();
  Serial.println(OutID);


  String aString = "{\"v";
  aString.concat(OutID);
  aString.concat("\":");

  if (params == true)
  {
    mcp.digitalWrite(OutID, LOW);
    Serial.println("on");
    aString.concat("1");
  } else if (params == false)
  {
    mcp.digitalWrite(OutID, HIGH);
    Serial.println("off");
    aString.concat("0");
  }

  aString.concat("}");
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/telemetry , "));
  Serial.println(aString);

  client.publish( "v1/devices/me/telemetry", aString.c_str());

  /*char json[length + 1];
    strncpy (json, (char*)payload, length);
    json[length] = '\0';
    Serial.println(json);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& data = jsonBuffer.parseObject((char*)json);

    String methodName = String((const char*)data["method"]);
    Serial.println(methodName);

    if (methodName.startsWith("setV"))
    {
    char json[length + 1];
    strncpy (json, (char*)payload, length);
    json[length] = '\0';

    downlink = json;
    String param = String((const char*)data["params"]);
    String meth =  String((const char*)data["method"]);
    Serial.print("param:"); Serial.println(param);

    Serial.println(meth);

    int relayId = meth.substring(meth.length() - 1, meth.length()).toInt();

    Serial.println(relayId);
    String aString = "{\"v";

    aString.concat(relayId);
    aString.concat("\":");
    if (param.equals("true") ) {

      mcp.digitalWrite(relayId, HIGH);

      Serial.println("on");
      aString.concat("1");
    } else if (param.equals("false")) {

      mcp.digitalWrite(relayId, LOW);
      Serial.println("off");
      aString.concat("0");
      delay(2000);
      digitalWrite(4, HIGH);

    }
    aString.concat("}");
    Serial.println("OK");
    Serial.print(F("+:topic v1/devices/me/telemetry , "));
    Serial.println(aString);

    client.publish( "v1/devices/me/telemetry", aString.c_str());

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


/*
  void sendtelemetry()
  {
  String json = "";
  json.concat("{\"temp\":");
  json.concat("33");
  json.concat(",\"hum\":");
  json.concat("33");
  json.concat("}");
  Serial.println(json);

  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);


  processTele(char_array);
  //}
  }*/
