const int channel = 1;

unsigned long tempo = 500;

int lastTick = 0;
int currentTick = 0;
int dt = 0;

class SequencerTrack{

public:
  int step;
  int steps;  // how many steps are in a track
  int channel;
  bool isGateOpen;
  int stepGates[16];  // whether or not the gate

  SequencerTrack()
  {

  }

  SequencerTrack(int trackSteps, int trackChannel)
  {
    steps = trackSteps;
    // stepGates[16] = { };
    channel = trackChannel;
  }

  void TickStep()
  {
    //Update steps and output.
    if(isGateOpen)
    {
      usbMIDI.sendNoteOff(70 + step%steps, 100, channel);
      isGateOpen = false;
      // Serial.print(step);
      // Serial.print(' ');
      // Serial.println("off");
    }

    step = (step + 1) % 16;

    if(stepGates[step%steps])
    {
      usbMIDI.sendNoteOn(70 + step%steps, 100, channel);
      isGateOpen = true;
      // Serial.print(step);
      // Serial.print(' ');
      // Serial.println("on");
    }

  }

};


int trackCount = 1;
int trackSteps = 16;
SequencerTrack tracks [1];



/***************************************
 * BUTTON PAD
****************************************/




//config variables
#define NUM_LED_COLUMNS (4)
#define NUM_LED_ROWS (4)
#define NUM_BTN_COLUMNS (4)
#define NUM_BTN_ROWS (4)
#define NUM_COLORS (3)

#define MAX_DEBOUNCE (3)

// Global variables
static uint8_t LED_outputs[NUM_LED_COLUMNS][NUM_LED_ROWS];
static int32_t next_scan;


static const uint8_t btnselpins[4]   = {23,21,19,17};
static const uint8_t btnreadpins[4] = {3,7,11,15};
static const uint8_t ledselpins[4]   = {22,20,18,16};

// RGB pins for each of 4 rows
static const uint8_t colorpins[4][3] = {{2,0,1}, {6,4,5}, {10,8,9}, {14,12,13}};


static int8_t debounce_count[NUM_BTN_COLUMNS][NUM_BTN_ROWS];

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


static void toggleButton(int buttonIndex) {
  Serial.print("Toggle! ");
  Serial.println(buttonIndex);

  if (tracks[0].stepGates[buttonIndex] >= 1) {
    tracks[0].stepGates[buttonIndex] = 0;
  } else {
    tracks[0].stepGates[buttonIndex] = 1;
  }

  int row = buttonIndex / 4;
  int column = buttonIndex % 4;

  if (LED_outputs[row][column] == 0) {
    LED_outputs[row][column] = 1;
  } else {
    LED_outputs[row][column] = 0;
  }
}


static int getButtonColor(int buttonIndex) {
  int row = buttonIndex / 4;
  int column = buttonIndex % 4;

  int isCurrentStep = (tracks[0].step % tracks[0].steps) == buttonIndex;
  int isSelected = LED_outputs[row][column] & 0x03;

  if (isCurrentStep && isSelected) {
    return 3;
  } else if (isCurrentStep) {
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
          // Serial.print("Key Down ");
          // Serial.println((current * NUM_BTN_ROWS) + j);

          // LED_outputs[current][j]++;
          toggleButton((current * NUM_BTN_ROWS) + j);
        }
      }
    } else {
      // otherwise, button is released
      if (debounce_count[current][j] > 0) {
        debounce_count[current][j]--;
        if (debounce_count[current][j] == 0) {
          // Serial.print("Key Up ");
          // Serial.println((current * NUM_BTN_ROWS) + j);
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






/***************************************
 * SETUP & LOOP
****************************************/






void setup(){
  Serial.begin(115200);
  Serial.print("Starting Setup...");

  /***************************************
  * TRACK SETUP
  ****************************************/

  tracks [trackCount] = {};

  for(int i=0; i<trackCount; i++)
  {
    tracks[i] = SequencerTrack(16, i+1); //First track is channel 1;
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
      tracks[i].TickStep();
    }
  }

  /***************************************
  * BUTTON LOOP
  ****************************************/

  if (millis() >= next_scan) {
    next_scan = millis()+1;
    scan();
  }
}
