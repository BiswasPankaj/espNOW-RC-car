#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

#define SIGNAL_TIMEOUT 1000  // This is signal timeout in milli seconds. We will reset the data if no signal

unsigned long lastRecvTime = 0;

typedef struct PacketData
{
  byte throttle;          //ch2
  byte steering;          //ch1
  byte knob1;             //ch3
}PacketData;
PacketData receiverData;

Servo ch1;  //steering     
Servo ch2;  //throttle
Servo ch3;  //knob1       

//Assign default input received values
void setInputDefaultValues()
{
  // The middle position for joystick. (254/2=127)
  receiverData.throttle = 129;
  receiverData.steering = 127;
  receiverData.knob1 = 127;   
}

void mapAndWriteValues()
{
  ch2.write(map(receiverData.throttle, 0, 254, 0, 180));    //esc
  ch1.write(map(receiverData.steering, 0, 254, 0, 180));  //servo
  ch3.write(map(receiverData.knob1, 0, 254, 0, 180));  
}

// Updated callback function that will be executed when data is received
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) 
{
  if (len == 0)
  {
    return;
  }
  memcpy(&receiverData, incomingData, sizeof(receiverData));

  // Get the sender's mac address
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           esp_now_info->src_addr[0], esp_now_info->src_addr[1],
           esp_now_info->src_addr[2], esp_now_info->src_addr[3],
           esp_now_info->src_addr[4], esp_now_info->src_addr[5]);

  String inputData = "Recv from: " + String(macStr) + " values " + 
                     String(receiverData.throttle) + "  " + 
                     String(receiverData.steering) + " " + 
                     String(receiverData.knob1);
  Serial.println(inputData);

  mapAndWriteValues();  
  lastRecvTime = millis(); 
}

void setUpPinModes()
{
  ch1.attach(25, 1000, 2000);       //25
  ch2.attach(26, 1000, 2000);       //26
  ch3.attach(27, 1000, 2000);       //27

  setInputDefaultValues();
  mapAndWriteValues();
}

void setup() 
{
  setUpPinModes();
 
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) 
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  //Check Signal lost.
  unsigned long now = millis();
  if ( now - lastRecvTime > SIGNAL_TIMEOUT ) 
  {
    setInputDefaultValues();
    mapAndWriteValues();  
  }
}