
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <SPI.h>
#include <MFRC522.h>

// Wi-Fi credentials
#define WIFI_SSID "Abhay"
#define WIFI_PASSWORD "Rahul123"

// RFID pins
#define RST_PIN 22
#define SS_PIN 5

// Hardware pins
#define IR_SENSOR_PIN 34
#define GREEN_LED 14
#define RED_LED 27
#define BUZZER 26

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

MFRC522 rfid(SS_PIN, RST_PIN);  // RFID instance

// ✅ Authorized tags
struct AccessTag {
  byte uid[4];
  String type;
};

AccessTag authorizedTags[] = {
  { {0x93, 0xB4, 0xA9, 0x29}, "Card" },
  { {0xF3, 0x36, 0x55, 0x14}, "Key" }
};

const int totalTags = sizeof(authorizedTags) / sizeof(authorizedTags[0]);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  // Firebase configuration
  config.api_key = "AIzaSyCfloCGr4MVJJ78ALSS8sCEtTi62xrpRgw";
  config.database_url = "https://smart-lock-c33fb-default-rtdb.firebaseio.com";

  // Optional: Use Firebase secret if you're using legacy auth
  auth.user.email = "";
  auth.user.password = "";
  config.signer.tokens.legacy_token = "OhJXuuus1wu3a2uTdpzGEiRIFsU8xN1rgSR3eayc";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Smart Locker System Ready");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String accessType;
  Serial.print("Scanned UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (isAuthorized(rfid.uid.uidByte, accessType)) {
    Serial.println("✅ Access Granted (" + accessType + ")");
    grantAccess(accessType, rfid.uid.uidByte);
  } else {
    denyAccess(rfid.uid.uidByte);
  }

  rfid.PICC_HaltA(); // Stop reading
}

bool isAuthorized(byte *uid, String &typeOut) {
  for (int j = 0; j < totalTags; j++) {
    bool match = true;
    for (byte i = 0; i < 4; i++) {
      if (uid[i] != authorizedTags[j].uid[i]) {
        match = false;
        break;
      }
    }
    if (match) {
      typeOut = authorizedTags[j].type;
      return true;
    }
  }
  return false;
}

void grantAccess(String accessType, byte *uid) {
  Serial.println("Access by: " + accessType);
  digitalWrite(GREEN_LED, HIGH);
  tone(BUZZER, 1000, 200);
  delay(3000);
  digitalWrite(GREEN_LED, LOW);

  // Log to Firebase
  String path = "/accessLogs/" + String(millis());
  Firebase.setString(fbdo, path + "/uid", getUIDString(uid));
  Firebase.setString(fbdo, path + "/type", accessType);
  Firebase.setString(fbdo, path + "/status", "Granted");

  // Check IR sensor
  Serial.println("Checking locker access...");
  bool activity = false;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(IR_SENSOR_PIN) == HIGH) {
      activity = true;
      break;
    }
    delay(1000);
  }

  if (activity) {
    Serial.println("Locker was accessed.");
    tone(BUZZER, 1500, 100);
  } else {
    Serial.println("⚠️ Locker not accessed!");
    for (int i = 0; i < 3; i++) {
      digitalWrite(RED_LED, HIGH);
      tone(BUZZER, 500, 300);
      delay(300);
      digitalWrite(RED_LED, LOW);
      delay(300);
    }
  }
}

void denyAccess(byte *uid) {
  Serial.println("❌ Access Denied");
  digitalWrite(RED_LED, HIGH);
  tone(BUZZER, 500, 500);
  delay(1000);
  digitalWrite(RED_LED, LOW);

  // Log denied attempt
  String path = "/accessLogs/" + String(millis());
  Firebase.setString(fbdo, path + "/uid", getUIDString(uid));
  Firebase.setString(fbdo, path + "/status", "Denied");
}

String getUIDString(byte *uid) {
  String uidString = "";
  for (byte i = 0; i < 4; i++) {
    if (uid[i] < 0x10) uidString += "0";
    uidString += String(uid[i], HEX);
  }
  return uidString;
}
