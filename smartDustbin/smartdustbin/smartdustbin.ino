#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <ESP_Mail_Client.h>
#include <Servo.h>

#define WIFI_SSID "abc" // Wifi name connected to laptop
#define WIFI_PASSWORD "12345678" // Wifi password

//Firebase HostName
#define FIREBASE_HOST "smart-dustbin-1cb4f-default-rtdb.firebaseio.com" 
//Firebase Security Key
#define FIREBASE_AUTH "wDeqZGUrbJ8e8464B2TpRyr1qXKh8sVks0o1pbbg" 

#define SMTP_HOST "smtp.gmail.com" //Email HostName
#define SMTP_PORT 465 //Port Number
#define AUTHOR_EMAIL "smartdustbinsystem@gmail.com" //Source Email Address
#define AUTHOR_PASSWORD "evfz kcmw nwfd uafn" //Source Email Password

// Ultrasonic Sensor Pins
#define TRIG_PIN1 D1 
#define ECHO_PIN1 D2
#define TRIG_PIN2 D6 
#define ECHO_PIN2 D7

// servo motor pins
#define SERVO_PIN D3

#define BUZZER_PIN D5  
#define MOISTURE_PIN A0 

SMTPSession smtp;
ESP_Mail_Session session;
SMTP_Message message;
bool trigger_Send = true;
float p_Threshold_above = 85.0;
float p_Threshold_below = 70.0;

FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

Servo wasteServo;

void setup() {
  Serial.begin(115200);

  wasteServo.attach(SERVO_PIN); // Attach servo to the pin
  wasteServo.write(90); // Default position (neutral)

  pinMode(TRIG_PIN1, OUTPUT);
  pinMode(ECHO_PIN1, INPUT);
  pinMode(TRIG_PIN2, OUTPUT);
  pinMode(ECHO_PIN2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");

  // Configure Firebase
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // SMTP session setup
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "smtp.gmail.com";
}

long measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

void sendEmail(String subject, String messageBody) {
  message.sender.name = "Smart Drainage System"; 
  message.sender.email = AUTHOR_EMAIL;
  message.subject = subject;

//Receiver Email
  message.addRecipient("Admin", "smartdustbinsystem@gmail.com"); 
  message.text.content = messageBody.c_str();

  if (!smtp.connect(&session)) {
    Serial.println("SMTP connection failed!");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Email send failed!");
  } else {
    Serial.println("Email sent successfully!");
  }
}

void loop() {
  long distance1 = measureDistance(TRIG_PIN1, ECHO_PIN1);
  long distance2 = measureDistance(TRIG_PIN2, ECHO_PIN2);
  int moistureValue = analogRead(MOISTURE_PIN);

  Serial.print("Distance 1: "); Serial.print(distance1); Serial.println(" cm");
  Serial.print("Distance 2: "); Serial.print(distance2); Serial.println(" cm");
  Serial.print("Moisture Level: "); Serial.println(moistureValue);

  String status1 = (distance1 < 7) ? "Dry Section Full!!" : "Dry Section in safe range!!";
  String status2 = (distance2 < 7) ? "Wet Section Full!!" : "Wet Section in safe range!!";
  String wasteType = (moistureValue > 700) ? "Dry Waste" : "Wet Waste";

  if (moistureValue < 700) {
    wasteServo.write(180);  // Move to the "Wet Waste" side
  } else if(moistureValue > 700 && moistureValue < 1024) {
    wasteServo.write(90); // Move to the "dry Waste" side
  } else{
    wasteServo.write(0);
  }
// Store in Firebase
  if (Firebase.RTDB.setString(&fbdo, "/SensorData/Distance1", String(distance1))) {
    Serial.println("Distance 1 stored successfully");
  } else {
    Serial.println("Error storing Distance 1: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setString(&fbdo, "/SensorData/Distance2", String(distance2))) {
    Serial.println("Distance 2 stored successfully");
  } else {
    Serial.println("Error storing Distance 2: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setString(&fbdo, "/SensorData/Moisture", String(moistureValue))) {
    Serial.println("Moisture stored successfully");
  } else {
    Serial.println("Error storing Moisture: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setString(&fbdo, "/SensorData/DryStatus", status1)) {
    Serial.println("Dry Status stored successfully");
  } else {
    Serial.println("Error storing Dry Status: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setString(&fbdo, "/SensorData/WetStatus", status2)) {
    Serial.println("Wet Status stored successfully");
  } else {
    Serial.println("Error storing Wet Status: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setString(&fbdo, "/SensorData/WasteType", wasteType)) {
    Serial.println("Waste Type stored successfully");
  } else {
    Serial.println("Error storing Waste Type: " + fbdo.errorReason());
  }

  // Trigger Buzzer & Email on threshold
  if (distance1 < 7 || distance2 < 7) {
    digitalWrite(BUZZER_PIN, HIGH);
    if (trigger_Send) {
      sendEmail("Threshold Alert!", "Warning: " + status1 + ", " + status2);
      trigger_Send = false;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("buzzer should stop running" );
    trigger_Send = true;
  }

  delay(5000);
}