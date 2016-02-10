/*
 * HID RFID Reader Wiegand Interface for Arduino Uno
 * Written by Sam Morgan, 2015.02.10
 * Original HID Weigand interface by by Daniel Smith, 2012.01.30
 *                                      www.pagemac.com
 *
 * This program will decode the wiegand data from a HID RFID Reader (or, theoretically,
 * any other device that outputs weigand data).
 * The Wiegand interface has two data lines, DATA0 and DATA1.  These lines are normall held
 * high at 5V.  When a 0 is sent, DATA0 drops to 0V for a few us.  When a 1 is sent, DATA1 drops
 * to 0V for a few us.  There is usually a few ms between the pulses.
 *
 * Your reader should have at least 4 connections (some readers have more).  Connect the Red wire 
 * to 5V.  Connect the black to ground.  Connect the green wire (DATA0) to Digital Pin 2 (INT0).  
 * Connect the white wire (DATA1) to Digital Pin 3 (INT1).  That's it!
 *
 * Operation is simple - each of the data lines are connected to hardware interrupt lines.  When
 * one drops low, an interrupt routine is called and some bits are flipped.  After some time of
 * of not receiving any bits, the Arduino will decode the data.  I've only added the 26 bit and
 * 35 bit formats, but you can easily add more.
 
*/

#include <Sha256.h>
#include <avr/pgmspace.h>

#define DATA0 2                      // Wiegand bit 0 (green)
#define DATA1 3                      // Wiegand bit 1 (white)
#define GREEN_LED 4                  // Output to blink green LED
#define DOOR_STRIKE 5                // Output to open door

#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another wiegand pulse. 

// 4 digit PINs that have been converted to 64-bit SHA-256 hexes
const char pin01[] PROGMEM = "0315b4020af3eccab7706679580ac87a710d82970733b8719e70af9b57e7b9e6";
const char pin02[] PROGMEM = "9106f1ec4a2142f02273d7a820b6fd53c612fdfbdf8626c96d65af38828e735e";
const char pin03[] PROGMEM = "5a2398a2ea274d788e478bf17845d42aa0c9d5aa9c1415ba01156e8609d27dcd";
const char pin04[] PROGMEM = "90fe2c25cc8b9530bd60a2b198ce85c53b06521848c81ba9ecb2a7f57e3c06d8";
const char pin05[] PROGMEM = "937554b0f5ce7ea8254bfcdfc6f6133841ab7c416ceaa1f5de86b7dbec1b24d9";
const char pin06[] PROGMEM = "196499f197648f9eb37ffafe41ba444d531d59784a3bcddd0e80e77bade487d1";
const char pin07[] PROGMEM = "5c6ab6a10221871a18b25558a77a99d1324732e4d5ac403e0bed5d85acba24fd";
const char pin08[] PROGMEM = "90b5bc7f03c840b2efddb22ffdfc37dd12cb391b49aa0fc8751726c04d32ff30";
const char pin09[] PROGMEM = "24fd7e2735af185459f293eb8704789722c8e46ef86c880322577fe019bb829c";
const char pin10[] PROGMEM = "ac8c1aa79856748c7dfc370cdd0f5d01841c36b8b22eabf69c4f495bf8eba4d7";
const char pin11[] PROGMEM = "cbfad02f9ed2a8d1e08d8f74f5303e9eb93637d47f82ab6f1c15871cf8dd0481";
const char pin12[] PROGMEM = "286c897eba57ce67e79fff80229d9eacddde784b29a18a1d47c17bade5ad1a08";
const char pin13[] PROGMEM = "1c49f22f6de9bd15e5e566fa8983be4cfa4709abf0f95edf96dcd3d6249c2649";
const char pin14[] PROGMEM = "2f4011ca31d756ee52aa794fa11f9c1d54f0701969a9462607dcdf6abc8eaed9";
const char pin15[] PROGMEM = "eb5e19340837ae6d45ab28fa05cb781e34520a3cf8d0b996d3a3875bd50863b9";



// Table to point at PROGMEM PIN hashes
const char* const string_table[] PROGMEM = {pin01, pin02, pin03, pin04, pin05, pin06, pin07, pin08, pin09, pin10, pin11, pin12, pin13, pin14, pin15};

char buffer[64];

// Weigand stuff
unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

String guess;
int tries;

// Card array
unsigned long cards[][2] = {
  {21,15840},
  {21,15841},
  {21,15842},
  {21,15843},
  {21,15844},
  {21,15845},
  {21,15846},
  {21,15847},
  {21,15848},
};
 
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
 
void setup() {
  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);
  pinMode(DOOR_STRIKE, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  digitalWrite(GREEN_LED, HIGH);
  
  Serial.begin(9600);
  Serial.println("Reader Ready");
  
  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(0, ISR_INT0, FALLING);
  attachInterrupt(1, ISR_INT1, FALLING);
  
  weigand_counter = WEIGAND_WAIT_TIME;
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
      // you can add other formats if you want!
      Serial.println("Unable to decode.");
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

// Open the door. "true" opens, "false"
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
  unsigned long card[] = {f,c};
  int numCards = sizeof(cards)/sizeof(cards[0]);
  
  Serial.println(String(card[0]) + ", " + String(card[1]));
  
  for (int i = 0; i < numCards; i++) {
    if (cardCompare(cards[i], card)) {
      return true;
    }
  }
  return false;
}

// Check the guessed pin against the list of possible passwords
boolean checkPin(String s) {
  s = hash(s);
  int numPins = sizeof(string_table)/sizeof(string_table[0]);

  Serial.println(s);
  for (int i = 0; i < numPins; i++) {
    String test = strcpy_P(buffer, (char*)pgm_read_word(&(string_table[i])));
    if (test == s) {
      return true;
    }
  }
  return false;
}
