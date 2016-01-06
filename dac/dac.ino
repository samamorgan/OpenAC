#include <Keypad.h>
#include <MD5.h>

#define rLED 11
#define gLED 12

// Enter 4 digit pins in this list that have been converted to 32-bit MD5 hexes
String passwords[] = {"8685549650016d9e1d14bf972262450b", 
                      "fe7ecc4de28b2c83c016b5c6c2acd826", 
                      "926c11cc055de9b8d697b6a587d40c4d",
                      "cc7e2b878868cbae992d1fb743995d8f",
                      "8de4aa6f66a39065b3fac4aa58feaccd",
                      "32e0bd1497aa43e02a42f47d9d6515ad",
                      "6e8404c3b93a9527c8db241a1846599a",
                      "end"};

String guess;

int tries;
int pinLength = 4;

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
  Serial.begin(9600);
}

// Main loop
void loop() {
  char key = keypad.getKey();
  if(key) {  // Check for a valid key.
    switch (key) {
      case '*':
      case '#':
        resetGuess(2);
        break;
      default:
        guess = guess + String(key);
        if (guess.length() == pinLength) {
          tries++;
          checkPassword();
        }
    }
  }
}

// Reset the guessed password, passing 1 for LED flash
void resetGuess(int i) {
  if (i > 0) {
    digitalWrite(rLED, LOW);
    delay(100);
    digitalWrite(rLED, HIGH);
    delay(100);
    resetGuess(i-1);
  }
  else {
    guess = "";
  }
}

// Open the door. "true" opens, "false" 
void openDoor(boolean s) {
  if (s) {
    //TODO: Piezo and LCD feedback
    digitalWrite(rLED, LOW);
    digitalWrite(gLED, HIGH);
    delay(3000);
    digitalWrite(gLED, LOW);
    digitalWrite(rLED, HIGH);
    resetGuess(0);
    tries = 0;
  }
  else {
    //TODO: Piezo and LCD feedback
    resetGuess(2);
  }
}

// Secure state that stops any input for an amount of time.
void secure() {
  //TODO: Piezo and LCD feedback
  resetGuess(20);
  tries = 0;
}

// Check the guessed password against the list of possible passwords
void checkPassword() {
  int i;
  char c[5];
  guess.toCharArray(c, 5);
  
  // Convert guess to MD5 hash and hex encode for our string
  unsigned char* hash=MD5::make_hash(c);
  char *md5str = MD5::make_digest(hash, 16);
  free(hash);
  Serial.println(md5str);
  
  while (passwords[i] != "end") {
    if (passwords[i] == md5str) {
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
  free(md5str);
}
