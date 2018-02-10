#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <SPI.h>  


/*****************************************
 *   This program will connect a huzzah to wifi and listen for commands
 *   It will change the LED brightness for RGB leds
 *   It also reads temperature on a DS18B20
 * 
 *   PINS:
 *   13 - MOSI
 *   14 - SCK
 *   
 *   Changeable pins
 *   4 - red CS
 *   5 - green CS
 *   2 - blue CS
 */
 


//The pins used for CS on the MCP41010 chip to control LED
const int CS_red = 5; // ds 5 is pin D2 (mislabeled) on nodemcu
const int CS_green = 0; // nodemcu D3
const int CS_blue = 2; // nodemcu D4

//define a OneWire sensor pin
OneWire  ds(4);  

//address of the DS18B20 temp sensor
byte OneWireSensor[8] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };


//This string must match database on bakery
const String OneWireName = "LivingRoomTemp";

// wifi connection variables
const String hostname = "LivingRoom";

//Define wifi network
const char* ssid = "SSID";
const char* password = "password";
boolean wifiConnected = false;

//debug logs to syslog server
const char* syslog_IP = "192.168.0.1";

//control and messaging to bakery
const char* bakery_IP = "192.168.0.2";
const int bakery_port = 5008;


//Shouldn't have to change these variables
unsigned int localPort = 5005;
WiFiUDP UDP;
boolean udpConnected = false;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];



void setup(void) {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); //disable AP mode
  
  SPI.begin();    
  pinMode (CS_red, OUTPUT);   
  pinMode (CS_green, OUTPUT); 
  pinMode (CS_blue, OUTPUT); 
  
  //start with all leds off
  digitalWrite(CS_red,0);
  digitalWrite(CS_green,0);
  digitalWrite(CS_blue,0);

}

void loop(void) {

  // check if the WiFi and UDP connections were successful
  if(wifiConnected){
    if(udpConnected){
      // if there’s data available, read a packet
      int packetSize = UDP.parsePacket();
      if(packetSize)
      {
        Serial.println("");
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = UDP.remoteIP();
        for (int i =0; i < 4; i++)
        {
          Serial.print(remote[i], DEC);
          if (i < 3)
          {
            Serial.print(".");
          }
        }
        
        Serial.print(", port ");
        Serial.println(UDP.remotePort());

        // read the packet into packetBufffer
        UDP.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
        Serial.print("Contents:");       
        Serial.print(packetBuffer);
        Serial.println(":");
        
        ReceivedMessage(packetBuffer, packetSize);

        //Clear the char array of old data
        for( int i = 0; i < sizeof(packetBuffer);  ++i )
          packetBuffer[i] = (char)0;
      }
    delay(10);
    }
    else {
      //reconnect to udp port
      udpConnected = connectUDP();
    }
  }
  else{
    // Initialise wifi connection, since it failed
    wifiConnected = connectWifi();
  }

  
}




// connect to UDP – returns true if successful or false if not
boolean connectUDP(){
  boolean state = false;
  
  Serial.println("");
  Serial.println("Connecting to UDP");

  if(UDP.begin(localPort) == 1){
    Serial.println("Connection successful");
    state = true;
  }
  else{
    Serial.println("Connection failed");
  }

  return state;
}


// connect to wifi – returns true if successful or false if not
// Should send a notice to bakery with the ID of this arduino and IP address
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 10){
      state = false;
      break;
    }
    i++;
  }
  
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());    
    //Send message to update database
    UpdateLEDBakery();
    UpdateSensorBakery();
  }
  else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  return state;
}


//Sends a message to bakery 
void UpdateLEDBakery(){

  String LogMessage = "";
  LogMessage += "led_RGB:";
  LogMessage += hostname;
  LogMessage += ":";   
  LogMessage += WiFi.localIP().toString();  

  Serial.println(LogMessage);
  
  //convert string to char array and send to syslog
  char UDPMessage[LogMessage.length() + 1]; 
  LogMessage.toCharArray(UDPMessage,LogMessage.length() + 1);
  
  UDP.beginPacket(bakery_IP, bakery_port);
  UDP.write(UDPMessage);
  UDP.endPacket();
  
}


//Sends a message to bakery 
void UpdateSensorBakery(){

  String LogMessage = "";
  LogMessage += "OneWire:";
  LogMessage += hostname;
  LogMessage += ":";  
  LogMessage += OneWireName;
  LogMessage += ":"; 
  LogMessage += WiFi.localIP().toString();  

  Serial.println(LogMessage);
  
  //convert string to char array and send to syslog
  char UDPMessage[LogMessage.length() + 1]; 
  LogMessage.toCharArray(UDPMessage,LogMessage.length() + 1);
  
  UDP.beginPacket(bakery_IP, bakery_port);
  UDP.write(UDPMessage);
  UDP.endPacket();
  
}



