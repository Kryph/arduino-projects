#include <Servo.h>

/* -----------------------------
   Pin and Hardware Definitions
--------------------------------*/
//Potentiometers
const int potRadiationPin = A0;

// Buttons
const int buttonOxygenBreakPin = 11;
const int buttonTimerStartPin = 10;
const int buttonTimerPausePin = 9;
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
const int geigerBuzzerPin = 12;

/* -----------------------------
   Global Variables
--------------------------------*/
int radiation = 90;

// --- Debounce ---
unsigned long debounceDelay = 100;
int lastReadingStart = LOW;
int lastReadingPause = LOW;
int lastReadingReset = LOW;
unsigned long lastChangeStart = 0;
unsigned long lastChangePause = 0;
unsigned long lastChangeReset = 0;

// Geiger Click Logic
unsigned long lastGeigerClick = 0;
unsigned long geigerPulseStart = 0;
bool geigerPulseActive = false;
const unsigned long geigerPulseLength = 2;

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

    int mappedValue = map(potRadiationValue, 0, 1023, 180, 0);
    radiation = mappedValue + randomNoise;
}

/* ----------------------------------------------------------
   Move servos
-----------------------------------------------------------*/
void moveGeiger(){
    gaussianRadiation(analogRead(potRadiationPin), 0);
    int servoAngle = radiation;
    
    servoAngle = constrain(servoAngle, 0, 180);
    servoGeiger.write(servoAngle);
}

void moveCountdown() {
    int servoAngle = map(countdownMillis, 0, resetMillis, 180, 0);
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

    // --- Start Button ---
    if (isPressed(buttonTimerStartPin, lastReadingStart, lastChangeStart)) {
        timerRunning = true;
        oxygenLeak();
        Serial.println(">> TIMER START");
        if (oxygenBottleWillBreak) {
            Serial.print("!! Oxygen bottle scheduled to break at ms: ");
            Serial.println(oxygenBreakTime);
        }
    }

    // --- Pause Button ---
    if (isPressed(buttonTimerPausePin, lastReadingPause, lastChangePause)) {
        timerRunning = false;
        oxygenBottleWillBreak = false;
        Serial.println(">> TIMER Pause (oxygen reset)");
    }

    // --- Reset Button ---
    if (isPressed(buttonTimerResetPin, lastReadingReset, lastChangeReset)) {
        countdownMillis = resetMillis;
        timerRunning = false;
        oxygenBottleWillBreak = false;
        Serial.println(">> TIMER RESET");
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

        // Ausgabe Countdown jede Sekunde
        Serial.print("Timer: ");
        Serial.print(countdownMillis / 1000);
        Serial.println("s");
    }

    // Oxygen bottle event
    if (oxygenBottleWillBreak && countdownMillis <= oxygenBreakTime) {
        countdownMillis = 0;
        timerRunning = false;
        oxygenBottleWillBreak = false;
        Serial.println("!! OXYGEN BOTTLE BROKE — TIMER FORCED TO 0");
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
    moveGeiger();
    geigerClickLogic();
}

void geigerClickLogic() {
    unsigned long now = millis();

    // --- 1) Wenn ein Klick aktiv ist → Pulse beenden ---
    if (geigerPulseActive) {
        if (now - geigerPulseStart >= geigerPulseLength) {
            noTone(geigerBuzzerPin);
            geigerPulseActive = false;
        }
        return;
    }

    // --- 2) Klick-Wahrscheinlichkeit abhängig vom radiation Wert ---
    // radiation: 0..180
    // wir mappen das auf eine Klick-Wahrscheinlichkeit pro Loop
    int probability = map(radiation, 0, 180, 1, 40);
    // Wert ist konservativ — 40 = viel Geknister

    // Jede Iteration hat eine Chance auf einen Klick
    if (random(1000) < probability) {
        // Start des Klicks
        tone(geigerBuzzerPin, 6000);  // sehr kurzer hoher Klick
        geigerPulseActive = true;
        geigerPulseStart = now;
    }
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

    pinMode(buttonTimerStartPin, INPUT);
    pinMode(buttonTimerPausePin, INPUT);
    pinMode(buttonTimerResetPin, INPUT);

    pinMode(alertBuzzerPin, OUTPUT);
    pinMode(geigerBuzzerPin, OUTPUT);

    digitalWrite(ledFinePin, LOW);
    digitalWrite(ledDangerPin, LOW);
    digitalWrite(ledAlertPin, LOW);
}

void loop() {
    countdown();
    geiger();
}
