const int channel = 1;

unsigned long tempo = 500;

int lastTick = 0;
int currentTick = 0;
int dt = 0;

class SequencerTrack{

public:
  int step;
  int steps;
  int channel;
  bool isGateOpen;
  int stepGates [];

  SequencerTrack()
  {

  }

  SequencerTrack(int trackSteps, int trackChannel)
  {
    steps = trackSteps;
    stepGates [steps] = { };
    channel = trackChannel;
  }

  void TickStep()
  {
    //Update steps and output.
    if(isGateOpen)
    {
      usbMIDI.sendNoteOff(70 + step%steps, 100, channel);
      isGateOpen = false;
    }
    
    step++;
    
    if(stepGates[step%steps])
    {
      usbMIDI.sendNoteOn(70 + step%steps, 100, channel);
      isGateOpen = true;
    }
    
  }
  
};

int trackCount = 1;
int trackSteps = 8;
SequencerTrack tracks [1];

void setup(){
  Serial.begin(9600);
  
  //tracks [trackCount] = {};
  
  for(int i=0; i<trackCount; i++)
  {
    tracks[i] = SequencerTrack(8, i+1); //First track is channel 1;
  }
}

void loop() {
  dt = millis()-lastTick;

  if(millis() - lastTick > tempo)
  {
    lastTick = millis();
    
    for(int i=0; i<trackCount; i++)
    {
      tracks[i].TickStep();
    }
  }
  
}
