#include <Arduino.h>
#include <WiFi.h>

#include "HardwareSerial.h"
#include "esp_private/wifi.h"
#include <esp_wifi_types.h>
#include <esp_now.h>


#include "util.hpp"

// #define BOOT_PIN 9
#define LED_PIN 8

#define CHANNEL 1
#define DATARATE WIFI_PHY_RATE_LORA_250K
#define QUEUE_SIZE 256

#define RXD1 6
#define TXD1 7

/* global variables */
Queue<QUEUE_SIZE,packet_t> recv_queue;
Queue<QUEUE_SIZE,packet_t> send_queue;

#ifdef RECIEVER
uint8_t peerAddr[] = {0x80,0x65,0x99,0x85,0xBD,0x64};
uint8_t myAddr[] = {0x48,0x31,0xB7,0x04,0x90,0x1C}; // 48:31:B7:04:90:1C
#elif TRANSMITTER
uint8_t peerAddr[] = {0x48,0x31,0xB7,0x04,0x90,0x1C};
uint8_t myAddr[] = {0x80,0x65,0x99,0x85,0xBD,0x64};
#endif
/* global variables */


void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if(memcmp(mac,peerAddr,6)!=0) return;
	packet_t tmp;
	tmp.len = len;
	memcpy(tmp.data,incomingData,len);
	recv_queue.push(tmp);
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);

  #ifdef DEBUG
  delay(1000);
  Serial.println("Running in debug mode");
  #endif

	WiFi.mode(WIFI_STA);

	// setting mac address only once is enough
	#ifdef DEBUG
  if (esp_wifi_set_mac(WIFI_IF_STA, &myAddr[0]) == ESP_OK) {
    Serial.println("Success changing Mac Address");
  } else {
  	Serial.println("Unable to set Mac Address");
  }
	#endif
	Serial.println("My mac address:");
  Serial.println(WiFi.macAddress());


	// esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

	esp_wifi_config_espnow_rate(WIFI_IF_STA,DATARATE);

	if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

	esp_now_register_recv_cb(onRecv);
	esp_now_register_send_cb(onSent);


	esp_now_peer_info_t peerInfo;
	memcpy(peerInfo.peer_addr, peerAddr, 6);
	peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.channel = CHANNEL;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

}


void loop(){
	delay(100);
	// recieve esp-now and print to uart
	if(recv_queue.len()!=0){
		packet_t tmp = recv_queue.pop();
		Serial.printf("Received esp-now packet of length:%d\n",+tmp.len);
		// Serial.write(tmp.data,tmp.len);
		Serial1.write(tmp.data,tmp.len);
	}

	static packet_t tmp;

	uint8_t read_len = Serial1.read(tmp.data,sizeof(tmp.data));
	tmp.len = read_len;
	// Serial1.write(tmp.data,tmp.len);
	Serial.write(tmp.data,tmp.len);
	if(tmp.len>0){
		send_queue.push(tmp);
	}

	read_len = Serial.read(tmp.data,sizeof(tmp.data));
	tmp.len = read_len;
	Serial1.write(tmp.data,tmp.len);
	// Serial.write(tmp.data,tmp.len);
	if(tmp.len>0){
		send_queue.push(tmp);
	}

	while(send_queue.len()!=0){
		packet_t* tt = send_queue.top();
		// Serial.printf("packet len: %u\n",tt->len);

		esp_err_t send_status;
		if(tt->len>0) send_status = esp_now_send(peerAddr,(uint8_t*)tt->data,tt->len);
		if(send_status==ESP_OK){
			send_queue.pop();
			Serial.println("Successfully sent a packet");
		} else {
			Serial.println("Couldn't send a packet");
		}
	}
}
