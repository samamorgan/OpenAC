/*
 * HID RFID Reader Wiegand Interface for Arduino
 * Written by Sam Morgan, 2015.02.10
 * Original HID wiegand interface by by Daniel Smith, 2012.01.30
 *                                      www.pagemac.com
 *
 * This program will decode the wiegand data from a HID RFID Reader (or, theoretically,
 * any other device that outputs wiegand data).
 * The Wiegand interface has two data lines, DATA0 and DATA1.  These lines are normally held
 * high at 5V.  When a 0 is sent, DATA0 drops to 0V for a few μs.  When a 1 is sent, DATA1 drops
 * to 0V for a few μs.  There is usually a few ms between the pulses.
 *
 * Your reader should have at least 4 connections (some readers have more).  Connect the Red wire 
 * to 5V.  Connect the black to ground.  Connect the green wire (DATA0) to the defined DATA0 pin.  
 * Connect the white wire (DATA1) to the defined DATA1 pin.  Orange wire for Green LED is
 * optional. That's it!
 *
 * Operation is simple - each of the data lines are connected to hardware interrupt lines.  When
 * one drops low, an interrupt routine is called and some bits are flipped.  After some time of
 * of not receiving any bits, the Arduino will decode the data.  I've only added the 26 bit and
 * 35 bit formats, but you can easily add more.
 
*/

#include <ESP8266Sha256.h>
#include <WiFiManager.h>             //https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include <FS.h>

#define DATA0 D1                     // Wiegand bit 0 (green)
#define DATA1 D2                     // Wiegand bit 1 (white)
#define GREEN_LED D3                 // Output to blink green LED
#define DOOR_STRIKE D4               // Output to open door
#define MAX_BITS 100                 // max number of bits 
#define WIEGAND_WAIT_TIME  3000      // time to wait for another wiegand pulse.

Ticker ticker;
WiFiClient client;

char buffer[64];

// wiegand stuff
unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int wiegand_counter;        // countdown until we assume there are no more bits

// Phant stuff
const char pushHost[] = "data.sparkfun.com";
const String publicKey = "4JWM5n2Y9yFDadwxW57J";
const String privateKey = "b5XgY96GNmUgxvypRbKk";
const byte NUM_FIELDS = 3;
const String fieldNames[NUM_FIELDS] = {"event", "location", "user"};

// S3 stuff
const char getHost[] = "openac.s3.amazonaws.com";
const char getFile[] = "OpenAC_Plugable.csv";
String fileName = "/users.csv";

const int tcpPort = 80;

String guess;
int tries;

void setup() {
  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);
  pinMode(DOOR_STRIKE, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(GREEN_LED, HIGH);

  ticker.attach(0.6, tick);
  
  Serial.begin(115200);

  WiFiManager wifiManager;
  //wifiManager.resetSettings(); //reset settings - for testing
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println("Connected to WiFi");
  
  // Mount FS and check if formatted
  SPIFFS.begin();
  if(!SPIFFS.exists(fileName)) {
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");
    SPIFFS.format();
    Serial.println("Spiffs formatted");
  }
  // Update the authorized user list for the first time
  if(!updateUsers()) {
    Serial.println("User update failed");
  }
  
  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(DATA0, ISR_INT0, FALLING);
  attachInterrupt(DATA1, ISR_INT1, FALLING);
  
  wiegand_counter = WIEGAND_WAIT_TIME;
  Serial.println("Reader Ready");
  
  //keep LED on
  ticker.detach();
  digitalWrite(BUILTIN_LED, LOW);
}
 
