

int muxPin1 = 9;
int muxPin2 = 10;
int muxPin3 = 11;
int muxPin4 = 12;

int touchPin = 0;

//int enPin = 30;
//int voltPin = 31;
//int gndPin = 32;

int pluckSensors[4] = {1, 3, 4, 16};

int sensorMin[20] = {768, 768 , 768, 768, 764, 760, 755, 750, 768, 755, 762, 760, 768, 768, 768, 768, 768, 768, 768, 768};
int sensorThreshold[20] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};
int sensorMax[20] = {868, 868 , 868, 868, 864, 860, 855, 850, 868, 855, 862, 860, 868, 868, 868, 868, 868, 868, 868, 868};
int sensorReadings[20];
int lastSensorReadings[20];
int sensorActive[20];
int lastSensorActive[20];
int orgNoteValues[4] = {40, 45, 50, 55};
int noteValues[4];
int currentStringMax[4];

// Button pins:
int calibrateBtn = 8;

// Are we currently calibrating?
int calibrating = 0;


float error;

void setup() {
  Serial.begin(9600);
  pinMode(muxPin1,OUTPUT);
  pinMode(muxPin2,OUTPUT);
  pinMode(muxPin3,OUTPUT);
  pinMode(muxPin4,OUTPUT);

  pinMode(calibrateBtn, INPUT_PULLUP);
//  usbMIDI.sendControlChange(123, 0, 1);
}



float snapCurve(float x){
  float y = 1.0 / (x + 1.0);
  y = (1.0 - y) * 2.0;
  if(y > 1.0) {
    return 1.0;
  }
  return y;
}
  
int expFilter(int newValue, int lastValue, int resolution, float snapMult){
  unsigned int diff = abs(newValue - lastValue);
  error += ((newValue - lastValue) - error) * 0.4;
  float snap = snapCurve(diff * snapMult);
  float outputValue = lastValue;
  outputValue  += (newValue - lastValue) * snap;
  if(outputValue < 0.0){
    outputValue = 0.0;
  }
  else if(outputValue > resolution - 10){
    outputValue = resolution - 10;
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
    for(i = 0; i < 20; i++){
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
      for(i = 16; i < 20; i++){
        sensorReadings[i] = touchRead(pluckSensors[i - 16]);
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
    for(i = 0; i < 20; i++){
      sensorMax[i] = sensorMax[i] - 10;
      sensorMin[i] = sensorMin[i] + 10;
    }
  }
  // Finish calibration//
  
  
  
  for(i = 0; i < 16; i++){
    lastSensorReadings[i] = sensorReadings[i];
    lastSensorActive[i] = sensorActive[i];
    int tempReading = map(readSingleCap(i), sensorMin[i], sensorMax[i], 0, 127);
    sensorReadings[i] = expFilter(tempReading, lastSensorReadings[i], 128, 0.05);
    if(sensorReadings[i] > sensorThreshold[i]){
      sensorActive[i] = 1;
    }
    else{
      sensorActive[i] = 0;
    }
    Serial.print(sensorReadings[i]);
    Serial.print(" - ");
  }
  for(i = 16; i < 20; i++){
    lastSensorReadings[i] = sensorReadings[i];
    lastSensorActive[i] = sensorActive[i];
    int tempReading = map(touchRead(pluckSensors[i - 16]), sensorMin[i], sensorMax[i], 0, 127);
    sensorReadings[i] = expFilter(tempReading, lastSensorReadings[i], 128, 0.2);
    if(sensorReadings[i] > sensorThreshold[i]){
      sensorActive[i] = 1;
      if(sensorReadings[i] > currentStringMax[i - 16]){
        currentStringMax[i - 16] = sensorReadings[i];
      }
    }
    else{
      sensorActive[i] = 0;
    }
    Serial.print(sensorReadings[i]);
    Serial.print(" - ");
  }
  Serial.println();
  
  for(i = 0; i < 4; i++){
    if(sensorActive[i + 12] == 1){
      noteValues[i] = orgNoteValues[i] + 4;
    }
    else if(sensorActive[i + 8] == 1){
      noteValues[i] = orgNoteValues[i] + 3;
    }
    else if(sensorActive[i + 4] == 1){
      noteValues[i] = orgNoteValues[i] + 2;
    }
    else if(sensorActive[i] == 1){
      noteValues[i] = orgNoteValues[i] + 1;
    }
    else{
      noteValues[i] = orgNoteValues[i];
    }
    if(lastSensorActive[i + 16] == 1 && sensorActive[i + 16] == 0){
      usbMIDI.sendNoteOn(noteValues[i], currentStringMax[i], 1);
      currentStringMax[i] = 0;
    }
    if(lastSensorActive[i + 16] == 0 && sensorActive[i + 16] == 1){
      usbMIDI.sendNoteOn(noteValues[i], 0, 1);
    }
  }
//  delay(5);
}


int readSingleCap(int number){
  digitalWrite(muxPin1, bitRead(number, 0)); 
  digitalWrite(muxPin2, bitRead(number, 1)); 
  digitalWrite(muxPin3, bitRead(number, 2));
  digitalWrite(muxPin4, bitRead(number, 3));
  int value = touchRead(touchPin);
  return value;
}

