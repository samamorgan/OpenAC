#include <Keypad.h>
#include <Sha256.h>

#define rLED 11
#define gLED 12

// Enter 4 digit PINs in this list that have been converted to 64-bit SHA-256 hexes
// TODO: Move to PROGMEM
String passwords[] = {"ac8c1aa79856748c7dfc370cdd0f5d01841c36b8b22eabf69c4f495bf8eba4d7",
                      "0315b4020af3eccab7706679580ac87a710d82970733b8719e70af9b57e7b9e6",
                      "2f4011ca31d756ee52aa794fa11f9c1d54f0701969a9462607dcdf6abc8eaed9",
                      "9106f1ec4a2142f02273d7a820b6fd53c612fdfbdf8626c96d65af38828e735e",
                      "5c6ab6a10221871a18b25558a77a99d1324732e4d5ac403e0bed5d85acba24fd",
                      "5a2398a2ea274d788e478bf17845d42aa0c9d5aa9c1415ba01156e8609d27dcd",
                      "90fe2c25cc8b9530bd60a2b198ce85c53b06521848c81ba9ecb2a7f57e3c06d8",
                      "end"};

String g;

int tries;
int pinLength = 4;
int openTime = 5000;

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
        resetg(2);
        break;
      default:
        g = g + String(key);
        if (g.length() == pinLength) {
          tries++;
          checkPassword();
        }
    }
  }
}

// Hashes a string with SHA-256
String stringHash() {
  String t;
  String h;
  
  //SHA-256 Attempt
  uint8_t *hash;
  Sha256.init();
  Sha256.print(g);
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
void resetg(int i) {
  if (i > 0) {
    digitalWrite(rLED, LOW);
    delay(100);
    digitalWrite(rLED, HIGH);
    delay(100);
    resetg(i-1);
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
    delay(openTime);
    digitalWrite(gLED, LOW);
    digitalWrite(rLED, HIGH);
    resetg(0);
    tries = 0;
  }
  else {
    //TODO: Piezo and LCD feedback
    resetg(2);
  }
}

// Secure state that stops any input for an amount of time.
void secure() {
  //TODO: Piezo and LCD feedback
  resetg(20);
  tries = 0;
}

// Check the guessed password against the list of possible passwords
void checkPassword() {
  int i;
  String hash = stringHash();
  
  while (passwords[i] != "end") {
    if (passwords[i] == hash) {
      openDoor(true);
      break;
    }
    i++;
  }
  if (passwords[i] == "end") {
    openDoor(false);
  }
  if (tries >= 3) {
    secure();
  }
}
