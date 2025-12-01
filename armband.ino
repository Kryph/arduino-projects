#include <Servo.h>

/* -----------------------------
   Pin and Hardware Definitions
--------------------------------*/
//Potentiometers
const int potVolumePin = 13;
const int potRadiationPin = 12;

// Buttons
//const int buttonGeigerPin = 11;
const int buttonTimerStartPin = 10;
const int buttonTimerStopPin = 9;
const int buttonTimerResetPin = 8;

//Servo
Servo servoGeiger;
Servo servoTimer;
const int servoGeigerPin = 7;
const int servoTimerPin = 6;

//LEDs
const int ledFinePin = 5;
const int ledDangerPin = 4;
const int ledAlertPin = 3;

//Buzzer
const int alertBuzzerPin = 2;

/* -----------------------------
   Global Variables
--------------------------------*/
int radiation = 90;

// --- Debounce ---
unsigned long debounceDelay = 50;
unsigned long lastGeigerChange = 0;
unsigned long lastStartChange = 0;
unsigned long lastStopChange = 0;
unsigned long lastResetChange = 0;

// Geiger
// int buttonGeigerState = 0;
int buttonLastReading = LOW;

// Countdown
unsigned long countdownMillis = 1200000UL;
unsigned long lastUpdateMillis = 0;
const unsigned long interval = 1000;
bool timerRunning = false;
const unsigned long resetMillis = 1200000UL;
unsigned long lastAlertBlinkTime = 0;
bool alertLedState = false;

// Buzzer timing (non-blocking)
unsigned long lastBeepTime = 0;
bool buzzerState = false;

//Oxygen Bottle Health Check
bool oxygenBottleWillBreak = false;
unsigned long oxygenBreakTime = 0;


/* ----------------------------------------------------------
   Simple Gaussian-like radiation generator
-----------------------------------------------------------*/
void gaussianRadiation(int potRadiationValue, int range) {
    long sum = 0;
    for (int i = 0; i < 6; i++) {
        sum += random(-range, range + 1);
    }
    int randomNoise = sum / 6;

    int mappedValue = map(potRadiationValue, 0, 1023, 0, 180);
    radiation = mappedValue + randomNoise;
}

/* ----------------------------------------------------------
   Move servos
-----------------------------------------------------------*/
void moveGeiger() {
    int servoAngle = radiation;
    servoAngle = constrain(servoAngle, 0, 180);
    servoGeiger.write(servoAngle);
}

void moveCountdown() {
    int servoAngle = map(countdownMillis, 0, 1200000UL, 0, 180);
    servoAngle = constrain(servoAngle, 0, 180);
    servoTimer.write(servoAngle);
}

/* ----------------------------------------------------------
   Debounced Button Check
-----------------------------------------------------------*/
bool isPressed(int pin, int &lastReading, unsigned long &lastChange) {
    int reading = digitalRead(pin);
    unsigned long now = millis();

    if (reading != lastReading) {
        lastChange = now;
    }

    lastReading = reading;

    if (now - lastChange > debounceDelay && reading == HIGH) {
        return true;
    }

    return false;
}

/* ----------------------------------------------------------
   Oxygen Logic
-----------------------------------------------------------*/
void oxygenLeak(){
    if (!oxygenBottleWillBreak){
        if (random(100) < 20) {
            oxygenBottleWillBreak = true;
            oxygenBreakTime = random( (resetMillis * 0.25), (resetMillis * 0.75) );
    }
}



/* ----------------------------------------------------------
   Countdown Logic
-----------------------------------------------------------*/

void handleBuzzer() {
    if (countdownMillis > 60000UL) {
        noTone(alertBuzzerPin);
        buzzerState = false;
        return;
    }

    unsigned long now = millis();
    if (now - lastBeepTime >= 500) {
        lastBeepTime = now;
        buzzerState = !buzzerState;

        if (buzzerState) tone(alertBuzzerPin, 5000);
        else noTone(alertBuzzerPin);
    }
}

void handleAlertLed() {
    unsigned long now = millis();

    if (countdownMillis <= 60000UL) {
        if (now - lastAlertBlinkTime >= 500) {
            lastAlertBlinkTime = now;
            alertLedState = !alertLedState;
            digitalWrite(ledAlertPin, alertLedState ? HIGH : LOW);
        }
    } 
    else {
        digitalWrite(ledAlertPin, LOW);
        alertLedState = false;
    }
}

void countdown() {
    unsigned long now = millis();

    if (isPressed(buttonTimerStartPin, buttonLastReading, lastStartChange)) {
        timerRunning = true;
        oxygenLeak();
    }
    if (isPressed(buttonTimerStopPin, buttonLastReading, lastStopChange)) {
        timerRunning = false;
        oxygenBottleWillBreak = false;
    }
    if (isPressed(buttonTimerResetPin, buttonLastReading, lastResetChange)) {
        countdownMillis = resetMillis;
        timerRunning = false;
        oxygenBottleWillBreak = false;
    }

    // --- Timer Core ---
    if (timerRunning && now - lastUpdateMillis >= interval) {
        lastUpdateMillis = now;

        if (countdownMillis >= 1000) {
            countdownMillis -= 1000;
        } else {
            countdownMillis = 0;
            timerRunning = false;
        }
    }

    if (oxygenBottleWillBreak && countdownMillis <= oxygenBreakTime) {
        countdownMillis = 0;
        timerRunning = false;
        oxygenBottleWillBreak = false;
    }

    moveCountdown();

    // LEDs
    if (countdownMillis > 10UL * 60UL * 1000UL) {
        digitalWrite(ledFinePin, HIGH);
        digitalWrite(ledDangerPin, LOW);
    }
    else if (countdownMillis > 1UL * 60UL * 1000UL) {
        digitalWrite(ledFinePin, LOW);
        digitalWrite(ledDangerPin, HIGH);
    }
    else {
        digitalWrite(ledFinePin, LOW);
        digitalWrite(ledDangerPin, LOW);
    }

    // Buzzer separat, non-blocking
    handleBuzzer();
    handleAlertLed();
}

/* ----------------------------------------------------------
   Geiger Logic
-----------------------------------------------------------*/
void geiger() {
    // if (isPressed(buttonGeigerPin, buttonLastReading, lastGeigerChange)) {
    //     buttonGeigerState = (buttonGeigerState + 1) % 2;
    // }
    moveGeiger();
}

/* ----------------------------------------------------------
   Main
-----------------------------------------------------------*/
void setup() {
    Serial.begin(9600);

    servoGeiger.attach(servoGeigerPin);
    servoTimer.attach(servoTimerPin);

    pinMode(ledFinePin, OUTPUT);
    pinMode(ledDangerPin, OUTPUT);
    pinMode(ledAlertPin, OUTPUT);

    pinMode(buttonGeigerPin, INPUT);
    pinMode(buttonTimerStartPin, INPUT);
    pinMode(buttonTimerStopPin, INPUT);
    pinMode(buttonTimerResetPin, INPUT);

    pinMode(alertBuzzerPin, OUTPUT);

    digitalWrite(ledFinePin, LOW);
    digitalWrite(ledDangerPin, LOW);
    digitalWrite(ledAlertPin, LOW);
}

void loop() {
    countdown();
    geiger();
}