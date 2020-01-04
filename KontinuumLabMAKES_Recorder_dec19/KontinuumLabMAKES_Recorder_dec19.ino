
int breathPin = 25;

int keyPins[8] = {1, 3, 4, 16, 17, 18, 19, 22};

// Button pins:
int calibrateBtn = 8;
int calibrating = 0;

// Variables for the analog breath sensor Initial minumum and maximum,
// plus raw values, mapped output values and previous values:
int breathMin = 350;
int breathMax = 100;
int breathRaw;
int lastBreathRaw;
int breathOut;
int lastBreathOut;


// Which MIDI channel to send on:
int MIDIchannel = 1;


int keyReadings[8];
int lastKeyReadings[8];
// Binary on / off value for the individual keys:
int activeKeys[8];


int fingerings[15][14] = {
  {2, 100, 1, 3, 100, 0, 0, 0, 0, 0, 0, 0, 60}, // C
  {1, 100, 2, 3, 100, 0, 0, 0, 0, 0, 0, 0, 59}, // B
  {1, 3, 4, 6, 100, 2, 5, 100, 0, 0, 0, 0, 58}, // Bb
  {1, 2, 100, 3, 4, 5, 100, 0, 0, 0, 0, 0, 57}, // A
  {1, 2, 4, 5, 100, 3, 6, 100, 0, 0, 0, 0, 56}, // Ab
  {1, 2, 3, 100, 4, 5, 6, 100, 0, 0, 0, 0, 55}, // G
  {1, 2, 3, 5, 6, 100, 4, 100, 0, 0, 0, 0, 54}, // F#
  {1, 2, 3, 4, 100, 5, 6, 7, 100, 0, 0, 0, 53}, // F
  {1, 2, 3, 4, 5, 100, 6, 7, 100, 0, 0, 0, 52}, // E
  {1, 2, 3, 4, 5, 7, 100, 6, 100, 0, 0, 0, 51}, // Eb
  {1, 2, 3, 4, 5, 6, 100, 7, 100, 0, 0, 0, 50}, // D
  {1, 2, 3, 4, 6, 7, 100, 5, 100, 0, 0, 0, 49}, // C#
  {1, 2, 3, 4, 5, 6, 7, 100, 100, 0, 0, 0, 48} // C  
};

int octaveSensor = 0;

// current and previous MIDI note number:
int currentNote;
int lastNote;

// Octave value:
int octave = 0;


// Initial minimum and maximum values for key sensors:
int keyMin[8] = {150, 150, 150, 150, 150, 150, 150, 150};
int keyMax[8] = {300, 300, 300, 300, 300, 300, 300, 300};
// Threshold for activating each sensor, in the context of a reading mapped to MIDI range:
int keyThreshold[8] = {40, 40, 40, 40, 40, 40, 40, 40};

// Error value for the exponential filter:
float error;



void setup() {
  Serial.begin(9600);
  pinMode(calibrateBtn, INPUT_PULLUP);
}


// Helper function for the exponential filter function:
float snapCurve(float x){
  float y = 1.0 / (x + 1.0);
  y = (1.0 - y) * 2.0;
  if(y > 1.0) {
    return 1.0;
  }
  return y;
}

// Main exponential filter function. Input "snapMult" = speed setting. 0.001 = slow / 0.1 = fast:
int expFilter(int newValue, int lastValue, int resolution, float snapMult){
  unsigned int diff = abs(newValue - lastValue);
  error += ((newValue - lastValue) - error) * 0.4;
  float snap = snapCurve(diff * snapMult);
  float outputValue = lastValue;
  outputValue  += (newValue - lastValue) * snap;
  if(outputValue < 0.0){
    outputValue = 0.0;
  }
  else if(outputValue > resolution - 1){
    outputValue = resolution - 1;
  }
  return (int)outputValue;
}

