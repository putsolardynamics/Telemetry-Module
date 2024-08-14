#pragma once

#include <Arduino.h>

enum connection_status : uint8_t{
	DISCONNECTED=B00,
	CONNECTED=B01,
	LAGGING=B10, // data = num packets left to process
};

struct packet_t{
	// uint8_t icmp; // bitfield [connection_status(2bits), connection_data(6bits)]
	uint8_t len;
	uint8_t data[250];
};

template<size_t capacity, class T>
class Queue{
private:
	T elements[capacity];
	uint8_t start=0;
	uint8_t end=0;
	uint8_t size=0;
public:
	uint8_t len(){
		return size;
	}
	T* top(){
		return &elements[start];
	}
	void push(const T& packet){
		// memcpy(&elements[end],packet,sizeof(T));
		elements[end] = packet;
		end = (end+1)%capacity;
		++size;
		// memcpy(&elements[end],packet,sizeof(T));
	}
	T pop(){
		uint8_t tmp_start = start;
		start = (tmp_start+1)%capacity;
		--size;
		return elements[tmp_start];
	}
};

