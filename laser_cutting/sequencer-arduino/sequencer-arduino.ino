const int channel = 1;

unsigned long tempo = 500;

int lastTick = 0;
int currentTick = 0;
int dt = 0;

//config variables
#define NUM_LED_COLUMNS (4)
#define NUM_LED_ROWS (4)
#define NUM_BTN_COLUMNS (4)
#define NUM_BTN_ROWS (4)
#define NUM_COLORS (3)

#define NUMBER_OF_PATCHES (4)
#define NUMBER_OF_WIRES (4)

#define MAX_DEBOUNCE (3)

#define BUTTON_HISTORY_MAX (4)

// Global variables
static uint8_t LED_outputs[NUM_LED_COLUMNS][NUM_LED_ROWS];
static int32_t next_scan;


static const uint8_t btnselpins[4]   = {23,21,19,17};
static const uint8_t btnreadpins[4] = {3,7,11,15};
static const uint8_t ledselpins[4]   = {22,20,18,16};

static const uint8_t WIRE_PINS[4] = { 27, 26, 25, 24 };
static const uint8_t PATCH_PINS[4] = { 32, 31, 30, 29 };
static const char* PATCH_NAMES[4] = { "WHITE", "BLUE", "BLACK", "RED" };

// RGB pins for each of 4 rows
static const uint8_t colorpins[4][3] = {{2,0,1}, {6,4,5}, {10,8,9}, {14,12,13}};

static uint8_t patchWireMap[NUMBER_OF_PATCHES];
static int8_t wirePatchMap[NUMBER_OF_WIRES];

static uint8_t newPatchWireMap[NUMBER_OF_PATCHES];
static int8_t newWirePatchMap[NUMBER_OF_WIRES];

static int8_t buttonPressHistory[32];
static int8_t buttonPressHistoryLength = 0;

static int8_t debounce_count[NUM_BTN_COLUMNS][NUM_BTN_ROWS];


class SequencerTrack{

public:
  int step;
  int steps;  // how many steps are in a track
  bool isGateOpen;
  int stepGates[16];  // whether or not the gate

  SequencerTrack()
  {

  }

  SequencerTrack(int trackSteps)
  {
    steps = trackSteps;
  }

  void TickStep(int channel)
  {
    //Update steps
    if(isGateOpen)
    {
      usbMIDI.sendNoteOff(70 + step%steps, 100, channel);
      isGateOpen = false;
    }

    step = (step + 1) % steps;

    if(stepGates[step%steps] && channel > 0)
    {
      Serial.print(PATCH_NAMES[channel-1]);
      Serial.println("!");
      usbMIDI.sendNoteOn(70 + step%steps, 100, channel);
      isGateOpen = true;
    }

  }

  void Unplug(int channel) {
    if (isGateOpen) {
      usbMIDI.sendNoteOff(70 + step%steps, 100, channel);
      isGateOpen = false;
    }
  }
};


int trackCount = 4;
int trackSteps = 4;
SequencerTrack tracks[4];



/***************************************
 * BUTTON PAD
****************************************/





static void setuppins() {
  uint8_t i;

  for(i = 0; i < NUM_LED_COLUMNS; i++) {
    pinMode(ledselpins[i], OUTPUT);
    digitalWrite(ledselpins[i], HIGH);
  }

  for(i = 0; i < NUM_BTN_COLUMNS; i++) {
    pinMode(btnselpins[i], OUTPUT);
    digitalWrite(btnselpins[i], HIGH);
  }

  for(i = 0; i < 4; i++) {
    pinMode(btnreadpins[i], INPUT_PULLUP);
  }

  for(i = 0; i < NUM_LED_ROWS; i++) {
    for(uint8_t j = 0; j < NUM_COLORS; j++) {
      pinMode(colorpins[i][j], OUTPUT);
      digitalWrite(colorpins[i][j], LOW);
    }
  }

  for(uint8_t i = 0; i < NUM_BTN_COLUMNS; i++) {
    for(uint8_t j = 0; j < NUM_BTN_ROWS; j++) {
      debounce_count[i][j] = 0;
    }
  }

  for(i = 0; i < NUMBER_OF_WIRES; i++) {
    pinMode(WIRE_PINS[i], OUTPUT);
    digitalWrite(WIRE_PINS[i], LOW);
  }

  for(i = 0; i < NUMBER_OF_PATCHES; i++) {
    pinMode(PATCH_PINS[i], INPUT_PULLDOWN);
  }

}


