#include <Keypad.h>

#define rLED 11
#define gLED 12

// All strings in this array *MUST* be the same length, or code may break
String passwords[] = {"4659", "4242", "5821", "2278", "9779", "5993", "5612", "end"};

String guess;

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
  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);
  Serial.begin(9600);
  digitalWrite(rLED, HIGH);
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
        Serial.println("Pressed: " + String(key) + ", Guess: " + guess);
        if (guess.length() == passwords[1].length()) {
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
  while (passwords[i] != "end") {
    if (passwords[i] == guess) {
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
