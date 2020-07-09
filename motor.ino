`/*
 * 
 * Created by bkbilly
 * 
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include "secrets.h"

#define mqtt_subscribe_topic "smartlock/+/set"

// define Motor Vars
const int motor1Pin1 = 26;
const int motor1Pin2 = 27;
String motor_status = "stopped";

// define Motor Encoder Vars
const int ENCPIN_Y = 15;
const int ENCPIN_G = 13;
volatile long int encoder_pos = 0;
volatile long int prev_encoder_pos = 0;
String encoder_status = "stopped";
unsigned long encoder_timer;

// define MQTT messages
char topic_status[] = "smartlock/status";                 //message=['online']
char set_topic_door[] = "smartlock/door/set";

// define Globals
String door_state;
WiFiClient espClient;
PubSubClient client;


void setup() {
  // Serial init
  Serial.begin(115200);   // Initiate a serial communication
  Serial.println("Booting");

  // Timers init
  encoder_timer = millis();


  // Motor init
  pinMode(ENCPIN_Y, INPUT);
  pinMode(ENCPIN_G, INPUT);
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(ENCPIN_Y), encoder, RISING);

  wifi_init();
  mqtt_init();
  ota_init();
}

void loop() {
  client.loop();
  if (!client.connected()) {
    mqtt_reconnect();
  }

  // Stop motor when key has reached the end for 1 sec
  if (millis() - encoder_timer >= 1 * 1000 && encoder_status != "stopped"){
    if (encoder_pos == prev_encoder_pos) {
      encoder_status = "stopped";
      if (motor_status != "stopped") {
        stopMotor();
      }
    }
  }

}

//////////////////////////////////////// MOTOR //////////////////////////////

void turnMotor(String moto_dir){
  Serial.print("Encoder status: ");
  Serial.println(encoder_pos);
  if (moto_dir == "unlock") {
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH);
    motor_status = "unlocking";
    door_state = "unlocked";
  } else if (moto_dir == "lock") {
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);
    motor_status = "locking";
    door_state = "locked";
  }
  // delay(10000);
  // stopMotor();
  // Serial.println(door_state);
}

void stopMotor(){
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  motor_status = "stopped";
  Serial.print("Motor has stopped: ");
  Serial.print("Encoder status: ");
  Serial.println(encoder_pos);
}


void encoder(){
  if(digitalRead(ENCPIN_G) == HIGH){
    encoder_pos++;
    encoder_status = "go_up";
  }else{
    encoder_pos--;
    encoder_status = "go_down";
  }
  encoder_timer = millis();
}


//////////////////////////////////////// MQTT //////////////////////////////

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      client.publish(topic_status,"connected");
      client.subscribe(mqtt_subscribe_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqtt_callback(char* topic_char, byte* payload, unsigned int length) {
  String topic = topic_char;
  payload[length] = '\0';
  String message = (char*)payload;
  message.toUpperCase();
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  // ##### Other options #####  
  if (topic == set_topic_door) {
    if (message == "UNLOCK") {
      turnMotor("unlock");
    } else if (message == "LOCK"){
      turnMotor("lock");
    }
  }

  else {
    Serial.println("something went wrong. the topic can't be subscribed");
  }

}


//////////////////////////////////////// INIT //////////////////////////////
void wifi_init() {

  // WiFi init
  WiFi.begin(ssid, password);
  delay(10);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_init(){
  // MQTT init
  client.setClient(espClient);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  if (client.connect("arduinoClient", mqtt_user, mqtt_pass)) {
    client.publish(topic_status, "connected");
    client.subscribe(mqtt_subscribe_topic);
  }
}

void ota_init() {
  // OTA init
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
   ArduinoOTA.setHostname("smartlock");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
}
