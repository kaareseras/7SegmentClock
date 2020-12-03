#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <NTPClient.h>  //https://github.com/taranais/NTPClient
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <PubSubClient.h>


// Constants
const char *ssid = "Seras";
const char *password =  "Ivanhoe1";
const char *msg_toggle_led = "toggleLED";
const char *msg_get_led = "getLEDState";
const int dns_port = 53;
const int http_port = 80;
const int ws_port = 1337;
const int led_pin = 15;

// Globals
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(1337);
char msg_buf[100];
int led_state = 1;
int clock_state = 1;
int brightness = 255;
bool dots = true;
int r_val = 255;
int g_val = 255;
int b_val = 255;
unsigned long _time;
unsigned long next_time;

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.1.11";

WiFiClient espClient;
PubSubClient mqttClient(espClient);



// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;


//Varible for LedStrip
#define PIN 4

Adafruit_NeoPixel strip = Adafruit_NeoPixel(30, PIN, NEO_GRB + NEO_KHZ800);
long ledstrip = 0b000000000000000000000000000000;
long numbers[] = {
  0b1111110,  // [0] 0
  0b0011000,  // [1] 1
  0b1101101,  // [2] 2
  0b0111101,  // [3] 3
  0b0011011,  // [4] 4
  0b0110111,  // [5] 5
  0b1110111,  // [6] 6
  0b0011100,  // [7] 7
  0b1111111,  // [8] 8
  0b0111111,  // [9] 9
  0b0000000,  // [10] off
  0b0001111,  // [11] degrees symbol
  0b0111100,  // [12] C(elsius)
  0b0011101,  // [13] F(ahrenheit)
};

/***********************************************************
 * Functions
 */

// Callback: receiving any WebSocket message
void onWebSocketEvent(uint8_t client_num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", client_num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(client_num);
        Serial.printf("[%u] Connection from ", client_num);
        Serial.println(ip.toString());
      }
      break;

    // Handle text messages from client
    case WStype_TEXT:
      {
        StaticJsonDocument<200> docRecieved;
        StaticJsonDocument<200> docSend;
        String outputJson;

        // Print out raw message
        Serial.printf("[%u] Received text: %s\n", client_num, payload);
  
        // Get and parse Json
        deserializeJson(docRecieved, payload);
        const char* command = docRecieved["command"];
        const int value = docRecieved["value"];
        const int _r = docRecieved["r"];
        const int _g = docRecieved["g"];
        const int _b = docRecieved["b"];
        
        
        // Toggle clock_state
        if ( strcmp((char *)command, "toggleLED") == 0 ) {
          Serial.printf("Toggling clock_state to %u\n", clock_state);
          clock_state = clock_state ? 0 : 1;         
          Serial.printf("Toggling clock_state to %u\n", clock_state);
          digitalWrite(led_pin, clock_state);
          printClock();
  
        // Report clock_state
        } else if ( strcmp((char *)command, "getLEDState") == 0 ) {
          docSend["tag"] = "led_state";
          docSend["value"] = clock_state;
          serializeJson(docSend, outputJson);
          const char* charOutputJson = outputJson.c_str();
          sprintf(msg_buf, "%s", charOutputJson);
          Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
          webSocket.sendTXT(client_num, msg_buf);

        // Set brightness
        } else if ( strcmp((char *)command, "setBrightness") == 0 ) {
          brightness = value;
          Serial.printf("Setting brightness to %u\n", value);

        // Report brightness
        } else if ( strcmp((char *)command, "getBrightness") == 0 ) {
          docSend["tag"] = "brightness";
          docSend["value"] = brightness;
          serializeJson(docSend, outputJson);
          const char* charOutputJson = outputJson.c_str();
          sprintf(msg_buf, "%s", charOutputJson);
          Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
          webSocket.sendTXT(client_num, msg_buf);

        // Set color
        } else if ( strcmp((char *)command, "setColor") == 0 ) {
          r_val = _r;
          g_val = _g;
          b_val = _b;
          Serial.printf("Setting color to (RGB) %u %u %u\n", r_val,g_val,b_val);

        // Report color
        } else if ( strcmp((char *)command, "getColor") == 0 ) {
          docSend["tag"] = "color";
          docSend["r"] = r_val;
          docSend["g"] = g_val;
          docSend["b"] = b_val;
          serializeJson(docSend, outputJson);
          const char* charOutputJson = outputJson.c_str();
          sprintf(msg_buf, "%s", charOutputJson);
          Serial.printf("Sending to [%u]: %s\n", client_num, msg_buf);
          webSocket.sendTXT(client_num, msg_buf);
  
        // Message not recognized
        } else {
          Serial.println("[%u] Message not recognized");
        }
      }
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

// Callback: send homepage
void onIndexRequest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                  "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/index.html", "text/html");
}

// Callback: send style sheet
void onCSSRequest(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                  "] HTTP GET request of " + request->url());
  request->send(SPIFFS, "/style.css", "text/css");
}

