#include <Arduino.h>
#include <WiFi.h>
#include "esp_private/wifi.h"
#include "esp_wifi.h"

#include <cstdint>
#include <esp_now.h>

#define BOOT_PIN 9
#define LED_PIN 8

#define CHANNEL 1
#define DATARATE WIFI_PHY_RATE_24M
#define CONNECTION_WAIT 5000


#define RECIEVER 1
// #define TRANSMITTER 1

#ifdef RECIEVER
uint8_t peerAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#elif TRANSMITTER
uint8_t peerAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif


enum connection_status :uint8_t{
	DISCONNECTED=B00,
	CONNECTED=B01,
	LAGGING=B10, // data = num packets left to process
};


struct packet_t{
	uint8_t icmp; // bitfield [connection_status(2bits), connection_data(6bits)]
	uint8_t len;
	uint8_t data[252];
};

void print_packet(packet_t* pkt){
	for(uint8_t i=0;i<sizeof(packet_t);i++){
		Serial.printf("%02X",pkt[i]);
	}
	Serial.println();
 }

#define QUEUE_SIZE 64
packet_t queue[QUEUE_SIZE];
uint8_t queue_len = 0;

uint64_t last_packet =0;

void enqueue(packet_t* packet){
	memcpy(&queue[++queue_len], packet, sizeof(packet_t));
}
packet_t dequeue(){
	packet_t element = queue[0];
	for(uint8_t i=1;i<queue_len+1;i++) queue[i-1] = queue[i];
	return element;
}


#ifdef RECIEVER
void onRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	if(!memcmp(mac,peerAddr,6)) return;
	enqueue((packet_t*)incomingData);
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
	
}
#elif TRANSMITTER

#endif

void setup() {
  Serial.begin(9600);


	// setup code from https://hackaday.io/project/161896-linux-espnow/log/161046-implementation
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);

	/*Stop wifi to change config parameters*/
	esp_wifi_stop();
	esp_wifi_deinit();

	/*Disabling AMPDU is necessary to set a fix rate*/
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT(); //We use the default config ...
	my_config.ampdu_tx_enable = 0;                             //... and modify only what we want.
	esp_wifi_init(&my_config);                                 //set the new config

	esp_wifi_start();

	/*You'll see that in the next subsection ;)*/
	esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
	// struct wifi_iterface_t if

	/*set the rate*/
	esp_wifi_internal_set_fix_rate(WIFI_IF_STA, true, DATARATE);

	/*rest of the code :*/
	esp_now_init();

	#ifdef RECIEVER
	esp_now_register_recv_cb(onRecv);
	esp_now_register_send_cb(onSent);
	#elif TRANSMITTER

	#endif

}

#ifdef RECIEVER
packet_t tmp;
void loop() {
	if(last_packet > CONNECTION_WAIT && queue_len==0){
		tmp.icmp = DISCONNECTED << 6;
		tmp.len = 0;
		esp_now_send(peerAddr,(uint8_t*)&tmp, sizeof(packet_t));
		delay(500);
	}

	if(queue_len==0) return;
	
	tmp = dequeue();
	last_packet = millis();
	switch(tmp.icmp & B11000000){
		case(DISCONNECTED):{
			tmp.icmp = CONNECTED << 6;
			tmp.len = 0;
			esp_now_send(peerAddr,(uint8_t*)&tmp,sizeof(packet_t));
			break;
		}
		case(CONNECTED):{
			// TODO: output the packet via serial
			print_packet(&tmp);
			if(queue_len<=QUEUE_SIZE/2) break;
			tmp.icmp = (LAGGING << 6) | queue_len;
			tmp.len = 0;
			esp_now_send(peerAddr,(uint8_t*)&tmp,sizeof(packet_t));
			break;
		}
	}
	
	delay(100);
}
#elif TRANSMITTER
void loop(){
	
}
#endif
