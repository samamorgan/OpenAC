/*
 * HID RFID Reader Wiegand Interface for Arduino Uno
 * Written by Daniel Smith, 2012.01.30
 * www.pagemac.com
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
 
#define DATA0 2
#define DATA1 3
#define GREEN_LED 4
#define DOOR_STRIKE 5

#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse. 

unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

// Card array
unsigned long cards[][2] = {
  {21,15840},   // Sam
  {21,15841},   // Josh
  {21,15842},   // Bernie
  {21,15843},   // Gary
  {21,15844},   // Amanda
  {21,15845},   // Josephine
  {21,15846},   // Patrick
  {21,15847},   // Bob
  {21,15848},   // Dave
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
  
  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {
    if (--weigand_counter == 0)
      flagDone = 1;  
  }
 
  // if we have bits and we the weigand counter went out
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
    else {
      // you can add other formats if you want!
     Serial.println("Unable to decode."); 
    }
 
     // cleanup and get ready for the next card
     bitCount = 0;
     for (i=0; i<MAX_BITS; i++) {
       databits[i] = 0;
     }
  }
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
