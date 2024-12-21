#include <Servo.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Wi-Fi and Firebase configuration
#define WIFI_SSID "Hara"
#define WIFI_PASSWORD "12345678"
#define API_KEY "AIzaSyBdCtdx90chwqKEqDWmNhrCdMnlFzLIg8g"
#define DATABASE_URL "https://pametna-garaza-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Pin definitions
#define echoPin D7
#define trigPin D6
#define ldrPin A0
#define ledLdrPin D4
#define servoPin D5

int zeljena_udaljenost = 25;
Servo servo;
bool servoMoved = false;
bool automatsko_zatvaranje = true;
bool upaljenoSvjetlo=false;
bool automatski_rezim=false;
// Function to update Firebase
void updateFirebase(float distance, int ldrValue, bool vrataOtvorena) {
  if (Firebase.ready() && signupOK) {
    if (!Firebase.RTDB.setFloat(&fbdo, "/senzori/udaljenost", distance)) {
      Serial.println("FAILED to send distance: " + fbdo.errorReason());
    }
    if (!Firebase.RTDB.setInt(&fbdo, "/senzori/svjetlost", ldrValue)) {
      Serial.println("FAILED to send LDR value: " + fbdo.errorReason());
    }
    if (!Firebase.RTDB.setBool(&fbdo, "/stanje/vrataOtvorena", vrataOtvorena)) {
      Serial.println("FAILED to send door state: " + fbdo.errorReason());
    }
     if (!Firebase.RTDB.setBool(&fbdo, "/stanje/upaljenoSvjetlo", upaljenoSvjetlo)) {
      Serial.println("FAILED to send door state: " + fbdo.errorReason());
    }
  }
}

void UpaliSvijetlo() {
  digitalWrite(ledLdrPin, HIGH);
  upaljenoSvjetlo=true;
}

void UgasiSvijetlo() {
  digitalWrite(ledLdrPin, LOW);
  upaljenoSvjetlo=false;
}

void OtvoriVrata() {
  for (int i = 0; i < 4; i++) {
    servo.write(180);
    delay(1000);
  }
  servo.write(90);
  servoMoved = true;
  updateFirebase(0, 0, true); // Update Firebase with door open state
}

void ZatvoriVrata() {
  for (int i = 0; i < 4; i++) {
    servo.write(0);
    delay(1000);
  }
  servo.write(90);
  servoMoved = false;
  updateFirebase(0, 0, false); // Update Firebase with door closed state
}

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ldrPin, INPUT);
  pinMode(ledLdrPin, OUTPUT);

  servo.attach(servoPin);
  servo.write(90);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Initialize Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
    signupOK = true;
  } else {
    Serial.printf("Firebase SignUp FAILED: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

}

void loop() {
  // Ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distance = (duration * 0.0343) / 2;

  // LDR sensor
  int ldrValue = analogRead(ldrPin);
  // Control light
if (Firebase.RTDB.getBool(&fbdo, "stanje/vrataOtvorena")) {
    if (fbdo.dataType() == "boolean") {
        if (fbdo.boolData() && !servoMoved) {
            // Vrata treba otvoriti
            Serial.println("Otvaranje vrata preko Firebase-a");
            OtvoriVrata();
        } else if (!fbdo.boolData() && servoMoved) {
            // Vrata treba zatvoriti
            Serial.println("Zatvaranje vrata preko Firebase-a");
            ZatvoriVrata();
        }
    }
    delay(1000); // Kašnjenje kako bi se izbjegle prečeste provjere
}
if (Firebase.RTDB.getBool(&fbdo, "stanje/upaljenoSvjetlo")) {
    if (fbdo.dataType() == "boolean") {
        if (fbdo.boolData() && !upaljenoSvjetlo) {
            Serial.println("paljenje svjetla preko Firebase-a");
            UpaliSvijetlo();
        } else if (!fbdo.boolData() && upaljenoSvjetlo) {
            Serial.println("gasenje svjetla preko Firebase-a");
            UgasiSvijetlo();
        }
    }
    delay(1000); // Kašnjenje kako bi se izbjegle prečeste provjere
}
if(Firebase.RTDB.getBool(&fbdo,"stanje/automatskoZatvaranje")){
  if(fbdo.dataType()=="boolean"){
    if(fbdo.boolData() && !automatski_rezim){
       Serial.println("Automatski rezim preko firebasea");
            automatski_rezim=true;
    }
    else if(!fbdo.boolData() && automatski_rezim){
       Serial.println("Automatski rezim preko firebaseaaaaa");
            automatski_rezim=false;
    }
  }
}
  if(automatski_rezim==true){

  if (distance <= zeljena_udaljenost + 20 && ldrValue <= 40 && !upaljenoSvjetlo) {
    UpaliSvijetlo();
  } else {
    UgasiSvijetlo();
  }

  // Control servo motor
  if (distance <= zeljena_udaljenost && !servoMoved) {
    OtvoriVrata();
  } else if (distance > zeljena_udaljenost && servoMoved && automatsko_zatvaranje) {
    ZatvoriVrata();
  }
  
  }
  // Send data to Firebase periodically
  if (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();
    updateFirebase(distance, ldrValue, servoMoved);
  }

  delay(100);
}