//Sends a message to bakery 
void LogSensorBakery(String OneWireName, byte OneWireaddr[8]){
  
  float celsius = get1WireTemperature(OneWireSensor);
  //float celsius = 10.0;
  
  String LogMessage = "";
  LogMessage += "OneWireSensor:";
  LogMessage += hostname;
  LogMessage += ":";  
  LogMessage += OneWireName;
  LogMessage += ":";  
  LogMessage += String(celsius);  

  Serial.println(LogMessage);
  
  //convert string to char array and send to syslog
  char UDPMessage[LogMessage.length() + 1]; 
  LogMessage.toCharArray(UDPMessage,LogMessage.length() + 1);
  
  UDP.beginPacket(bakery_IP, bakery_port);
  UDP.write(UDPMessage);
  UDP.endPacket();
 
}



//Log LED changes
void LEDLogging(String colour, int LED_brightness){

  String LogMessage = "";
  LogMessage += hostname;
  LogMessage += " : Setting ";
  LogMessage += colour;
  LogMessage +=" LED to : ";
  
  //Need to fix reversed wires, so 255 - value
  LogMessage += 255 - LED_brightness;

  Serial.println(LogMessage);
  
  //convert string to char array and send to syslog
  char UDPMessage[LogMessage.length() + 1]; 
  LogMessage.toCharArray(UDPMessage,LogMessage.length() + 1);
  
  UDP.beginPacket(syslog_IP, 514);
  UDP.write(UDPMessage);
  UDP.endPacket();
}


//Process a message that was received via udp
void ReceivedMessage (String inputString, int packetSize){   
   if (inputString.length() > 0) {
    //look for "set:" and 1-3 characters after that
    if (inputString.startsWith("set_red:") && packetSize >= 9 && packetSize <= 11) {
      int LED_red = getValue(inputString, ':', 1).toInt();      
      if (LED_red >= 0 && LED_red <= 255 ) {               
        LEDLogging("Red",LED_red);
        WriteSPI(CS_red,LED_red);
      }
    }

    if (inputString.startsWith("set_green:") && packetSize >= 11 && packetSize <= 13) {      
      int LED_green = getValue(inputString, ':', 1).toInt();
      if (LED_green >= 0 && LED_green <= 255 ) {
        LEDLogging("Green",LED_green);        
        WriteSPI(CS_green,LED_green);              
      }
    }
    
    if (inputString.startsWith("set_blue:") && packetSize >= 10 && packetSize <= 12) {      
      int LED_blue = getValue(inputString, ':', 1).toInt();
      if (LED_blue >= 0 && LED_blue <= 255 ) {    
        LEDLogging("Blue",LED_blue);    
        WriteSPI(CS_blue,LED_blue);              
      }
    }

    if (inputString.startsWith("set_all:") && packetSize >= 13 && packetSize <= 19) {
      //Need to split the packet into 3 variables
      int LED_red   = getValue(inputString, ':', 1).toInt();
      int LED_green = getValue(inputString, ':', 2).toInt();
      int LED_blue  = getValue(inputString, ':', 3).toInt();

      if ( 0 <= LED_red && LED_red <= 255 && 0 <= LED_green && LED_green <= 255 && 0 <= LED_blue && LED_blue <= 255) {
        LEDLogging("Red",LED_red);
        WriteSPI(CS_red,LED_red);
        
        LEDLogging("Green",LED_green);
        WriteSPI(CS_green,LED_green);
        
        LEDLogging("Blue",LED_blue);
        WriteSPI(CS_blue,LED_blue);        
      }
    }  


    //1wire temperature sensor  
    //OneWireName
    if (inputString.startsWith("get_temp:") && packetSize >= 10 && packetSize <= 30) {
      String SensorName = getValue(inputString, ':', 1);      
      if (SensorName == OneWireName ) {
        Serial.println("Sending temperature update to bakery");               
        LogSensorBakery(OneWireName, OneWireSensor);
      }
    }

    
  }  
}

//getValue is used to split the array into different variables, seperated by ":"
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


//writes to SPI to program MCP4010 chips
void WriteSPI(int CS,byte value) 
{
  // Note that the integer vale passed to this subroutine
  // is cast to a byte
  digitalWrite(CS,LOW);
  SPI.transfer(B00010001); // This tells the chip to set the pot
  
  //Need to fix reversed wires, so 255 - value
  SPI.transfer(255 - value);     // This tells it the pot position
  digitalWrite(CS,HIGH); 
}



//returns the temperature in celsius
float get1WireTemperature(byte OneWire_addr[8])
{
  byte i;
  byte present = 0;
  byte type_s;
  byte OneWire_data[12];

  //Display the 1wire address
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(OneWire_addr[i], HEX);
  }
  Serial.println();
    
  
// the first ROM byte indicates which chip
  switch (OneWire_addr[0]) {
    case 0x10:      
      type_s = 1;
      break;
    case 0x28:     
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
  } 

  ds.reset();
  ds.select(OneWire_addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(OneWire_addr);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    OneWire_data[i] = ds.read();
    Serial.print(OneWire_data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(OneWire_data, 8), HEX);
  Serial.println();


  int16_t raw = (OneWire_data[1] << 8) | OneWire_data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (OneWire_data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - OneWire_data[6];
    }
  } else {
    byte cfg = (OneWire_data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  
  return (float)raw / 16.0;
}