void loop() {
  int i;


  // Read calibrate button. Inverse result for "1" to be "yes":
  calibrating = !digitalRead(calibrateBtn);
  Serial.println(calibrating);
  // Calibration function:
  if(calibrating == 1){
    // Set sensor minimum really high, and sensor maximum really low:
    for(i = 0; i < 8; i++){
      keyMax[i] = 0;
      keyMin[i] = 2000;
      breathMin = 0;
      breathMax = 1023;
    }
    // Keep calibrating while button is pressed:
    while(calibrating == 1){
      // Read all sensors. If lower than "min" set as new min. If higher than "max" set as new max
      for(i = 0; i < 8; i++){
        keyReadings[i] = touchRead(keyPins[i]);
        if(keyReadings[i] < keyMin[i]){
          keyMin[i] = keyReadings[i];
        }
        else if(keyReadings[i] > keyMax[i]){
          keyMax[i] = keyReadings[i];
        }
      }
      breathRaw = analogRead(breathPin);
      if(breathRaw > breathMin){
        breathMin = breathRaw;
      }
      else if(breathRaw < breathMax){
        breathMax = breathRaw;
      }
     // Read button every loop:
      calibrating = !digitalRead(calibrateBtn);
    }

    // Adjust minimum and maximum values with a buffer value:
    for(i = 0; i < 8; i++){
      keyMax[i] = keyMax[i] - 10;
      keyMin[i] = keyMin[i] + 10;
    }
    breathMax = breathMax + 20;
    breathMin = breathMin - 30;
  }
  // Finish calibration//

  


  // Save previous breath sensor raw value, then read and filter new value:
  lastBreathRaw = breathRaw;
  breathRaw = analogRead(breathPin);
  breathRaw = expFilter(breathRaw, lastBreathRaw, 1024, 0.01);
  // Save previous breath sensor output value, then map new value from raw reading:
  lastBreathOut = breathOut;
  breathOut = map(breathRaw, breathMin, breathMax, 0, 127);
//  Serial.println(breathOut);

  // Limit output to MIDI range:
  if(breathOut < 0){
    breathOut = 0;
  }
  else if(breathOut > 127){
    breathOut = 127;
  }
  Serial.println(breathOut);
  // If breath sensor output value has changed: 
  if(breathOut != lastBreathOut){
//    Serial.println(breathOut);
    // Send CC2 volume control:
    usbMIDI.sendControlChange(2, breathOut, MIDIchannel);
    // If breath sensor recently DEactivated, send note off message:
    if(breathOut == 0){
      usbMIDI.sendControlChange(123, 0, MIDIchannel);
    }
    // Else if breath sensor recently activated, send note on message:
    else if(breathOut != 0 && lastBreathOut == 0){
      usbMIDI.sendNoteOn(currentNote, 127, MIDIchannel);
    }
  }



  // Key reading for loop is only done if breath sensor output is NOT equal to zero:
  if(breathOut != 0){ 
    // Main key reading for loop. "i" is sensor number:
    for(i = 0; i < 8; i++){
      // Read sensor at current position:
      keyReadings[i] = touchRead(keyPins[i]);
      // Map reading to MIDI range:
      keyReadings[i] = map(keyReadings[i], keyMin[i], keyMax[i], 0, 127);
      // Limit reading to MIDI range:
      if(keyReadings[i] > 127){
        keyReadings[i] = 127;
      }
      if(keyReadings[i] < 0){
        keyReadings[i] = 0;
      }
      // Filter reading for smoothness:
      keyReadings[i] = expFilter(keyReadings[i], lastKeyReadings[i], 128, 0.01);
      // Activate "activeKeys" at current position, if reading is higher than threshold:
      if(keyReadings[i] > keyThreshold[i]){
        activeKeys[i] = 1;
      }
      // Else, DEactivate "activeKeys" at current position:
      else{
        activeKeys[i] = 0;
      }
      Serial.print(activeKeys[i]);
      Serial.print(" - ");
    }
    Serial.println();



    // Variables for the fingering-analysis section:
    int keyPhase;
    int correct = 0;
    int newNote = 0;
    lastNote = currentNote;

    // If octave key is activated, add 12 semitones to final result, else add zero:
    if(activeKeys[octaveSensor] == 1){
      octave = 0;
    }
    else{
      octave = 12;
    }
    
    // Main fingering-analysis for-loop.
    // "n" = fingering array, "i" = position within the fingering loop:
    int n;
    for(n = 0; n < 13; n++){
      // During first keyPhase, check if the requested sensor is active. 
      keyPhase = 0;
      for(i = 0; i < 14; i++){
        if(fingerings[n][i] != 100){
          // If active sensors coincide with "fingerings" array requirements: "correct" = yes.
          // Else: "correct" = no and exit current "fingering" array.
          if(keyPhase == 0){  // Check "closed" keys
            if(activeKeys[fingerings[n][i]] == 1){
              correct = 1;
            }
            else{
              correct = 0;
              break;
            }
          }
          // During second keyPhase, check 
          else if(keyPhase == 1){ // Check "open" keys
            if(activeKeys[fingerings[n][i]] == 0){
              correct = 1;
            }
            else{
              correct = 0;
              break;
            }
          }       
        }
        // When a "100" is encountered, add 1 to keyPhase, except if keyPhase is 1, exit loop:
        else if(fingerings[n][i] == 100){
          if(keyPhase == 1){
            break;
          }
          else{
            keyPhase ++;
          }
        }
      }
      // if "correct" is still yes after all loops finish, then a "fingerings" array has perfectly coincided with
      // current situation. Set "currentNote to the note defined at the end of that array:
      if(correct == 1){
        currentNote = fingerings[n][12];
        break;
      }
    }
    // If correct is 1, then there is a new note to report, so set "newNote" to 1, and set "currentNote" to
    // currentNote plus octave: 
    if(correct == 1){
      currentNote = currentNote + octave;
      if(currentNote != lastNote){
        newNote = 1;
      }
    }
    // If newNote is a yes, then send MIDI messages. lastNote off and currentNote On
    if(newNote == 1){
      Serial.print("Note: ");
      Serial.println(currentNote);
//      delay(1000);
      usbMIDI.sendNoteOn(lastNote, 0, MIDIchannel);
      usbMIDI.sendNoteOn(currentNote, 127, MIDIchannel);
    }
  }

}
