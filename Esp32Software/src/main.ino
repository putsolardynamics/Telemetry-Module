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
#define DATARATE WIFI_PHY_RATE_24M
// #define DATARATE WIFI_PHY_RATE_48M
#define GATHER_DATA_WAIT 128
#define QUEUE_SIZE 256

#define RXD1 6
#define TXD1 7

/* global variables */
#ifdef RECIEVER

uint8_t peerAddr[] = {0x80,0x65,0x99,0x85,0xBD,0x64};
uint8_t myAddr[] = {0x48,0x31,0xB7,0x04,0x90,0x1C};

uint64_t last_packet;
Queue<QUEUE_SIZE,packet_t> recv_queue;
// Queue<QUEUE_SIZE,packet_t> send_queue;

#elif TRANSMITTER

uint8_t peerAddr[] = {0x48,0x31,0xB7,0x04,0x90,0x1C};
uint8_t myAddr[] = {0x80,0x65,0x99,0x85,0xBD,0x64};

uint64_t last_packet;
// Queue<QUEUE_SIZE,packet_t> recv_queue;
Queue<QUEUE_SIZE,packet_t> send_queue;

#endif
/* global variables */


#ifdef RECIEVER

void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if(memcmp(mac,peerAddr,6)) return;
	packet_t tmp;
	tmp.len = len;
	memcpy(tmp.data,incomingData,len);
	recv_queue.push(tmp);
	last_packet = millis();
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	
}

#elif TRANSMITTER

void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if(memcmp(mac,peerAddr,6)) return;
	// recv_queue.push((packet_t*)incomingData);
	last_packet = millis();
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	
}
#endif

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);

  #ifdef DEBUG
  delay(1000);
  #endif

	// setting mac address
	esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, &myAddr[0]);
  if (err == ESP_OK) {
    Serial.println("Success changing Mac Address");
  }
	Serial.println("My mac address:");
  Serial.println(WiFi.macAddress());


	// setup code from https://hackaday.io/project/161896-linux-espnow/log/161046-implementation
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);

	esp_wifi_stop();
	esp_wifi_deinit();


	/*Disabling AMPDU is necessary to set a fix rate*/
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT(); //We use the default config ...
	my_config.ampdu_tx_enable = 0;                             //... and modify only what we want.
	esp_wifi_init(&my_config);                                 //set the new config

	esp_wifi_start();

	/*You'll see that in the next subsection ;)*/
	esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

	/*set the rate*/
	esp_wifi_internal_set_fix_rate(WIFI_IF_STA, true, DATARATE);

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

#ifdef RECIEVER
void loop() {
	if(recv_queue.len()!=0){
		packet_t tmp = recv_queue.pop();
		Serial.write(tmp.data,tmp.len);
		Serial1.write(tmp.data,tmp.len);
	}
}

#elif TRANSMITTER

packet_t tmp;
uint8_t buffer[250];
void loop(){
	packet_t tmp2;
	uint8_t read_len2 = Serial1.read((uint8_t*)tmp2.data,sizeof(tmp2.len));
	Serial.write(tmp2.data,read_len2);

	uint8_t read_len = Serial.read((uint8_t*)buffer,sizeof(tmp.data));
	if(read_len>0){
		uint8_t size_left = sizeof(tmp.data)-tmp.len;
		if(size_left>=read_len){
			memcpy(tmp.data+tmp.len,buffer,read_len);
			tmp.len += read_len;
		} else{
			memcpy(tmp.data+tmp.len,buffer,size_left);
			tmp.len = sizeof(tmp.data);
			send_queue.push(tmp);

			tmp.len = read_len - size_left;
			memcpy(tmp.data,buffer,tmp.len);
		}
		if(tmp.len==sizeof(tmp.data)){
			send_queue.push(tmp);
			tmp.len=0;
		}
	}

	if(millis()-last_packet>GATHER_DATA_WAIT && tmp.len>0){
		send_queue.push(tmp);
		tmp.len=0;
	}

	if(send_queue.len()!=0){
		packet_t* tt = send_queue.top();
		Serial.printf("packet len: %u\n",tt->len);

		esp_err_t send_status;
		if(tt->len>0) send_status = esp_now_send(peerAddr,(uint8_t*)tt->data,tt->len);
		if(send_status==ESP_OK){
			send_queue.pop();
			Serial.println("sender sent packet");
			last_packet = millis();
		}
	}
}

#endif
