#include <Keypad.h>
#include <Sha256.h>
#include <avr/pgmspace.h>

#define rLED 11
#define gLED 12

// 4 digit PINs that have been converted to 64-bit SHA-256 hexes
const char string0[] PROGMEM = "ac8c1aa79856748c7dfc370cdd0f5d01841c36b8b22eabf69c4f495bf8eba4d7";
const char string1[] PROGMEM = "0315b4020af3eccab7706679580ac87a710d82970733b8719e70af9b57e7b9e6";
const char string2[] PROGMEM = "2f4011ca31d756ee52aa794fa11f9c1d54f0701969a9462607dcdf6abc8eaed9";
const char string3[] PROGMEM = "9106f1ec4a2142f02273d7a820b6fd53c612fdfbdf8626c96d65af38828e735e";
const char string4[] PROGMEM = "5c6ab6a10221871a18b25558a77a99d1324732e4d5ac403e0bed5d85acba24fd";
const char string5[] PROGMEM = "5a2398a2ea274d788e478bf17845d42aa0c9d5aa9c1415ba01156e8609d27dcd";
const char string6[] PROGMEM = "90fe2c25cc8b9530bd60a2b198ce85c53b06521848c81ba9ecb2a7f57e3c06d8";

// Table to refer to strings
const char* const string_table[] PROGMEM = {string0, string1, string2, string3, string4, string5, string6};

char buffer[64];

String g;

int tries;

const byte ROWS = 4; // 4 rows
const byte COLS = 3; // 3 columns

// Define the Keymap
char keys[ROWS][COLS] = {
{'1','2','3'},
{'4','5','6'},
{'7','8','9'},
{'*','0','#'}
};

byte rowPins[ROWS] = {5,4,3,2};
byte colPins[COLS] = {8,7,6};


// Create the Keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  digitalWrite(rLED, HIGH);
}

// Main loop
void loop() {
  char key = keypad.getKey();
  if(key) {  // Check for a valid key.
    switch (key) {
      case '*':
      case '#':
        reset(2);
        break;
      default:
        g = g + String(key);
        if (g.length() == 4) {
          tries++;
          checkPin(g);
        }
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

// Reset the ged password, passing 1 for LED flash
void reset(int i) {
  if (i > 0) {
    digitalWrite(rLED, LOW);
    delay(100);
    digitalWrite(rLED, HIGH);
    delay(100);
    reset(i-1);
  }
  else {
    g = "";
  }
}

// Open the door. "true" opens, "false" 
void openDoor(boolean s) {
  if (s) {
    //TODO: Piezo and LCD feedback
    digitalWrite(rLED, LOW);
    digitalWrite(gLED, HIGH);
    delay(5000);
    digitalWrite(gLED, LOW);
    digitalWrite(rLED, HIGH);
    reset(0);
    tries = 0;
  }
  else {
    //TODO: Piezo and LCD feedback
    reset(2);
  }
}

// Secure state that stops any input for an amount of time.
void secure() {
  //TODO: Piezo and LCD feedback
  reset(20);
  tries = 0;
}

// Check the guessed pin against the list of possible passwords
String checkPin(String s) {
  s = hash(s);
  
  for (int i = 0; i < 6; i++) {
    String test = strcpy_P(buffer, (char*)pgm_read_word(&(string_table[i])));
    if (test == s) {
      return s + " : " + buffer;
    }
  }
  return s + " : No Match";
}
