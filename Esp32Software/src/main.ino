#include <Arduino.h>
#include <WiFi.h>

#include "esp_private/wifi.h"
#include <cstdint>
#include <esp_wifi_types.h>
#include <esp_now.h>


#include "util.hpp"

#define BOOT_PIN 9
#define LED_PIN 8

#define CHANNEL 1
#define DATARATE WIFI_PHY_RATE_24M
#define CONNECTION_WAIT 5000
#define GATHER_DATA_WAIT 128
#define QUEUE_SIZE 64

/* global variables */
#ifdef RECIEVER

uint8_t peerAddr[] = {0x48,0x31,0xB7,0x04,0x90,0x1C}; // esp32c3
uint64_t last_packet;
Queue<QUEUE_SIZE,packet_t> recv_queue;
// Queue<QUEUE_SIZE,packet_t> send_queue;

#elif TRANSMITTER

uint8_t peerAddr[] = {0x7C,0x9E,0xBD,0x48,0x94,0x10}; // esp32
uint64_t last_packet;
Queue<QUEUE_SIZE,packet_t> recv_queue;
Queue<QUEUE_SIZE,packet_t> send_queue;

#endif
/* global variables */


void print_packet(packet_t* pkt){
	// Serial.printf("%02X",pkt->icmp);
	for(uint8_t i=0;i<sizeof(packet_t);i++){
		Serial.printf("%02X",pkt[i]);
	}
	Serial.println();
}


#ifdef RECIEVER

void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if(memcmp(mac,peerAddr,6)) return;
	recv_queue.push((packet_t*)incomingData);
	last_packet = millis();
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	
}

#elif TRANSMITTER

void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if(memcmp(mac,peerAddr,6)) return;
	recv_queue.push((packet_t*)incomingData);
	last_packet = millis();
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	
}
#endif

void setup() {
  Serial.begin(9600);

  #ifdef DEBUG
  delay(1000);
  #endif

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

packet_t tmp;
uint8_t status = DISCONNECTED;
void loop() {
	if(millis()-last_packet > CONNECTION_WAIT){
		tmp.icmp = DISCONNECTED << 6;
		tmp.len = 0;
		esp_now_send(peerAddr,(uint8_t*)&tmp, sizeof(packet_t));
		Serial.println("recv sending disconnected bcs connection_wait");
		delay(500);
	}

	if(recv_queue.len()==0) return;
	Serial.println("packets left to process:");
	Serial.println(recv_queue.len());
	
	tmp = recv_queue.pop();
	#ifdef DEBUG
	print_packet(&tmp);
	#endif

	switch(tmp.icmp & B11000000){
		case(DISCONNECTED):{
			tmp.icmp = CONNECTED << 6;
			tmp.len = 0;
			esp_now_send(peerAddr,(uint8_t*)&tmp,sizeof(packet_t));
			Serial.println("recv sending CONNECTED via status");
			delay(500);
			break;
		}
		case(CONNECTED):{
			// TODO: output the packet via serial
			// print_packet(&tmp);
			if(recv_queue.len()<=QUEUE_SIZE/2) break;
			tmp.icmp = (LAGGING << 6) | recv_queue.len();
			tmp.len = 0;
			esp_now_send(peerAddr,(uint8_t*)&tmp,sizeof(packet_t));
			Serial.println("recv sending LAGGING");
			break;
		}
		case(LAGGING):{
			tmp.icmp = CONNECTED << 6;
			tmp.len = 0;
			esp_now_send(peerAddr,(uint8_t*)&tmp,sizeof(packet_t));
			Serial.println("recv sending LAGGING");
			break;
		}
	}
}

#elif TRANSMITTER

packet_t tmp = {.icmp = CONNECTED<<6,.len=0};
uint8_t buffer[250];
uint8_t status = DISCONNECTED;

void loop(){
	// data gathering
	if(send_queue.len()<QUEUE_SIZE){
		uint8_t read_len = Serial.read((uint8_t*)&buffer,sizeof(buffer));
		uint8_t size_left = (sizeof(tmp.data)-tmp.len);
		if(read_len>size_left){
			// we need to split up the buffer
			memcpy(tmp.data+tmp.len,buffer,size_left);
			tmp.len += size_left;
			send_queue.push(&tmp);

			tmp.len = read_len-size_left;
			memcpy(tmp.data,buffer+tmp.len,tmp.len);
		} else if(read_len>0){
			// just copy from buffer to tmp.data
			memcpy(tmp.data+tmp.len, buffer, read_len);
			tmp.len += read_len;
		}
		if(millis()-last_packet>GATHER_DATA_WAIT && tmp.len!=0){
			send_queue.push(&tmp);
			tmp.len = 0;
		}
	}
	// event handling
	if(recv_queue.len()!=0){
		status = recv_queue.pop().icmp >> 6;
	}
	// data sending
	switch(status){
		case(CONNECTED):{
			if(send_queue.len()!=0){
				esp_err_t send_status = esp_now_send(peerAddr,(uint8_t*)send_queue.top(),sizeof(packet_t));
				if(send_status==ESP_OK){
					send_queue.pop();
					last_packet = millis();
				}
				Serial.println("send sending CONNECTED from status");
			}
			break;
		}
		case(DISCONNECTED):{
			packet_t tmp_send = {.icmp = DISCONNECTED<<6,.len=0};
			esp_now_send(peerAddr, (uint8_t*)&tmp_send, sizeof(packet_t));
			Serial.println("send sending DISCONNECTED from status");
			delay(500);
			break;
		}
		case(LAGGING):{
			delay(500);
			packet_t tmp_send = {.icmp = LAGGING<<6,.len=0};
			esp_now_send(peerAddr,(uint8_t*)&tmp_send,sizeof(packet_t));
			Serial.println("send sending LAGGING from status");
			break;
		}
	}
}

#endif