// Callback: send 404 if requested file does not exist
void onPageNotFound(AsyncWebServerRequest *request) {
  IPAddress remote_ip = request->client()->remoteIP();
  Serial.println("[" + remote_ip.toString() +
                  "] HTTP GET request of " + request->url());
  request->send(404, "text/plain", "Not found");
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  StaticJsonDocument<200> docSend;
  String outputJson;

  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic cmnd/clock/power, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "cmnd/clock/power") {
    Serial.print("Changing output to ");
    
   if(messageTemp == "on" or messageTemp == "ON"){

      Serial.println("clock_state: on");
      clock_state = 1;
      docSend["power"] = "on";
    }
    else if(messageTemp == "off" or messageTemp == "OFF"){     
      Serial.println("clock_state: off");
      clock_state = 0;
      docSend["power"] = "off";
    }

    serializeJson(docSend, outputJson);
    const char* charOutputJson = outputJson.c_str();
    sprintf(msg_buf, "%s", charOutputJson);

    mqttClient.publish("tele/clock/state", charOutputJson);

    printClock();
  }
}

/***********************************************************
 * Main
 */

void setup() {
  // Init LED and turn off
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, LOW);

  // Start Serial port
  Serial.begin(115200);

  // Make sure we can read the file system
  if( !SPIFFS.begin()){
    Serial.println("Error mounting SPIFFS");
    while(1);
  }

  // Start access point
  //WiFi.softAP(ssid, password);

  // Print our IP address
  //Serial.println();
  //Serial.println("AP running");
  //Serial.print("My IP address: ");
  //Serial.println(WiFi.softAPIP());


  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  
  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(3600);


  

  strip.setBrightness(brightness);
  strip.show(); // Initialize all pixels to 'off'

  // Handlers

  // On HTTP request for root, provide index.html file
  server.on("/", HTTP_GET, onIndexRequest);

  // On HTTP request for style sheet, provide style.css
 // server.on("/style.css", HTTP_GET, onCSSRequest);


  // Route to load index.js file
  server.on("/js/index.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/index.js", "text/javascript");
  });

  // Route to load jquery.minicolors.min.js file
  server.on("/js/jquery.minicolors.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/jquery.minicolors.min.js", "text/javascript");
  });

    // Route to load jquery.minicolors.js file
  server.on("/js/jquery.minicolors.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/jquery.minicolors.js", "text/javascript");
  });

  // Route to load jquery-3.5.1.min.js file
  server.on("/js/jquery-3.5.1.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/jquery-3.5.1.min.js", "text/javascript");
  });

  // Route to load moment-with-locales.js file
  server.on("/js/moment-with-locales.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/moment-with-locales.js", "text/javascript");
  });

  server.on("/js/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/js/bootstrap.min.js", "text/javascript");
  });

  server.on("/css/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/bootstrap.min.css", "text/css");
  });

  server.on("/css/jquery.minicolors.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/jquery.minicolors.css", "text/css");
  });

  server.on("/css/jquery.minicolors.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/css/jquery.minicolors.png", "image/png");
  });

  // Handle requests for pages that do not exist
  server.onNotFound(onPageNotFound);

  

  // Start web server
  server.begin();

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  
}

long IntTosevenSeg(int number){
  long sevenSeg;
  switch(number){
      case 0:
        sevenSeg = numbers[0];
        break;
      case 1:
        sevenSeg = numbers[1];
        break;
      case 2:
        sevenSeg = numbers[2];
        break;
      case 3:
        sevenSeg = numbers[3];
        break;
      case 4:
        sevenSeg = numbers[4];
        break;
      case 5:
        sevenSeg = numbers[5];
        break;
      case 6:
        sevenSeg = numbers[6];
        break;
      case 7:
        sevenSeg = numbers[7];
        break;
      case 8:
        sevenSeg = numbers[8];
        break;
      case 9:
        sevenSeg = numbers[9];
        break;  
      default:
        sevenSeg = 0b0000000;
  }  
  return sevenSeg;
}

void printClock(){

  //Get time
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  int intHour = timeClient.getHours();
  int intMinute = timeClient.getMinutes();
  int intSecond = timeClient.getSeconds();

  dots = !dots;

  //Build LED ciffers
  int ciffers[] = {0,0,0,0};
  if (intHour > 9)
    ciffers[3] = (intHour - intHour % 10)/10;
  else
    ciffers[3] = 10;
  
  ciffers[2] = intHour % 10;
  ciffers[1] = (intMinute - intMinute % 10)/10;
  ciffers[0] = intMinute % 10;

  //Debug
  //Serial.println("ciffers");
  //for ( int i = 0; i <4; i++){
  //    Serial.print(ciffers[3-i]);
  //  }
  //Serial.println("");

  //Populate bits to be shown
  ledstrip = IntTosevenSeg(ciffers[0]) << 0;
  ledstrip = ledstrip + (IntTosevenSeg(ciffers[1]) << 7);
  if(dots) 
    ledstrip = ledstrip + (0b11 << 14);
  ledstrip = ledstrip + (IntTosevenSeg(ciffers[2]) << 16);
  ledstrip = ledstrip + (IntTosevenSeg(ciffers[3]) << 23);


  //Write bits to display
  for(int pos =0; pos <strip.numPixels(); pos ++) {
    if (ledstrip & (1 << pos))
      strip.setPixelColor(pos, strip.Color(r_val, b_val, g_val)); //RBG
    else 
      strip.setPixelColor(pos, strip.Color(0, 0, 0));   
  }

  if (clock_state == 0) {
    strip.setBrightness(0);  
    }
  else {
    strip.setBrightness(brightness);
    }
  strip.show();
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ClockClient")) {
      Serial.println("connected");
      // Subscribe
      mqttClient.subscribe("cmnd/clock/power/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {

  _time = millis();

  if (next_time < _time) {
     next_time = _time + 1000;
     if ( clock_state == 1){
       printClock(); 
     }
    
  }
    
  // Look for and handle WebSocket data
  webSocket.loop();

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

}