void loop() {
  unsigned long facilityCode = 0;        // decoded facility code
  unsigned long cardCode = 0;            // decoded card code
  unsigned long pin = 0;
  
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--wiegand_counter == 0)
      flagDone = 1;
  }
 
  // if we have bits and we the wiegand counter went out
  if (bitCount && flagDone) {
    unsigned char i;
    // 35 bit HID Corporate 1000 format
    if (bitCount == 35) {
      // facility code = bits 2 to 14
      for (i=2; i<14; i++) {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      // card code = bits 15 to 34
      for (i=14; i<34; i++) {
         cardCode <<=1;
         cardCode |= databits[i];
      }
    }
    
    // standard 26 bit format (H10301)
    else if (bitCount == 26) {
      // facility code = bits 2 to 9
      for (i=1; i<9; i++) {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      // card code = bits 10 to 23
      for (i=9; i<25; i++) {
         cardCode <<=1;
         cardCode |= databits[i];
      }
    }
    
    // For keypad devices, 4 bits per keypress
    else if (bitCount == 4) {
      for (i=0; i<4; i++) {
         pin <<=1;
         pin |= databits[i];
      }
      guess = guess + String(pin);
      if (pin == 10 || pin == 11) {
        guess = "";
        secure(2);
      }
    }

    // Unknown format
    else {
      Serial.println("Unable to decode " + String(bitCount) + " bit formats.");
    }

    // Check if card is authorized
    if (facilityCode && cardCode) {
    String s = checkUser(String(facilityCode)+String(cardCode));
      if (s != "") {
        openDoor();
        tries = 0;
        guess = "";
        postData("Card Entry", s);
      }
    }

    // Check if PIN is authorized
    if (guess.length() >= 4) {
      tries++;
    String s = checkUser(hash(guess));
      if (s != "") {
        tries = 0;
        openDoor();
        postData("PIN Entry", s);
      }
      secure(2);
      guess = "";
    }
 
     // cleanup and get ready for the next card
     bitCount = 0;
     for (i=0; i<MAX_BITS; i++) {
       databits[i] = 0;
     }
     if (tries >= 3) {
       secure(20);
       tries = 0;
       guess = "";
     }     
  }
}

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0() {
  bitCount++;
  flagDone = 0;
  wiegand_counter = WIEGAND_WAIT_TIME;
 
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1() {
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  wiegand_counter = WIEGAND_WAIT_TIME;  
}

// LED tick function
void tick() {
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void postData(String event, String user) {
  //URL encode spaces  
  event.replace(" ", "%20");
  user.replace(" ", "%20");
  
  String fieldData[NUM_FIELDS] = {event, String(ESP.getChipId()), user};
  String request;
  
  // Make a TCP connection to remote host
  if (client.connect(pushHost, tcpPort)) {
    request = "GET /input/" + publicKey + "?private_key=" + privateKey;
    for (int i=0; i<NUM_FIELDS; i++) {
      request = request + "&" + fieldNames[i] + "=" + fieldData[i];
    }
    request = request + " HTTP/1.1";

    client.println(request);
    client.print("Host: ");
    client.println(pushHost);
    client.println("Connection: close");
    client.println();
  }
  else {
    Serial.println(F("Connection failed"));
  } 

  // Check for a response from the server, and route it
  // out the serial port.
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.print(c);
    }      
  }
  Serial.println();
  client.stop();
}

// Hashes a string with SHA-256
String hash(String s) {
  String t;
  String h;
  
  //SHA-256 Attempt
  uint8_t *hash;
  Sha256.init();
  Sha256.print(s);
  hash = Sha256.result();
  for (int i=0; i<=31; i++) {
    h = String(hash[i], HEX);
    if(h.length() < 2) {
      t = t + "0";
    }
    t = t + h;
  }

  return t;
}

// Open the door. True opens and returns true, false returns false.
void openDoor() {
  digitalWrite(DOOR_STRIKE, HIGH);
  digitalWrite(GREEN_LED, LOW);
  delay(5000);
  digitalWrite(DOOR_STRIKE, LOW);
  digitalWrite(GREEN_LED, HIGH);
}

// Secure the door for i*200 milliseconds
void secure(int i) {
  if (i > 0) {
    digitalWrite(GREEN_LED, LOW);
    delay(100);
    digitalWrite(GREEN_LED, HIGH);
    delay(100);
    secure(i-1);
  }
}

// Perform an HTTP GET request to a remote page, stores in local file
int updateUsers() {
  File f = SPIFFS.open(fileName, "w");
  String search = "Connection: close\r\n\r\n";
  
  if (!client.connect(getHost, tcpPort)) {
    return false;
  }
  else if (!f) {
    return false;
  }
  
  // Make an HTTP GET request
  client.print("GET /");
  client.print(getFile);
  client.println(" HTTP/1.1");
  
  client.print("Host: ");
  client.println(getHost);
  client.println(search);

  String s = client.readString();
  int i = s.indexOf(search);
  
  f.print(s.substring(i+search.length()));
  f.close();

  return true;
}

// Takes a card or PIN, checks against users in stored file
String checkUser(String s) {
  File f = SPIFFS.open(fileName, "r");
  String user[3] = {"","",""};
  String result;

  while(f.available()) {
    String t = f.readStringUntil('\n');
    user[0] = t.substring(0,t.indexOf(","));
    t = t.substring(t.indexOf(",")+1);
    user[1] = t.substring(0,t.indexOf(","));
    t = t.substring(t.indexOf(",")+1);
    user[2] = t;
    for(int i = 0; i < sizeof(user)/sizeof(user[0]); i++) {
      user[i].replace("\"","");
    }
    if (s == user[1] || s == user[2]) {
      result = user[0];
      break;
    }
  }
  f.close();
  
  return result;
}
