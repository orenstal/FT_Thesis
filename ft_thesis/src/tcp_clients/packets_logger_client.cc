/*
 * packet_logger_client.cpp
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */

#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "client.hh"
#include "../common/wrappedPacketData/wrapped_packet_data.hh"

#define STORE_COMMAND_TYPE 0

using namespace std;


class PacketLoggerClient : public Client {

protected:
	void serializeObject(void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command);

public:
	PacketLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
 };

void PacketLoggerClient::serializeObject(void* obj, char* serialized, int* len) {
	cout << "PacketLoggerClient::serializeObject" << endl;
	WrappedPacketData* wpd = (WrappedPacketData*)obj;
	uint16_t size = wpd->size;

	uint64_t *q = (uint64_t*)serialized;
	*q = wpd->packetId;
	q++;

	uint16_t *p = (uint16_t*)q;
	*p = wpd->offset;
	p++;

	*p = size;
	p++;

	char *r = (char*)p;

	for (int i=0; i< size; i++, r++) {
		*r = wpd->data[i];
	}

	*len = sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t) + (sizeof(char) * size);

	cout << "len is: " << *len << endl;
}

void PacketLoggerClient::handleReturnValue(int status, char* retVal, int len, int command) {
	cout << "PacketLoggerClient::handleReturnValue" << endl;

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		cout << "nothing to handle." << endl;
	}
}



WrappedPacketData* prepareTest1() {
	cout << "preparing test 1" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 193L;
	wpd->offset = 12;
	wpd->size = 15;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'a';
	}

	wpd->data = data;

	return wpd;
}

WrappedPacketData* prepareTest2() {
	cout << "preparing test 2" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 194L;
	wpd->offset = 13;
	wpd->size = 15;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'b';
	}

	wpd->data = data;

	return wpd;
}

WrappedPacketData* prepareTest3() {
	cout << "preparing test 3" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 195L;
	wpd->offset = 13;
	wpd->size = 10;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'c';
	}

	wpd->data = data;

	return wpd;
}

WrappedPacketData* prepareTest4() {
	cout << "preparing test 4" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 225L;
	wpd->offset = 0;
	wpd->size = 10;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'd';
	}

	wpd->data = data;

	return wpd;
}

void runTest(PacketLoggerClient *client, WrappedPacketData* wpd) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)wpd, serialized, &len, STORE_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

int main () {
	cout << "starting progress logger client" << endl;

	PacketLoggerClient *client = new PacketLoggerClient(9097, "127.0.0.1");
	client->connectToServer();

	cout << "start running test 1..." << endl;
	WrappedPacketData* wpd = prepareTest1();
	runTest(client, wpd);
	delete wpd;

	cout << "\nstart running test 2..." << endl;
	wpd = prepareTest2();
	runTest(client, wpd);
	delete wpd;

	cout << "\nstart running test 3..." << endl;
	wpd = prepareTest3();
	runTest(client, wpd);
	delete wpd;

	cout << "\nstart running test 4..." << endl;
	wpd = prepareTest4();
	runTest(client, wpd);
	delete wpd;

	return 0;
}




