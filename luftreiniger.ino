#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>
#include <DS1302.h>


/* -----------------------------
   Hardware Definitions
--------------------------------*/

// LCD (I2C)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buttons (HIGH = pressed)
const int buttonFillPin = 2;
const int buttonCheckPin = 3;
const int buttonRadiationPin = 4;
const int buttonBioPin = 5;

// Piezo
const int piezoPin = 6;

// 74HC595 Shift Register
const int shiftDataPin  = 7;
const int shiftClockPin = 8;
const int shiftLatchPin = 9;

// Fan [via NPN - not directly pinned]
const int fanPin = 10;

// DS1302 Real Clock
DS1302 rtc(11, 12, 13); // CLK, DAT, RST

/* -----------------------------
   Global Variables
--------------------------------*/
// Shift Register LED state
byte ledState = 0;


// Debounce
const unsigned long debounceDelay = 100;
unsigned long lastDebounceTime = 0;

// Display modes
enum DisplayMode {
    MODE_IDLE,
    MODE_RADIATION,
    MODE_BIO
};

DisplayMode currentMode = MODE_IDLE;

// Shake timing
unsigned long lastShakeTime = 0;
unsigned long nextShakeInterval = 0;

// Real Time Clock
RTC_DS3231 rtc;
#define CLK A1
#define DIO A2
TM1637Display tmDisplay(CLK, DIO);
bool rtcAvailable = true;
unsigned long startMillis = 0;

/* -----------------------------
   Custom LCD Characters
--------------------------------*/
// Full block
byte glyphSolid[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};
byte glyphScull[8] = {B00000, B01110, B10101, B11011, B01110, B01110, B01110, B00000 };

byte glyphRadHazTL[8] = { B00000, B00100, B01100, B11100, B11100, B11100, B00001, B00011 };
byte glyphRadHazTR[8] = { B00000, B00100, B00110, B00111, B00111, B00111, B10000, B11000 };
byte glyphRadHazBL[8] = { B00011, B00001, B00000, B00000, B00001, B00011, B00111, B01111 };
byte glyphRadHazBR[8] = { B11000, B10000, B00000, B00000, B10000, B11000, B11100, B11110 };

byte glyphScullTL[8] = { B00000, B10000, B11000, B01111, B00111, B01111, B01101, B01101 };
byte glyphScullTR[8] = { B00000, B00001, B00011, B11110, B11100, B11110, B10110, B10110 };
byte glyphScullBL[8] = { B00111, B00010, B00011, B00011, B00011, B00011, B00000, B00000 };
byte glyphScullBR[8] = { B11100, B01000, B11000, B11000, B11000, B11000, B00000, B00000 };
/* ----------------------------------------------------------
   Setup
-----------------------------------------------------------*/
void setup() {
    lcd.init();
    lcd.backlight();

    pinMode(buttonFillPin, INPUT_PULLUP);
    pinMode(buttonCheckPin, INPUT_PULLUP);
    pinMode(buttonRadiationPin, INPUT_PULLUP);
    pinMode(buttonBioPin, INPUT_PULLUP);

    pinMode(piezoPin, OUTPUT);
    pinMode(fanPin, OUTPUT);
    pinMode(shiftDataPin, OUTPUT);
    pinMode(shiftClockPin, OUTPUT);
    pinMode(shiftLatchPin, OUTPUT);

    lcd.createChar(0, glyphSolid);
    lcd.createChar(1, glyphScull);

    randomSeed(analogRead(A0));

    Serial.begin(9600);

    tmDisplay.setBrightness(0x0f);

    rtc.halt(false); 
    rtc.writeProtect(false);  


    nextShakeInterval = random(5000, 20000);
    bootSequence();

    lcd.print("LUFTFILTERSYSTEM");
    lcd.setCursor(0, 1);
    lcd.print("Bereit");
}


/* ----------------------------------------------------------
   Main Loop
-----------------------------------------------------------*/
void loop() {
    handleButtons();
    updateShakeEffect();
    displayTime();
}

