#include <Arduino.h>
#include <esp32_can.h>
#include "mqtt/mqtt.h"
#include "secrets.h"
#include "comfoair/message.h"
#include <WiFi.h>
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

comfoair::ComfoMessage *comfoMessage;

char messageBuf[30];

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

CAN_FRAME canMessage;
comfoair::DecodedMessage decodedMessage;

char mqttTopicMsgBuf[30];
char mqttTopicValBuf[30];

const char ventillation_level[] = MQTT_PREFIX "/commands/ventilation_level";
const char set_mode[] = MQTT_PREFIX "/commands/set_mode";

// converts character array
// to string and returns it
String convertToString(char* a, int size)
{
    int i;
    String s = "";
    for (i = 0; i < size; i++) {
        s = s + a[i];
    }
    return s;
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_level_0", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_level_1", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_level_2", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_level_3", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_level", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/set_mode", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/boost_10_min", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/boost_20_min", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/boost_30_min", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/boost_60_min", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/boost_end", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/auto", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/manual", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/bypass_activate_1h", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/bypass_deactivate_1h", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/bypass_auto", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_supply_only", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_supply_only_reset", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_extract_only", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/ventilation_extract_only_reset", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/temp_profile_normal", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/temp_profile_cool", 2);
  mqttClient.subscribe(MQTT_PREFIX "/commands/temp_profile_warm", 2);

  // Serial.print("Subscribing at QoS 2, packetId: ");
  // Serial.println(packetIdSub);
  // mqttClient.publish("test/lol", 0, true, "test 1");
  // Serial.println("Publishing at QoS 0");
  // uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
  // Serial.print("Publishing at QoS 1, packetId: ");
  // Serial.println(packetIdPub1);
  // uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
  // Serial.print("Publishing at QoS 2, packetId: ");
  // Serial.println(packetIdPub2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
  Serial.print("  payload: ");
  Serial.println(convertToString(payload, len));
  if (strcmp(topic, ventillation_level) == 0) {
    Serial.println("VENTILLATION LEVEL");
    sprintf(messageBuf, "ventilation_level_%c", payload[0]);
    comfoMessage->sendCommand(messageBuf);
  } else if (strcmp(topic, set_mode) == 0) {
    Serial.println("SET MODE");
    if (memcmp("auto", payload, 4) == 0) {
      sprintf(messageBuf, "auto");
    } else {
      sprintf(messageBuf, "manual");
    }
    comfoMessage->sendCommand(messageBuf);
  } else {
    String topic_string(topic);
    String removal(MQTT_PREFIX "/commands/");
    String action = topic_string.substring(removal.length(), topic_string.length());
    Serial.print("Action: ");
    Serial.println(action);
    sprintf(messageBuf, action.c_str());
    comfoMessage->sendCommand(messageBuf);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        break;
    }
}

void printFrame2(CAN_FRAME *message)
{
  Serial.print(message->id, HEX);
  if (message->extended) Serial.print(" X ");
  else Serial.print(" S ");   
  Serial.print(message->length, DEC);
  for (int i = 0; i < message->length; i++) {
    Serial.print(message->data.byte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);

  connectToWifi();

  comfoMessage = new comfoair::ComfoMessage();

  Serial.println("ESP_SMT init");
  CAN0.setCANPins(GPIO_NUM_17, GPIO_NUM_16);
  // CAN0.setDebuggingMode(true);
  CAN0.begin(50000);
  CAN0.watchFor();

  Serial.println("ESPVMC init");
  
}

void loop() {
  if (CAN0.read(canMessage)) {
    printFrame2(&canMessage);
    if (comfoMessage->decode(&canMessage, &decodedMessage)) {
      Serial.println("Decoded :)");
      Serial.print(decodedMessage.name);
      Serial.print(" - ");
      Serial.print(decodedMessage.val);
      sprintf(mqttTopicMsgBuf, "%s/%s", MQTT_PREFIX, decodedMessage.name);
      sprintf(mqttTopicValBuf, "%s", decodedMessage.val);
      mqttClient.publish(mqttTopicMsgBuf, 0, true, mqttTopicValBuf);
    }
  }
}