/*
 * progress_logger_client.cpp
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */


#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "client.hh"
#include "../common/progressData/progress_data.hh"

#define STORE_COMMAND_TYPE 0

using namespace std;


class ProgressLoggerClient : public Client {

private:
	void serializeProgressDataObject(int command, void* obj, char* serialized, int* len);

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command);

public:
	ProgressLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
 };

void ProgressLoggerClient::serializeProgressDataObject(int command, void* obj, char* serialized, int* len) {
	cout << "ProgressLoggerClient::serializeProgressDataObject" << endl;
	ProgressData* pd = (ProgressData*)obj;

	uint16_t *q = (uint16_t*)serialized;
	*q = pd->mbId;
	q++;

	uint64_t *p = (uint64_t*)q;
	*p = pd->packetId;
	p++;

	*len = sizeof(uint16_t) + sizeof(uint64_t);

	cout << "len is: " << *len << endl;
}

void ProgressLoggerClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	cout << "ProgressLoggerClient::serializeObject" << endl;

	if (command == STORE_COMMAND_TYPE) {
		serializeProgressDataObject(command, obj, serialized, len);
	}
}

void ProgressLoggerClient::handleReturnValue(int status, char* retVal, int len, int command) {
	cout << "ProgressLoggerClient::handleReturnValue" << endl;

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		cout << "nothing to handle." << endl;
	}
}



ProgressData* prepareTest1() {
	cout << "preparing test 1" << endl;

	ProgressData* pd = new ProgressData;
	pd->mbId = 4;
	pd->packetId = 111L;
	return pd;
}

ProgressData* prepareTest2() {
	cout << "preparing test 2" << endl;

	ProgressData* pd = new ProgressData;
	pd->mbId = 4;
	pd->packetId = 112L;
	return pd;
}

ProgressData* prepareTest3() {
	cout << "preparing test 3" << endl;

	ProgressData* pd = new ProgressData;
	pd->mbId = 5;
	pd->packetId = 111L;
	return pd;
}


void runTest(ProgressLoggerClient *client, ProgressData* pd) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)pd, serialized, &len, STORE_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

int main () {
	cout << "starting progress logger client" << endl;

	ProgressLoggerClient *client = new ProgressLoggerClient(9096, "127.0.0.1");
	client->connectToServer();

	cout << "start running test 1..." << endl;
	ProgressData* pd = prepareTest1();
	runTest(client, pd);
	delete pd;

	cout << "\nstart running test 2..." << endl;
	pd = prepareTest2();
	runTest(client, pd);
	delete pd;

	cout << "\nstart running test 3..." << endl;
	pd = prepareTest3();
	runTest(client, pd);
	delete pd;

	return 0;
}

