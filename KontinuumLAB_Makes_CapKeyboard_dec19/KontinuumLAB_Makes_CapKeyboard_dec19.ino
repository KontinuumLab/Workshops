// Multiplexer control pins:
int muxPin1 = 9;
int muxPin2 = 10;
int muxPin3 = 11;
int muxPin4 = 12;
int touchPin = 0;

// Key sensors initial minimum and maximum:
int sensorMin[16] = {365, 365, 365, 365, 365, 365, 365, 365, 365, 365, 365, 365, 365, 365, 365, 365};
int sensorMax[16] = {550, 550, 550, 550, 550, 550, 550, 550, 550, 550, 550, 550, 550, 550, 550, 550};

// current and last readings on keySensors:
int sensorReadings[17];
int lastSensorReadings[17];

// Currently pressed keys:
int activeSensors[16];
int lastActiveSensor;
int lastLastActiveSensor;

// Extra touchRead pins for pitch bend:
int bendDnPin = 17;
int bendUpPin = 18;

//  Pitch bend pins initial minimum and maximum:
int bendDnPinMin = 150;
int bendDnPinMax = 300;
int bendUpPinMin = 150;
int bendUpPinMax = 300;

// Pitch bend reading values:
int bendDn;
int bendUp;
int pitchBend;

// Button pins:
int calibrateBtn = 8;

// Are we currently calibrating?
int calibrating = 0;

// MIDI note numbers
int notes[17];
int transposeOffset = 47;

// error variable for the exponential filter:
float error;

// Output on this MIDI channel:
int MIDIchannel = 1;

void setup() {
  // Start the serial:
  Serial.begin(9600);
  // Set multiplexer control pins pinMode:
  pinMode(muxPin1,OUTPUT);
  pinMode(muxPin2,OUTPUT);
  pinMode(muxPin3,OUTPUT);
  pinMode(muxPin4,OUTPUT);
  
  // Turn both multiplexers on:
//  digitalWrite(12, LOW);
//  digitalWrite(13, LOW);
  
  // Set calibrate button pin pinMode:
  pinMode(calibrateBtn, INPUT_PULLUP);

  // Set MIDI note numbers:
  for(int i = 0; i < 16; i++){
    notes[i] = i + transposeOffset;
  }
}

// Function for accessing individual multiplexer sensors. Input sensor number: 
int readSingleCap(int number){
  digitalWrite(muxPin1, bitRead(number, 0)); 
  digitalWrite(muxPin2, bitRead(number, 1));
  digitalWrite(muxPin3, bitRead(number, 2));
  digitalWrite(muxPin4, bitRead(number, 3));
  int value = touchRead(touchPin);
  return value;
}

// Helper function used internally in the exponential filter:
float snapCurve(float x){
  float y = 1.0 / (x + 1.0);
  y = (1.0 - y) * 2.0;
  if(y > 1.0) {
    return 1.0;
  }
  return y;
}

// Exponential filter function. Input "snapMult" = speed setting. Lower (0.001) is slower. Higher (0.1) is faster:
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

// Main loop:
void loop(){
  // Make variables for for loops:
  int i;
  int j;

  // Read calibrate button. Inverse result for "1" to be "yes":
  calibrating = !digitalRead(calibrateBtn);
  Serial.println(calibrating);  
  // Calibration function:
  if(calibrating == 1){
    // Set sensor minimum really high, and sensor maximum really low:
    for(i = 0; i < 16; i++){
      sensorMax[i] = 0;
      sensorMin[i] = 2000;
    }
    // Keep calibrating while button is pressed:
    while(calibrating == 1){
      // Read all sensors. If lower than "min" set as new min. If higher than "max" set as new max
      for(i = 0; i < 16; i++){
        sensorReadings[i] = readSingleCap(i);
        if(sensorReadings[i] < sensorMin[i]){
          sensorMin[i] = sensorReadings[i];
        }
        else if(sensorReadings[i] > sensorMax[i]){
          sensorMax[i] = sensorReadings[i];
        }
      }
      // Read button every loop:
      calibrating = !digitalRead(calibrateBtn);
    }
    // Adjust minimum and maximum values with a buffer value:
    for(i = 0; i < 16; i++){
      sensorMax[i] = sensorMax[i] - 10;
      sensorMin[i] = sensorMin[i] + 10;
    }
  }
  // Finish calibration//

  // Main for loop. "i" is the sensor number:
  for(i = 0; i < 16; i++){
    // Remember the last sensor reading at this position:
    lastSensorReadings[i] = sensorReadings[i];
    // Read sensor at this position:
    int reading = readSingleCap(i);
    // Map reading to MIDI range, and filter for stability:
    reading = map(reading, sensorMin[i], sensorMax[i], 0, 127);
    sensorReadings[i] = expFilter(reading, lastSensorReadings[i], 128, 0.01);
    
    // If sensor at this position has just been pressed, adjust the "activeSensors" array:
    if(sensorReadings[i] != 0 && lastSensorReadings[i] == 0){
      // Move all values in the array one position to the right:
      for(j = 15; j > 0; j--){
        activeSensors[j] = activeSensors[j - 1]; 
      }
      // Save current sensor number at the leftmost position ([0]):
      activeSensors[0] = i;    
      // Send note on MIDI message for the note at the current position:
      usbMIDI.sendNoteOn(notes[i], 127, MIDIchannel);  
    }

    // If sensor at current position has just been released, adjust the "activeSensor" array:
    else if(sensorReadings[i] == 0 && lastSensorReadings[i] != 0){
      // Send note off MIDI message for the note at the current position:
      usbMIDI.sendNoteOn(notes[i], 0, MIDIchannel);
      // Move all values right of the current position in the array, one position to the left:
      for(j = 0; j < 15; j++){
        if(activeSensors[j] == i){
          for(int k = j; k < 15; k++){
            activeSensors[k] = activeSensors[k + 1]; 
          }
          // Set the value at the rightmost position ([15]) to 0:
          activeSensors[15] = 0;
        }
      }
    }
    // Send volume control CC using the reading from the last activated sensor:
    usbMIDI.sendControlChange(2, sensorReadings[activeSensors[0]], MIDIchannel);
    // Print the current sensor reading values to Serial:
    Serial.print(sensorReadings[i]);
    Serial.print(" - ");
  }
  // Finish main sensor reading loop//
  Serial.println();

  // Print current "activeSensors" array to Serial: 
  for(int k = 0; k < 16; k++){
    Serial.print(activeSensors[k]);
    Serial.print(" - ");
  }
  Serial.println();
  Serial.println(millis());
//
//  // Read the extra touchRead pins, and map them to pitchBend values. Not used in the most basic keyboard model
//  bendDn = map(touchRead(bendDnPin), bendDnPinMin, bendDnPinMax, 0, 64);
//  bendUp = map(touchRead(bendUpPin), bendUpPinMin, bendUpPinMax, 0, 63);
//  // Limit values to sane range:
//  if(bendDn < 0){
//    bendDn = 0;
//  }
//  else if(bendDn > 64){
//    bendDn = 64;
//  }
//  if(bendUp < 0){
//    bendUp = 0;
//  }
//  else if(bendUp > 63){
//    bendUp = 63;
//  }
//  // Calculate final pitchBend value, as "0" plus bendup minus bend down:
//  pitchBend = 0 - bendDn + bendUp;
//  usbMIDI.sendPitchBend(pitchBend<<7, MIDIchannel);
//  
//  delay(5);
}
// End main loop//



 
