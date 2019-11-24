int readPin = 14;
int MIDIout;
int lastMIDIout;

void setup(){
  Serial.begin(9600);
  // put your setup code here, to run once:

}

void loop() {
  int reading = analogRead(readPin);
  lastMIDIout = MIDIout;
  MIDIout = map(reading, 150, 35, 0, 127);
  if(MIDIout > 127){
    MIDIout = 127;
  }
  else if(MIDIout < 0){
    MIDIout = 0;
  }
  if(MIDIout != 0){
    if(lastMIDIout == 0){
      usbMIDI.sendNoteOn(40, 127, 1);
    }
    else{
      usbMIDI.sendControlChange(2, MIDIout, 1);
    }
  }
  else{
    if(lastMIDIout != 0){
      usbMIDI.sendControlChange(2, 0, 1);
      usbMIDI.sendNoteOn(40, 0, 1);
    }
  }
  delay(5);
  // put your main code here, to run repeatedly:

}
