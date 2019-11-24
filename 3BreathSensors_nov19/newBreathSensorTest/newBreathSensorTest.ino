int readPin = 14;


void setup() {
  Serial.begin(9600);
  // put your setup code here, to run once:

}

void loop() {
  Serial.println(analogRead(readPin));
  delay(5);
  // put your main code here, to run repeatedly:

}
