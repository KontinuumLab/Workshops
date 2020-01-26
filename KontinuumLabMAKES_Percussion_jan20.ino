//#include <EEPROMex.h>

// Main sensor pins
int sensorPins[4] = {16, 17, 18, 19};

// Button pins:
int calibrateBtn = 8;
// Binary value for calibration on/off:
int calibrating = 0;

// Arrays for raw sensor readings, output values and for remembering last output values:
int sensorReadings[4];
int sensorOut[4];
int lastSensorOut[4];

// Arrays for the velocity values for each sensor:
int sensorVelocity[4];
int preparingVel[4] = {0, 0, 0, 0};

// Initial minimum and maximum values, and velocity threshold, for each sensor:
int sensorMax[4] = {250, 275, 225, 250};
int sensorVelMin[4] = {150, 175, 125, 150};
int sensorMin[4] = {60, 60, 60, 60};
long unsigned velocityTimer[4] = {0, 0, 0, 0};


// MIDI note numbers, channel, and #cc for each sensor:
int midiNote[4] = {35, 38, 42, 45};
int MIDIchannel[4] = {4, 5, 6, 7};
int continuousController[4] = {2, 2, 2, 2};

// Error variable for the exponential filter:
float error;



void setup() {
  // Start the serial connection:
  Serial.begin(9600);
  // Set the calibration butten pinMode:
  pinMode(calibrateBtn, INPUT_PULLUP);
//  EEPROM.setMemPool(memBase, EEPROMSizeUno);
}


// Helper function for the exponential filter:
float snapCurve(float x){
  float y = 1.0 / (x + 1.0);
  y = (1.0 - y) * 2.0;
  if(y > 1.0) {
    return 1.0;
  }
  return y;
}

// Main exponential filter function. Input "snapMult" = filter speed. 0.001 = slow / 0.1 = fast.
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
  // Initiate variable for for-loops:
  int i;
  // Read calibrate button. Inverse result for "1" to be "yes":
  calibrating = !digitalRead(calibrateBtn);
  Serial.println(calibrating);
  // Calibration function:
  if(calibrating == 1){
    // Set sensor minimum really high, and sensor maximum really low:
    for(i = 0; i < 4; i++){
      sensorMax[i] = analogRead(sensorPins[i]);
      sensorMin[i] = 2000;
    }
    // Keep calibrating while button is pressed:
    while(calibrating == 1){
      // Read all sensors. If lower than "min" set as new min. If higher than "max" set as new max
      for(i = 0; i < 4; i++){
        sensorReadings[i] = analogRead(sensorPins[i]);
        if(sensorReadings[i] < sensorMin[i]){
          sensorMin[i] = sensorReadings[i];
        }
//        else if(sensorReadings[i] > sensorMax[i]){
//          sensorMax[i] = sensorReadings[i];
//        }
      }
     // Read button every loop:
     calibrating = !digitalRead(calibrateBtn);
    }
    // Adjust minimum and maximum values with a buffer value:
    for(i = 0; i < 4; i++){
      sensorMax[i] = sensorMax[i] - 10;
      sensorMin[i] = sensorMin[i] + 10;
      sensorVelMin[i] = sensorMax[i] - 100;
    }
  }
  // Finish calibration//

  // Main for loop. "i" is the sensor number:
  for(i = 0; i < 4; i++){
    // Remember the last sensor output:
    lastSensorOut[i] = sensorOut[i];
    // read the sensor at the current position:
    sensorReadings[i] = analogRead(sensorPins[i]);
    // map the reading to MIDI range, and filter for smoothness:
    sensorOut[i] = map(sensorReadings[i], sensorMax[i], sensorMin[i], 0, 127);
    sensorOut[i] = expFilter(sensorOut[i], lastSensorOut[i], 128, 0.01);
    // Limit to MIDI range:
    if(sensorOut[i] < 0){
      sensorOut[i] = 0;
    }
    else if(sensorOut[i] > 127){
      sensorOut[i] = 127;
    }
    Serial.print(sensorOut[i]);
    Serial.print(" - ");
    // Compare sensor output to previous value. If new value at current 
    // position, Send CC message for volume control:
    if(sensorOut[i] != lastSensorOut[i]){ 
//      Serial.println(sensorOut[i]);
      usbMIDI.sendControlChange(continuousController[i], sensorOut[i], MIDIchannel[i]);
      // Check for recently activated sensor. If activated in this loop,
      // activate "preparingVel" at this position, and save current time:
      if(lastSensorOut[i] == 0 && sensorOut[i] != 0){
        preparingVel[i] = 1;
      }
      // If preparingVel was activated at current position but in a 
      // previous loop, calculate time since activation:
      else if(preparingVel[i] == 1){
      // Calculate, map and limit velocity,
      // and then send a "note on" message with the values from current pos:
        int vel = analogRead(sensorPins[i]);
        sensorVelocity[i] = sensorOut[i];
//        if(sensorVelocity[i] < 0){
//          sensorVelocity[i] = 0;
//        }
//        else if(sensorVelocity[i] > 127){
//          sensorVelocity[i] = 127;
//        }
          Serial.print("noteOn: ");
          Serial.print(i);
          Serial.print(" - ");
          Serial.println(sensorVelocity[i]);
        usbMIDI.sendNoteOn(midiNote[i], sensorVelocity[i], MIDIchannel[i]);
        preparingVel[i] = 0;
//          delay(1000);
        }
      // If sensor at current position has been deactivated during current loop,
      // Send "note off" message with values from current position:
      if(sensorOut[i] == 0 && lastSensorOut[i] != 0){
        Serial.print("noteOff - ");
        Serial.print("Last noteOn: ");
        Serial.println(sensorVelocity[i]);
        usbMIDI.sendNoteOn(midiNote[i], 0, MIDIchannel[i]);
//        delay(1000);
      }
    }
  }
  Serial.println();
  // End main for loop//
//  delay(1);
//  Serial.println();
}