static void debugLEDOutputs() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (LED_outputs[r][c]) {
        Serial.print('X');
      } else {
        Serial.print('.');
      }
    }
    Serial.println();
  }

  Serial.println();
}


static void debugWirePatch() {
  for (int i = 0; i < 4; i++) {
    Serial.print(PATCH_NAMES[i]);
    Serial.print("=");
    Serial.print(patchWireMap[i]+1);
    Serial.print(" ");
  }

  Serial.println();
}


static void debugButtonHistory() {
  Serial.print("Length: ");
  Serial.print(buttonPressHistoryLength);
  for (int i = 0; i < 8; i++) {
    Serial.print(" ");
    Serial.print(buttonPressHistory[i]);
  }
  Serial.println();
}


static void turnOnButton(int buttonIndex) {
  if (buttonPressHistoryLength == BUTTON_HISTORY_MAX) {
    // Turn off first
    turnOffButton(buttonPressHistory[0]);
  }

  setButton(buttonIndex, 1);
  buttonPressHistory[buttonPressHistoryLength++] = buttonIndex;
}

static void turnOffButton(int buttonIndex) {
  // Remake the history array without the button we're turning off
  debugButtonHistory();
  for (int newIndex = 0, oldIndex = 0; oldIndex < buttonPressHistoryLength; oldIndex++) {
    if (buttonPressHistory[oldIndex] == buttonIndex) {
      // do nothing
    } else {
      buttonPressHistory[newIndex++] = buttonPressHistory[oldIndex];
    }
  }

  buttonPressHistoryLength--;
  debugButtonHistory();

  setButton(buttonIndex, 0);
}

static boolean isButtonOn(int buttonIndex) {
  int buttonTrackIndex = buttonIndex / trackCount;
  int buttonTrack = (NUMBER_OF_PATCHES-1) - (buttonIndex % trackCount);  // Invert

  return tracks[buttonTrack].stepGates[buttonTrackIndex] >= 1;
}

static void setButton(int buttonIndex, int value) {
  int buttonTrackIndex = buttonIndex / trackCount;
  int buttonTrack = (NUMBER_OF_PATCHES-1) - (buttonIndex % trackCount);  // Invert

  int ledRow = buttonIndex % 4;
  int ledColumn = buttonIndex / 4;

  tracks[buttonTrack].stepGates[buttonTrackIndex] = value;
  LED_outputs[ledRow][ledColumn] = value;
}


static void toggleButton(int buttonIndex) {
  Serial.print("BEFORE ");
  debugButtonHistory();

  if (isButtonOn(buttonIndex)) {
    turnOffButton(buttonIndex);
  } else {
    turnOnButton(buttonIndex);
  }

  Serial.print("AFTER  ");
  debugButtonHistory();
}


static int getButtonColor(int buttonIndex) {
  int buttonTrack = 3 - (buttonIndex % trackCount);
  int buttonTrackIndex = buttonIndex / trackCount;

  int row = buttonIndex % 4;
  int column = buttonIndex / 4;

  int isCurrentStep = (tracks[buttonTrack].step % tracks[buttonTrack].steps) == buttonTrackIndex;
  int isSelected = LED_outputs[row][column] & 0x03;
  int isTrackPlugged = wirePatchMap[buttonTrack] != -1;

  if (isCurrentStep && isSelected && isTrackPlugged) {
    return 3;
  } else if (isCurrentStep && isTrackPlugged) {
    return 2;
  } else if (isSelected) {
    return 1;
  } else {
    return 0;
  }
}





