/*
 * HID RFID Reader Wiegand Interface for Arduino
 * Written by Sam Morgan, 2015.02.10
 * Original HID Weigand interface by by Daniel Smith, 2012.01.30
 *                                      www.pagemac.com
 *
 * This program will decode the wiegand data from a HID RFID Reader (or, theoretically,
 * any other device that outputs weigand data).
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
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <Ticker.h>

#define DATA0 D1                      // Wiegand bit 0 (green)
#define DATA1 D2                      // Wiegand bit 1 (white)
#define GREEN_LED D3                  // Output to blink green LED
#define DOOR_STRIKE D4                // Output to open door
#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another wiegand pulse.

Ticker ticker;
WiFiClient client;

char buffer[64];

// Weigand stuff
unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

// Phant stuff
char server[] = "data.sparkfun.com";
const String publicKey = "xx";
const String privateKey = "xx";
const byte NUM_FIELDS = 3;
const String fieldNames[NUM_FIELDS] = {"event", "location", "user"};

String guess;
int tries;

// User array. {Name, Card, Hashed PIN}
const char* const users[][3] = {
  {"Sam Morgan","2115840","2f4011ca31d756ee52aa794fa11f9c1d54f0701969a9462607dcdf6abc8eaed9"},
  {"Joshua Henry","2115841","ac8c1aa79856748c7dfc370cdd0f5d01841c36b8b22eabf69c4f495bf8eba4d7"},
  {"Bernie Thompson","2115842","dc4aa707636ec734bb22a822b62592c85c668c511f10e98da810420c8d5d2181"},
  {"Gary Zeller","2115843","5c6ab6a10221871a18b25558a77a99d1324732e4d5ac403e0bed5d85acba24fd"},
  {"Stephen McCray","2115844","6a34287a9fe2312a761bd5158586999535cbc2c83ded6817adeb8d394e0b9abd"},
  {"Patric Neumann","2115846","286c897eba57ce67e79fff80229d9eacddde784b29a18a1d47c17bade5ad1a08"},
  {"Bob Boerner","2115847","337e00526bf9896c0ee150da7b454d72a2ce2e56a13de48e31b30fe305cb556a"},
  {"Dave Connor","2115848","90fe2c25cc8b9530bd60a2b198ce85c53b06521848c81ba9ecb2a7f57e3c06d8"},
  {"Amanda Henry","2115849","0315b4020af3eccab7706679580ac87a710d82970733b8719e70af9b57e7b9e6"},
  {"Bill Saltstein","2115850","26a020fdeb929b63b99ce0c66a95dec62364d84b3be7647b8453a0dbdce8d550"},
  {"David Roberts","2115851","937554b0f5ce7ea8254bfcdfc6f6133841ab7c416ceaa1f5de86b7dbec1b24d9"},
  {"David Washburn","2115852","196499f197648f9eb37ffafe41ba444d531d59784a3bcddd0e80e77bade487d1"},
  {"Ivan Ferrari","2115853","33c604aed594a9618f582adf5c78809254b81770ae3638d628e3c3b5012c357a"},
  {"Jordan Welch","2115854","24fd7e2735af185459f293eb8704789722c8e46ef86c880322577fe019bb829c"},
  {"Kayla Ostasiewski","2115855","cbfad02f9ed2a8d1e08d8f74f5303e9eb93637d47f82ab6f1c15871cf8dd0481"},
  {"Michelle Perreira","2115856","179f912d515ddb97a08feeb4e376996148da3bb7cb5d974b813c8afbdc61662b"},
  {"Sachiko Kuramura","2115857","fb1a382ee284b5583dd34f44245ab1444e083b4daec944fa533c19806ff3e90a"},
  {"David Quesenberry","2115858","bfc57feb2cbcfaf1c2f54172ff49665bbe60629e9cc1494b7a77a7b2baff3743"},
  {"Charlotte Young","2115859","c05b79d959e2e32c57d847112c3f1d317ce69a97196ce1cf662836537979770b"},
  {"Andy DeLaVergne","2115860","c75eca238ec250c28443dcb5bf69f9af1d465b62ebe19ef91874801b1b29bade"},
  {"Jaimee Parker","2115861","d9a5223b761c375d1263e6e57ebec42d3e0fe3f6f283488d2eb204fb6ff17ee5"},
  {"Gina Saifullah","2115876","b3658670d2a52e2c9af9900e5c34acaae4009113569447616ce7932ba67496d7"},
  {"Ritu Java","2115845","eb43272640b269219a01caf99c5a4122d6edc0916d45ac13c0ce80ca3ad2def0"},
  //INTERNS
  {"Justin Taylor","2115862","a19fbf8bf0530ca46179b803a8234f56276f21c0e7dc2f84c682924b95de5801"},
  {"Marissa Krantz","2115863","05758cd3875ad2171484c0026ccbb8adc210cd2d852407e3c7af1b751f35fdd6"},
  {"Braden Anderson","2115864","8552b874fd59a64333e862c0a1eda2cf54fa62e3ebb0710c7fb6b82858ede198"},
  {"Yuxin Fu","2115865","704240a9a309acf9c7df7febba1ec2723c6d2760a17d8b923af205e0d24cf0b9"},
  {"Bruce Oliver","2115866","7f489ad8915c281e41ae9affb4bdf12dd0b6746726632bb7301a6616ca7b04a0"},
  {"Jia Wen Chan","2115867","2cec8cf0e321c284fa0c2ebef804aac18bf1cbb85546f89e7e3d0b6aa8b9d2cf"},
  {"Tyler Hartje","2115868","dde66966c76b6d0589037b6610b5cbcb58582228339a4b6117592d62874a8638"},
  {"Robert Furgeson","2115869","0b0829e2c11114e877eaaf3115a09f8abe8c3a1bcc9be27925f4df79ce964785"},
  {"Sebastian Bury","2115870","70fa656aa0391eb9ef7bbc9c7e6771ec09e7d5d7ab1fcbbde2480d21263ee79a"},
  {"Janani Sankarasubramanian","2115871","0fcfb0d3bee2a790484b7e48314198b48ec0b6ba5b4646533f2c2b488cb77f78"},
  {"Christopher Sibley","2115872","b9102e884eb1d49c328fbaa4b130e043a74a8749e573a4b6c68aa875f2e5e9e8"},
  {"Nicholas Farn","2115873","abd3f2ed90f684b305ed7632ee24e58ed439de9158399ce9a5952281ef804b43"},
  {"Olivia Corrin","2115875","e68adee72e4d3c421f15cc33523be1c38208e4489b91d5e426503d99c5a7d790"}
};

int numUsers = sizeof(users)/sizeof(users[0]);

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0() {
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;
 
}
 
// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1() {
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;  
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

void setup() {
  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);
  pinMode(DOOR_STRIKE, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  ticker.attach(0.6, tick);
  
  digitalWrite(GREEN_LED, HIGH);
  
  Serial.begin(115200);

  WiFiManager wifiManager;
  //wifiManager.resetSettings(); //reset settings - for testing
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println("Connected to WiFi");
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, LOW);
  
  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(DATA0, ISR_INT0, FALLING);
  attachInterrupt(DATA1, ISR_INT1, FALLING);
  
  weigand_counter = WEIGAND_WAIT_TIME;
  Serial.println("Reader Ready");
  Serial.println("Users: " + String(numUsers));
}
 
void loop() {
  unsigned long facilityCode=0;        // decoded facility code
  unsigned long cardCode=0;            // decoded card code
  unsigned long pin=0;
  
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;  
  }
 
  // if we have bits and we the wiegand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
     
    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for more info
    if (bitCount == 35) {
      // 35 bit HID Corporate 1000 format
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
 
      openDoor(checkCard(facilityCode, cardCode));
    }
    else if (bitCount == 26) {
      // standard 26 bit format
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
 
      openDoor(checkCard(facilityCode, cardCode));
    }
    else if (bitCount == 4) {
      // for keypad devices
      // 4 bits per keypress
      for (i=0; i<4; i++) {
         pin <<=1;
         pin |= databits[i];
      }
      guess = guess + String(pin);
      if (pin == 10 || pin == 11) {
        guess = "";
        secure(2);
      }
      if (guess.length() >= 4) {
        tries++;
        if(openDoor(checkPin(guess))) {
          tries = 0;
        }
        secure(2);
        guess = "";
      }
    }
    else {
      // Unknown format
      Serial.println("Unable to decode " + String(bitCount) + " bit formats.");
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

void postData(String event, String location, String user) {
  //URL encode spaces  
  event.replace(" ", "%20");
  location.replace(" ", "%20");
  user.replace(" ", "%20");
  
  String fieldData[NUM_FIELDS] = {event, location, user};
  String request;
  
  // Make a TCP connection to remote host
  if (client.connect(server, 80)) {
    request = "GET /input/" + publicKey + "?private_key=" + privateKey;
    for (int i=0; i<NUM_FIELDS; i++) {
      request = request + "&" + fieldNames[i] + "=" + fieldData[i];
    }
    request = request + " HTTP/1.1";

    client.println(request);
    client.print("Host: ");
    client.println(server);
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
boolean openDoor(boolean s) {
  if (s) {
    digitalWrite(DOOR_STRIKE, HIGH);
    digitalWrite(GREEN_LED, LOW);
    delay(5000);
    digitalWrite(DOOR_STRIKE, LOW);
    digitalWrite(GREEN_LED, HIGH);
    return true;
  }
  else {
    return false;
  }
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

// Compares one card to another, returns true if matched
boolean cardCompare(unsigned long card[2], unsigned long check[2]) {
  if (card[0] == check[0]) {
    if (card[1] == check[1]) {
      return true;
    }
  }
  return false;
}

boolean checkCard(unsigned long f, unsigned long c) {
  String card = String(f)+String(c);
  Serial.println(card);
  
  for (int i = 0; i < numUsers; i++) {
    String test = users[i][1];
    if (test == card) {
      postData("Card Entry", String(ESP.getChipId()), String(users[i][0]));
      return true;
    }
  }
  return false;
}

boolean checkPin(String s) {
  s = hash(s);

  Serial.println(s);
  for (int i = 0; i < numUsers; i++) {
    String test = users[i][2];
    if (test == s) {
      postData("PIN Entry", String(ESP.getChipId()), String(users[i][0]));
      return true;
    }
  }
  return false;
}
