#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// Pins für SoftwareSerial
const int RX_PIN = 10; // Arduino empfängt von DFPlayer TX
const int TX_PIN = 11; // Arduino sendet an DFPlayer RX

SoftwareSerial mySerial(RX_PIN, TX_PIN);
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  Serial.println("=== DFPlayer Mini Test ===");
  delay(5000);
  // DFPlayer initialisieren
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("Error: DFPlayer konnte nicht initialisiert werden!");
    while(true); // Stop, wenn Player nicht gefunden
  }
  Serial.println("DFPlayer initialisiert [OK]");

  // Lautstärke testen
  myDFPlayer.volume(20); // 0-30
  Serial.println("Lautstärke auf 30 gesetzt");

  // Karte prüfen
  delay(2000);
  if (myDFPlayer.readType() == DFPlayerCardInserted) {
    Serial.println("SD-Karte erkannt!");
  } else {
    Serial.println("Keine SD-Karte oder Karte leer!");
  }

  // Test: erste Datei abspielen
  myDFPlayer.play(2);
  Serial.println("Spiele Datei 1 ab...");
}

void loop() {
  // Prüfen, ob MP3 fertig abgespielt
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    int value = myDFPlayer.read();

    if (type == DFPlayerPlayFinished) {
      Serial.println("MP3 fertig abgespielt!");
    } else if (type == DFPlayerError) {
      Serial.print("DFPlayer Fehler: ");
      Serial.println(value);
    }
  }
}
