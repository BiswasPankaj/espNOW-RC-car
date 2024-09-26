// 2 Channel Transmitter & Trims with 3-way Toggle Switch for Throttle Control
#include <esp_now.h>
#include <WiFi.h>
#include <EEPROM.h> 

// define the number of bytes you want to access
#define EEPROM_SIZE 3

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t receiverMacAddress[] = {0xd0, 0xef, 0x76, 0x58, 0xc0, 0x78};  //d0:ef:76:58:c0:78

#define trimbut_1 19                      // Trim button 1 / Pin 19
#define trimbut_2 18                      // Trim button 2 / Pin 18 
#define trimbut_3 23                      // Trim button 1 / Pin 23
#define trimbut_4 22                      // Trim button 2 / Pin 22 

// New definitions for the toggle switch
#define TOGGLE_PIN_1 14  // Replace with your actual pin number
#define TOGGLE_PIN_2 15  // Replace with your actual pin number

int tvalue1 = EEPROM.read(0) * 16;        // Reading trim values from Eprom
int tvalue2 = EEPROM.read(2) * 16;        // Reading trim values from Eprom

int throttle_offset = 0;                  //throttle offset

// New variables for throttle mode
int throttleMode = 0;
int lastThrottleMode = -1;

typedef struct PacketData {
  byte throttle;
  byte steering;
  byte knob1; 
};
PacketData data;

esp_now_peer_info_t peerInfo;

void ResetData() 
{
  data.throttle = 127;                      // Signal lost position 
  data.steering = 127;
  data.knob1 = 127;
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\r\nLast Packet Send Status:\t ");
  Serial.println(status);
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Message sent" : "Message failed");
}

void setup()
{
  // Initializing Serial Monitor 
  Serial.begin(115200);
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  WiFi.mode(WIFI_STA);

  // Initializing ESP-NOW
  if (esp_now_init() != ESP_OK) 
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  else
  {
    Serial.println("Success: Initialized ESP-NOW");
  }
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
  else
  {
    Serial.println("Success: Added peer");
  } 
  ResetData();
 
  pinMode(trimbut_1, INPUT_PULLUP); 
  pinMode(trimbut_2, INPUT_PULLUP);
  pinMode(trimbut_3, INPUT_PULLUP); 
  pinMode(trimbut_4, INPUT_PULLUP);
  
  // New: Set up toggle switch pins
  pinMode(TOGGLE_PIN_1, INPUT_PULLUP);
  pinMode(TOGGLE_PIN_2, INPUT_PULLUP);
  
  tvalue1 = EEPROM.read(0) * 16;
  tvalue2 = EEPROM.read(2) * 16;
}

// Joystick center and its borders 
int Border_Map(int val, int lower, int middle, int upper, bool reverse)
{
  val = constrain(val, lower, upper);
  if ( val < middle )
    val = map(val, lower, middle, 0, 128);
  else
    val = map(val, middle, upper, 128, 255);
  return ( reverse ? 255 - val : val );
}

// New: Function to read toggle switch
void readToggleSwitch() {
  if (digitalRead(TOGGLE_PIN_1) == LOW) {
    throttleMode = 0;  // Switch in position 1
  } else if (digitalRead(TOGGLE_PIN_2) == LOW) {
    throttleMode = 1;  // Switch in position 2
  } else {
    throttleMode = 2;  // Switch in position 3
  }
  
  // Print the mode only when it changes
  if (throttleMode != lastThrottleMode) {
    Serial.print("Throttle Mode: ");
    Serial.println(throttleMode);
    lastThrottleMode = throttleMode;
  }
}

// Modified: applyThrottleOffset function
int applyThrottleOffset(int throttleValue) {
  int centeredThrottle = throttleValue - 127;
  
  if (abs(centeredThrottle) < 5) {
    return 127;  // Center position (deadzone)
  }
  
  switch (throttleMode) {
    case 0:
      return map(centeredThrottle, -127, 127, 255, 0);  // Full range
    case 1:
      return map(centeredThrottle, -127, 127, 191, 63);  // Medium range
    case 2:
      return map(centeredThrottle, -127, 127, 150, 104);  // Reduced range
    default:
      return map(centeredThrottle, -127, 127, 191, 63);  // Default to medium range
  }
}

void loop()
{
  // New: Read toggle switch state
  readToggleSwitch();

  // Trims and Limiting trim values 
  if(digitalRead(trimbut_1)==LOW and tvalue1 < 2520) {
    tvalue1=tvalue1+60;
    EEPROM.write(0,tvalue1/16);
    EEPROM.commit(); 
    delay (130);
  }   
  if(digitalRead(trimbut_2)==LOW and tvalue1 > 1120){
    tvalue1=tvalue1-60;
    EEPROM.write(0,tvalue1/16);
    EEPROM.commit();
    delay (130);
  }

  if(digitalRead(trimbut_3)==LOW and tvalue2 < 2520) {
    tvalue2=tvalue2+60;
    EEPROM.write(2,tvalue2/16);
    EEPROM.commit(); 
    delay (130);
  }   
  if(digitalRead(trimbut_4)==LOW and tvalue2 > 1120){
    tvalue2=tvalue2-60;
    EEPROM.write(2,tvalue2/16);
    EEPROM.commit();
    delay (130);
  }

  int rawThrottle = Border_Map(analogRead(32), 0, tvalue1, 4095, true);
  data.throttle = applyThrottleOffset(rawThrottle);

  data.steering = Border_Map(analogRead(33), 1220, tvalue2, 2735, true);
  data.knob1 = Border_Map(analogRead(34), 0, 2047, 4095, false);
   
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t *) &data, sizeof(data));
  if (result == ESP_OK) 
  {
    Serial.println("Sent with success");
  }
  else 
  {
    Serial.println("Error sending the data");
  }
  Serial.println(EEPROM.read(0));
  Serial.println(EEPROM.read(2));     
  
  delay(50);
}