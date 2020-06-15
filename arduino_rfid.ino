/*
 * 
 * Created by bkbilly
 * https://github.com/omersiar/RFID522-Door-Unlock/blob/master/EEPROM/EEPROM.ino
 * 
 * 
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include "secrets.h"

#define RFID_SS_PIN 21
#define RFID_RST_PIN 22
#define uid_size 7          // Usually uid is either 4 or 7 bytes
#define rfid_timout 10      // Seconds to timeout for adding/deleting RFIDs
#define mqtt_subscribe_topic "smartlock/+/set"
#define unlock_timout 3     // Seconds to timeout for letting the door unlocked

volatile long int encoder_pos = 0;

// define Motor pins
const int motor1Pin1 = 14;
const int motor1Pin2 = 27;
const int enable1Pin = 26;
const int ENCPIN_Y = 25;
const int ENCPIN_G = 33;
unsigned long unlock_start_timer;
String door_state;

char topic_uid[] = "smartlock/rfid_uid";                  //message=[<uid>]
char topic_status[] = "smartlock/status";                 //message=['online']
char topic_timer_addrfid[] = "smartlock/rfidadd_timer";   //message=['ON','OFF']
char topic_timer_delrfid[] = "smartlock/rfiddel_timer";   //message=['ON','OFF']
char topic_access[] = "smartlock/access";                 //message=['authorized','denied']
char topic_saveduids[] = "smartlock/saved_uids";          //message=[<uid list>]

char set_topic_timer_addrfid[] = "smartlock/rfidadd_timer/set";   //message=['ON','OFF']
char set_topic_timer_delrfid[] = "smartlock/rfiddel_timer/set";   //message=['ON','OFF']
char set_topic_saveduids[] = "smartlock/saved_uids/set";          //message=['DELETE','']
char set_topic_deletealluids[] = "smartlock/deleteall_uids/set";
char set_topic_option[] = "smartlock/option/set";

byte storedCard[uid_size];
String set_option;
unsigned long rfid_start_timer;


MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);   // Create MFRC522 instance.
WiFiClient espClient;
PubSubClient client;


void setup() 
{
  rfid_start_timer = millis();
  unlock_start_timer = millis();
  EEPROM.begin(500);

  Serial.begin(115200);   // Initiate a serial communication
  Serial.println("Booting");

  // Motor init
  pinMode(ENCPIN_Y, INPUT);
  pinMode(ENCPIN_G, INPUT);
  
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(ENCPIN_Y), encoder, RISING);

//  pinMode(27, OUTPUT);
//  digitalWrite(27, HIGH);
//  
  // NFC init  
  SPI.begin();          // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  Serial.println("Approximate your card to the reader...");
  Serial.println();

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

  // MQTT init
  client.setClient(espClient);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqtt_callback);
  if (client.connect("arduinoClient", mqtt_user, mqtt_pass)) {
    client.publish(topic_status, "connected");
    client.subscribe(mqtt_subscribe_topic);
  }

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

void loop()
{
  if (millis() - unlock_start_timer >= unlock_timout * 1000 && door_state == "unlocked"){
    turnMotor("lock");
  }
  if (millis() - rfid_start_timer >= rfid_timout * 1000 && set_option != ""){
    if (set_option == "add_rfid") {
      client.publish(topic_timer_addrfid, "OFF");
    } else if (set_option == "del_rfid") {
      client.publish(topic_timer_delrfid, "OFF");
    } 
    Serial.println("----------> time is up......");
    set_option = "";
  }
  client.loop();
  if (!client.connected()) {
    mqtt_reconnect();
  }
  ArduinoOTA.handle();
  readRFID();
}

//////////////////////////////////////// MOTOR //////////////////////////////

void turnMotor(String moto_dir){
  Serial.print("Encoder status: ");
  Serial.println(encoder_pos);
  if (moto_dir == "unlock") {
    digitalWrite(enable1Pin, HIGH);
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH);
    door_state = "unlocked";
  } else if (moto_dir == "lock") {
    digitalWrite(enable1Pin, HIGH);
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);
    door_state = "locked";
  }
  delay(5000);
  stopMotor();

  unlock_start_timer = millis();
  Serial.println(door_state);
}

void stopMotor(){
  digitalWrite(enable1Pin, LOW); 
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  Serial.print("Motor has stopped: ");
  Serial.print("Encoder status: ");
  Serial.println(encoder_pos);
}

void encoder(){
  if(digitalRead(ENCPIN_G) == HIGH){
    encoder_pos++;
  }else{
    encoder_pos--;
  }
  
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

  // ##### Start timer for adding new RFID tag #####  
  if (topic == set_topic_timer_addrfid) {
    if (message == "ON") {
      Serial.println("Add a new RFID on the EEPROM.");
      set_option = "add_rfid";
      client.publish(topic_timer_addrfid, "ON");
      rfid_start_timer = millis();
    } else {
      set_option = "";
      client.publish(topic_timer_addrfid, "OFF");
    }
  }
  
  // ##### Start timer for deleting new RFID tag #####  
  else if (topic == set_topic_timer_delrfid) {
    if (message == "ON") {
      Serial.println("Delete a RFID from the EEPROM.");
      set_option = "del_rfid";
      client.publish(topic_timer_delrfid, "ON");
      rfid_start_timer = millis();
    } else {
      set_option = "";
      client.publish(topic_timer_delrfid, "OFF");
    }
  }

  else if (topic == set_topic_saveduids) {
    if (message == "DELETE"){
      Serial.println("Delete all RFIDs from the EEPROM.");
      deleteAll();
    }
    getIDs();
  }

  else if (topic == set_topic_deletealluids) {
    Serial.println("Delete all RFIDs from the EEPROM.");
    deleteAll();
    getIDs();
  }

  // ##### Other options #####  
  else if (topic == set_topic_option) {
    if (message == "DELETEALL") {
      Serial.println("Delete all RFIDs from the EEPROM.");
      deleteAll();
      getIDs();
    } else if (message == "GETIDS"){
      getIDs();
    }
  }

  else {
    Serial.println("something went wrong. the topic can't be subscribed");
  }

}


//////////////////////////////////////// RFID //////////////////////////////

void readRFID(){
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()){return;}
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {return;}
  Serial.println('test');
  String uid_str = "";
  byte uid_byte[uid_size];
  char uid_char[mfrc522.uid.size];
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     uid_str.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ":"));
     uid_str.concat(String(mfrc522.uid.uidByte[i], HEX));
     uid_byte[i] = mfrc522.uid.uidByte[i];
  }
  uid_str.toUpperCase();
  uid_str.toCharArray(uid_char, 50);
  uid_str = uid_str.substring(1);  // Get access denied if I remove this... :(
  client.publish(topic_uid, uid_char);
  
  Serial.print("UID tag: ");
  Serial.println(uid_char);
  
  Serial.print("Access: ");
  if (set_option == "add_rfid") {
    writeID(uid_byte);
    set_option = "";
    client.publish(topic_timer_addrfid, "OFF");
    getIDs();
  } else if (set_option == "del_rfid") {
    deleteID(uid_byte);
    set_option = "";
    client.publish(topic_timer_delrfid, "OFF");
    getIDs();
  } else if ( findID(uid_byte) ) {
    Serial.println("Authorized");
    client.publish(topic_access, "authorized");
    turnMotor("unlock");
  } else {
    Serial.println(" Denied");
    client.publish(topic_access, "denied");
    stopMotor();
  }
  Serial.println();
  delay(1000);
}





//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * uid_size ) + 2;    // Figure out starting position
  for ( int i = 0; i < uid_size; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}


void getIDs() {
  int count = EEPROM.read(0);
  Serial.print(F("I have "));
  Serial.print(count);
  Serial.print(F(" record(s) on EEPROM"));
  Serial.println("");
  String uids_str = "";
  for ( int i = 1; i <= count; i++ ) {
    String uid_str = "";
    int start = (i * uid_size ) + 2;
    for ( int j = 0; j < uid_size; j++ ) { 
      storedCard[i] = EEPROM.read(start + j);
      uid_str.concat(String(storedCard[i] < 0x10 ? "0" : ":"));
      uid_str.concat(String(storedCard[i], HEX));
    }
    uid_str.toUpperCase();
    if (i != 1){
      uids_str.concat(",");
    }
    uids_str.concat(uid_str);
    Serial.print(i);
    Serial.print(") UID: ");
    Serial.println(uid_str);
  }
  Serial.println();
  
  char uid_char[500];
  uids_str.toCharArray(uid_char, 500);
  client.publish(topic_saveduids, uid_char);
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    int num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    int start = ( num * uid_size ) + uid_size + 2;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( int j = 0; j < uid_size; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM, or already exists"));
  }
}


///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    int num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    int slot;       // Figure out the slot number of the card
    int start;      // = ( num * uid_size ) + uid_size + 2; // Figure out where the next slot starts
    int looping;    // The number of times the loop repeats
    int j;
    int count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * uid_size) + 2;
    looping = ((num - slot) * uid_size);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + uid_size + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( int k = 0; k < uid_size; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    EEPROM.commit();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}


void deleteAll() {
  int count = EEPROM.read(0);
  EEPROM.write( 0, 0 );

  for ( int i = 1; i <= count; i++ ) {
    int start = (i * uid_size ) + 2;
    for ( int j = 0; j < uid_size; j++ ) {
      EEPROM.write(start + j, 0);
    }
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  boolean match = false;
  if ( a[0] != NULL )       // Make sure there is something in the array first
    match = true;       // Assume they match at first
  for ( int k = 0; k < uid_size; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] )     // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if ( match ) {      // Check to see if if match is still true
    return true;      // Return true
  }
  else  {
    return false;       // Return false
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
int findIDSLOT( byte find[] ) {
  int count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  int count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( int i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    }
    else {    // If not, return false
    }
  }
  return false;
}