/* ----------------------------------------------------------
   System Boot Sequence
-----------------------------------------------------------*/
void bootSequence() {
    lcd.clear();
    lcd.print("SYSTEMSTART");

    const unsigned long bootTime = 5000; 
    const int barLength = 16;          
    unsigned long startTime = millis();

    while (millis() - startTime < bootTime) {
        float progress = (float)(millis() - startTime) / bootTime;
        int filled = progress * barLength;

        lcd.setCursor(0, 1);
        for (int i = 0; i < barLength; i++) {
            if (i < filled) lcd.write(byte(0));
            else lcd.print(" ");
        }

        for (int i = 0; i < 8; i++) {
            if (random(0, 100) < 30) ledState ^= (1 << i);
        }
        updateShiftRegister();

        delay(50);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Systemstart");
    lcd.setCursor(0, 1);
    lcd.print("erfolgreich");
    for (int i = 0; i < 8; i++) {
        ledState = (1 << i);
        updateShiftRegister();
        delay(150);
    }
    clearLEDs();
}



/* ----------------------------------------------------------
   Button Handling
-----------------------------------------------------------*/
bool isPressed(int pin) {
    if (digitalRead(pin) == HIGH && millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        return true;
    }
    return false;
}

void handleButtons() {
    if (isPressed(buttonFillPin))      fillOxygen();
    if (isPressed(buttonCheckPin))     checkAir();
    if (isPressed(buttonRadiationPin)) radiationMode();
    if (isPressed(buttonBioPin))       bioMode();
}

/* ----------------------------------------------------------
   Shift Register Control
-----------------------------------------------------------*/
void updateShiftRegister() {
    digitalWrite(shiftLatchPin, LOW);
    shiftOut(shiftDataPin, shiftClockPin, MSBFIRST, ledState);
    digitalWrite(shiftLatchPin, HIGH);
}

void clearLEDs() {
    ledState = 0;
    updateShiftRegister();
}

/* ----------------------------------------------------------
   Piezo Sound Helpers
-----------------------------------------------------------*/
void beep(int frequency, int duration) {
    tone(piezoPin, frequency, duration);
    delay(duration);
    noTone(piezoPin);
}

void hum(int frequency, int duration) {
    tone(piezoPin, frequency);
    delay(duration);
    noTone(piezoPin);
}

/* ----------------------------------------------------------
   System Error Effect
-----------------------------------------------------------*/
void systemErrorScreen() {
    if (random(0, 100) >= 10) return;

    const int screenCols = 16;
    const int screenRows = 2;

    for (int t = 0; t < 12; t++) {
        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("SYSTEMFEHLER");

        lcd.setCursor(0, 1);
        for (int i = 0; i < screenCols; i++) {
            char c = 32 + random(0, 95);  // ASCII 32–126 (druckbare Zeichen)
            lcd.print(c);
        }

        delay(100);
    }


    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SYSTEMFEHLER");
    lcd.setCursor(0, 1);
    lcd.print("!!! KRITISCH !!!");
    delay(2000);

    switch(currentMode) {
        case MODE_IDLE:
            lcd.print("LUFTFILTERSYSTEM");
            lcd.setCursor(0, 1);
            lcd.print("Bereit");
            break;

        case MODE_RADIATION:
            lcd.createChar(2, glyphRadHazTL);
            lcd.createChar(3, glyphRadHazTR);
            lcd.createChar(4, glyphRadHazBL);
            lcd.createChar(5, glyphRadHazBR);

            lcd.setCursor(0, 0);
            lcd.print("MODUS:");
            lcd.setCursor(14, 0);
            lcd.write(byte(2));
            lcd.write(byte(3));

            lcd.setCursor(0, 1);
            lcd.print("Strahlung");
            lcd.setCursor(14, 1);
            lcd.write(byte(4));
            lcd.write(byte(5));
            break;

        case MODE_BIO:
            lcd.createChar(2, glyphScullTL);
            lcd.createChar(3, glyphScullTR);
            lcd.createChar(4, glyphScullBL);
            lcd.createChar(5, glyphScullBR);

            lcd.setCursor(0, 0);
            lcd.print("MODUS:");
            lcd.setCursor(14, 0);
            lcd.write(byte(2));
            lcd.write(byte(3));

            lcd.setCursor(0, 1);
            lcd.print("Biogefahr");
            lcd.setCursor(14, 1);
            lcd.write(byte(4));
            lcd.write(byte(5));
            break;
    }

/* ----------------------------------------------------------
   Oxygen Fill Logic (LEDs 0–2)
-----------------------------------------------------------*/
void fillOxygen() {
    currentMode = MODE_IDLE;
    systemErrorScreen();

    lcd.print("FUELLE O2");
    lcd.setCursor(0, 1);
    lcd.print("AKTIV");
    digitalWrite(fanPin, HIGH);

    for (int round = 0; round < 6; round++) {
        for (int i = 0; i < 3; i++) {
            ledState = (1 << i);
            updateShiftRegister();
            hum(90 + i * 30, 140);
        }
    }

    digitalWrite(fanPin, LOW);
    clearLEDs();
    lcd.clear();
    
    lcd.print("O2 TANKS");
    lcd.setCursor(0, 1);
    lcd.print("GEFUELLT");

    beep(1200, 200);
    delay(1200);
    lcd.clear();
}

/* ----------------------------------------------------------
   Air Quality Check (LEDs 3–7)
-----------------------------------------------------------*/
void checkAir() {
    currentMode = MODE_IDLE;
    systemErrorScreen();

    lcd.print("ANALYSE DES");
    lcd.setCursor(0, 1);
    lcd.print("FLASCHENINHALTES");

    for (int round = 0; round < 5; round++) {
        for (int i = 3; i < 8; i++) {
            ledState = (1 << i);
            updateShiftRegister();
            beep(800 - i * 50, 60);
        }
    }

    clearLEDs();
    lcd.clear();

    lcd.setCursor(0, 0);  // Zeile 1, Spalte 0
    lcd.print("LUFT:");
    int result = random(0, 3);
    if (result == 0) lcd.print("OPTIMAL");
    if (result == 1) lcd.print("OK");
    if (result == 2) lcd.print("LEICHT TOXISCH");

    beep(result == 2 ? 300 : 1000, 300);
    delay(1600);
    lcd.clear();
}

/* ----------------------------------------------------------
   Radiation Regulation Mode
-----------------------------------------------------------*/
void radiationMode() {
    currentMode = MODE_RADIATION;
    systemErrorScreen();

    lcd.print("ZONENREGELUNG");
    lcd.setCursor(0, 1);
    lcd.print("STRAHLUNG");

    for (int round = 0; round < 6; round++) {
        for (int i = 3; i < 8; i++) {
            ledState = (1 << i);
            updateShiftRegister();
            hum(70, 70);
        }
    }

    clearLEDs();
    lcd.clear();
    
    lcd.createChar(2, glyphRadHazTL);
    lcd.createChar(3, glyphRadHazTR);
    lcd.createChar(4, glyphRadHazBL);
    lcd.createChar(5, glyphRadHazBR);

    lcd.setCursor(0, 0);
    lcd.print("MODUS:");  
    lcd.setCursor(14, 0);
    lcd.write(byte(2));
    lcd.write(byte(3));    

    lcd.setCursor(0, 1);
    lcd.print("Strahlung");
    lcd.setCursor(14, 1);
    lcd.write(byte(4));
    lcd.write(byte(5));
}

/* ----------------------------------------------------------
   Biohazard Regulation Mode
-----------------------------------------------------------*/
void bioMode() {
    currentMode = MODE_BIO;
    systemErrorScreen();

    lcd.print("ZONENREGELUNG");
    lcd.setCursor(0, 1);
    lcd.print("BIOGEFAHR");

    for (int round = 0; round < 6; round++) {
        for (int i = 3; i < 8; i++) {
            ledState = (1 << i);
            updateShiftRegister();
            hum(70, 70);
        }
    }

    lcd.clear();

    
    lcd.createChar(2, glyphScullTL);
    lcd.createChar(3, glyphScullTR);
    lcd.createChar(4, glyphScullBL);
    lcd.createChar(5, glyphScullBR);

    lcd.setCursor(0, 0);
    lcd.print("MODUS:");  
    lcd.setCursor(14, 0);
    lcd.write(byte(2));
    lcd.write(byte(3));    

    lcd.setCursor(0, 1);
    lcd.print("Biogefahr");
    lcd.setCursor(14, 1);
    lcd.write(byte(4));
    lcd.write(byte(5));
}

/* ----------------------------------------------------------
   Endscreen Shake Effect
-----------------------------------------------------------*/
void updateShakeEffect() {
    if (millis() - lastShakeTime >= nextShakeInterval) {
        lastShakeTime = millis();
        nextShakeInterval = random(5000, 20000); // nächstes Intervall zufällig 5–20 s
        shakeDisplay(); // der Textzittern-Effekt
    }
}

void updateFlicker() {
    int offset = random(-1, 2);
    lcd.clear(); 

    switch(currentMode) {
        case MODE_IDLE:
            lcd.setCursor(max(0, 0 + offset), 0);
            lcd.print("LUFTFILTERSYSTEM");
            lcd.setCursor(max(0, 0 + offset), 1);
            lcd.print("Bereit");
            break;

        case MODE_RADIATION:
            lcd.createChar(2, glyphRadHazTL);
            lcd.createChar(3, glyphRadHazTR);
            lcd.createChar(4, glyphRadHazBL);
            lcd.createChar(5, glyphRadHazBR);

            lcd.setCursor(max(0, 0 + offset), 0);
            lcd.print("MODUS:");
            lcd.setCursor(min(15, 14 + offset), 0);
            lcd.write(byte(2));
            lcd.write(byte(3));

            lcd.setCursor(max(0, 0 + offset), 1);
            lcd.print("Strahlung");
            lcd.setCursor(min(15, 14 + offset), 1);
            lcd.write(byte(4));
            lcd.write(byte(5));
            break;

        case MODE_BIO:
            lcd.createChar(2, glyphScullTL);
            lcd.createChar(3, glyphScullTR);
            lcd.createChar(4, glyphScullBL);
            lcd.createChar(5, glyphScullBR);

            lcd.setCursor(max(0, 0 + offset), 0);
            lcd.print("MODUS:");
            lcd.setCursor(min(15, 14 + offset), 0);
            lcd.write(byte(2));
            lcd.write(byte(3));

            lcd.setCursor(max(0, 0 + offset), 1);
            lcd.print("Biogefahr");
            lcd.setCursor(min(15, 14 + offset), 1);
            lcd.write(byte(4));
            lcd.write(byte(5));
            break;
    }
}

void displayTime() {
    int displayTime;

    if (rtc.isHalted()) {
        unsigned long elapsedSeconds = millis() / 1000;
        int minutes = elapsedSeconds % 60;
        int hours = (elapsedSeconds / 60) % 24;
        displayTime = hours * 100 + minutes;
    } else {
        Time t = rtc.time();
        displayTime = t.hr * 100 + t.min;
    }

    tmDisplay.showNumberDecEx(displayTime, 0b01000000, true);
}


}
