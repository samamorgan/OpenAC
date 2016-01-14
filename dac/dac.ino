#include <Keypad.h>
#include <Sha256.h>
#include <avr/pgmspace.h>

#define rLED 11
#define gLED 12

// 4 digit PINs that have been converted to 64-bit SHA-256 hexes
const char pin1[] PROGMEM = "ac8c1aa79856748c7dfc370cdd0f5d01841c36b8b22eabf69c4f495bf8eba4d7";
const char pin2[] PROGMEM = "0315b4020af3eccab7706679580ac87a710d82970733b8719e70af9b57e7b9e6";
const char pin3[] PROGMEM = "2f4011ca31d756ee52aa794fa11f9c1d54f0701969a9462607dcdf6abc8eaed9";
const char pin4[] PROGMEM = "9106f1ec4a2142f02273d7a820b6fd53c612fdfbdf8626c96d65af38828e735e";
const char pin5[] PROGMEM = "5c6ab6a10221871a18b25558a77a99d1324732e4d5ac403e0bed5d85acba24fd";
const char pin6[] PROGMEM = "5a2398a2ea274d788e478bf17845d42aa0c9d5aa9c1415ba01156e8609d27dcd";
const char pin7[] PROGMEM = "90fe2c25cc8b9530bd60a2b198ce85c53b06521848c81ba9ecb2a7f57e3c06d8";

// Table to point at PROGMEM PIN hashes
const char* const string_table[] PROGMEM = {pin1, pin2, pin3, pin4, pin5, pin6, pin7};

const byte ROWS = 4; // 4 rows
const byte COLS = 4; // 4 columns

// Define the Keymap
char keys[ROWS][COLS] = {
{'1','2','3','A'},
{'4','5','6','B'},
{'7','8','9','C'},
{'*','0','#','D'}
};

byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,8,9};

char buffer[64];


// Create the Keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  digitalWrite(rLED, HIGH);
  Serial.begin(9600);
}

// Main loop
void loop() {
  int tries = 0;
  String guess;
  
  while (tries < 4) {
    char key = keypad.getKey();
    if(key) {  // Check for a valid key.
    switch (key) {
      case '*':
      case '#':
        tries++;
        guess = "";
        secure(2);
        break;
      default:
        guess = guess + String(key);
        if (guess.length() == 4) {
          tries++;
          if(openDoor(checkPin(guess))) {
            tries = 0;
          }
          secure(2);
          guess = "";
        }
      }
    }
  }
  if (tries > 3) {
    secure(20);
    tries = 0;
    guess = "";
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

// Secure the door for i*200 milliseconds
void secure(int i) {
  if (i > 0) {
    digitalWrite(rLED, LOW);
    delay(100);
    digitalWrite(rLED, HIGH);
    delay(100);
    secure(i-1);
  }
}

// Open the door. "true" opens, "false"
// TODO: Piezo and LCD feedback
boolean openDoor(boolean s) {
  if (s) {
    digitalWrite(rLED, LOW);
    digitalWrite(gLED, HIGH);
    delay(5000);
    digitalWrite(gLED, LOW);
    digitalWrite(rLED, HIGH);
    secure(0);
    return true;
  }
  else {
    return false;
  }
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
