#include <Stepper.h>
#include <Bonezegei_DHT11.h>
#include <Servo.h>

int button_state = 0;
int current_button_state = 0;
int last_button_state = 0;
int led_timer = 100;

const int buzzer_pin = 6;
const int taster_pin = 13;
Bonezegei_DHT11 dht(12);

const int number_of_leds = 4;
const long led_interval = 100; // Zeit zwischen LED-Wechseln in ms

int currentLED = 2;                     // start LED
unsigned long previousMillisLED = 0;    // letzte Zeit, als LED gewechselt wurde

int potentiometer_pin= A0;
int potentiometer_value = 0;

Servo myservo;
int servo_pos = 0;
const int servo_pin = 7;

void runningLightSetup(int number_of_led) {
  for (int LED_PIN = 2; LED_PIN <= (2 + number_of_led); LED_PIN++) {
  pinMode(LED_PIN, OUTPUT);
  }
}

void runningLightMillis() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisLED >= led_interval) {
    previousMillisLED = currentMillis;
    for (int i = 2; i < 2 + number_of_leds; i++) {
      digitalWrite(i, LOW);
    }
    digitalWrite(currentLED, HIGH);
    // analogWrite(currentLED, map(potentiometer_value,0,1023,0,255));
    currentLED++;
    if (currentLED >= 2 + number_of_leds) currentLED = 2;
  }
}

void runningLight(int number_of_led){
  for (int LED_PIN = 2; LED_PIN <= (2 + number_of_led); LED_PIN++) {
  digitalWrite(LED_PIN, HIGH);
  delay(led_timer);
  digitalWrite(LED_PIN, LOW);
  delay(led_timer);
  }
}

void read_dht11() {
  if (dht.getData()) {
    float tempDeg = dht.getTemperature();      // Temperatur in °C
    int hum = dht.getHumidity();               // Luftfeuchtigkeit

    Serial.print("Temperature: ");
    Serial.print(tempDeg);
    Serial.print("°C  Humidity: ");
    Serial.print(hum);
    Serial.println("%");
  } 
  else {
    Serial.println("Fehler: Konnte DHT11-Daten nicht auslesen!");
  }
}

void buzzer_flank_detected(){
  //passiver
  tone(buzzer_pin, 5000); // Sende KHz Tonsignal
  delay(50); // 
  noTone(buzzer_pin); // Ton stoppen

  // // aktiver
  // digitalWrite(buzzer_pin,HIGH); // Ton an
  // delay(50);
  // digitalWrite(buzzer_pin,LOW); // Ton aus
}

int gaussianNoise(int range) {
  // erzeugt glockenförmiges Rauschen von ungefähr -range bis +range
  long sum = 0;

  // 6 Zufallswerte addieren → ergibt glockenförmige Verteilung
  for (int i = 0; i < 6; i++) {
    sum += random(-range, range + 1);
  }

  return sum / 6;  // Mittelwert → „Gaussianähnlich“
}


void servo_to_pont(int potentiometer_value){
  // Potentiometer (0–1023) → Servo-Winkel (-180 bis +180)
  int angle = map(potentiometer_value, 0, 1023, -180, 180);
  // Für den Servo (0–180 Grad):
  int servo_angle = map(angle, -180, 180, 0, 180);
  int noise = gaussianNoise(20);
  servo_angle += noise;

  // Sicherheitsbegrenzung:
  servo_angle = constrain(servo_angle, 0, 180);
  myservo.write(servo_angle);
  delay(15); // kleine Stabilisierung

  // for (pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
  //   // in steps of 1 degree
  //   myservo.write(pos);              // tell servo to go to position in variable 'pos'
  //   delay(15);                       // waits 15 ms for the servo to reach the position
  // }
  // for (pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
  //   myservo.write(pos);              // tell servo to go to position in variable 'pos'
  //   delay(15);                       // waits 15 ms for the servo to reach the position
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  myservo.attach(servo_pin);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(taster_pin, INPUT);
  runningLightSetup(4);
}

void loop() {

  potentiometer_value =analogRead(potentiometer_pin);
  servo_to_pont(potentiometer_value);
  // Serial.println(potentiometer_value);
  current_button_state = digitalRead(taster_pin);
  // if (current_button_state == HIGH){
  //     Serial.println("High erkannt!");
  // }

  if (current_button_state == HIGH && last_button_state != current_button_state) {
    button_state = (button_state + 1) % 2;
    Serial.println("Flanke erkannt! button_state gewechselt");
    buzzer_flank_detected();
  }

  last_button_state = current_button_state;

  if (button_state == 1){
    read_dht11();
    // runningLight(4);
    runningLightMillis(); // nicht-blockierendes Lauflicht
  }
  else {
    for (int i = 2; i < 2 + number_of_leds; i++) {
      digitalWrite(i, LOW);
  }
}

}