static void scan() {
  static uint8_t current = 0;
  uint8_t val;
  uint8_t i, j;

  digitalWrite(btnselpins[current], LOW);
  digitalWrite(ledselpins[current], LOW);

  for(i = 0; i < NUM_LED_ROWS; i++) {
    uint8_t val = getButtonColor((current * NUM_LED_ROWS) + i);

    if (val) {
      digitalWrite(colorpins[i][val-1], HIGH);
    }
  }

  delay(1);

  for (j = 0; j < NUM_BTN_ROWS; j++) {
    val = digitalRead(btnreadpins[j]);

    if (val == LOW) {
      // active low: val is low when btn is pressed
      if (debounce_count[current][j] < MAX_DEBOUNCE) {
        debounce_count[current][j]++;

        if (debounce_count[current][j] == MAX_DEBOUNCE) {
          toggleButton((current * NUM_BTN_ROWS) + j);
        }
      }
    } else {
      // otherwise, button is released
      if (debounce_count[current][j] > 0) {
        debounce_count[current][j]--;
        if (debounce_count[current][j] == 0) {
          // "key down"
        }
      }
    }
  }

  delay(1);

  digitalWrite(btnselpins[current], HIGH);
  digitalWrite(ledselpins[current], HIGH);

  for(i = 0; i < NUM_LED_ROWS; i++)
  {
    for(j = 0; j < NUM_COLORS; j++)
    {
      digitalWrite(colorpins[i][j], LOW);
    }
  }

  current++;
  if (current >= NUM_BTN_COLUMNS)
  {
    current = 0;
  }
}


static void patchBayScan() {
  for (int j = 0; j < NUMBER_OF_PATCHES; j++) {
    newPatchWireMap[j] = -1;
  }

  for (int wireIdx = 0; wireIdx < NUMBER_OF_WIRES; wireIdx++) {
    newWirePatchMap[wireIdx] = -1;
    digitalWrite(WIRE_PINS[wireIdx], HIGH);
    for (int j = 0; j < NUMBER_OF_PATCHES; j++) {
      int isOn = digitalRead(PATCH_PINS[j]);
      if (isOn) {
        newPatchWireMap[j] = wireIdx;
        newWirePatchMap[wireIdx] = j;
      }
    }
    digitalWrite(WIRE_PINS[wireIdx], LOW);
  }

  for (int patchIdx = 0; patchIdx < NUMBER_OF_PATCHES; patchIdx++) {
    patchWireMap[patchIdx] = newPatchWireMap[patchIdx];
  }

  for (int wireIdx = 0; wireIdx < NUMBER_OF_WIRES; wireIdx++) {
    if (wirePatchMap[wireIdx] != newWirePatchMap[wireIdx] && newWirePatchMap[wireIdx] == -1) {
      tracks[wireIdx].Unplug(wirePatchMap[wireIdx]);
    }

    wirePatchMap[wireIdx] = newWirePatchMap[wireIdx];
  }
}






/***************************************
 * SETUP & LOOP
****************************************/






void setup(){
  Serial.begin(115200);
  Serial.print("Starting Setup...");

  /***************************************
  * TRACK SETUP
  ****************************************/

  for(int i=0; i<trackCount; i++)
  {
    tracks[i] = SequencerTrack(4); //First track is channel 1;
  }

  /***************************************
  * BUTTON SETUP
  ****************************************/

  setuppins();

  next_scan = millis() + 1;

  for(uint8_t i = 0; i < NUM_LED_ROWS; i++) {
    for(uint8_t j = 0; j < NUM_LED_COLUMNS; j++) {
      LED_outputs[i][j] = 0;
    }
  }

  Serial.println("Setup Complete.");
}

void loop() {
  /***************************************
  * TRACK LOOP
  ****************************************/

  dt = millis()-lastTick;

  if(millis() - lastTick > tempo)
  {
    lastTick = millis();

    for(int i=0; i<trackCount; i++)
    {
      tracks[i].TickStep(wirePatchMap[i]+1);
    }

    // debugWirePatch();
  }

  /***************************************
  * BUTTON LOOP
  ****************************************/

  if (millis() >= next_scan) {
    next_scan = millis()+1;
    scan();
  }

  /***************************************
  * PATCH BAY
  ****************************************/

  patchBayScan();
}
