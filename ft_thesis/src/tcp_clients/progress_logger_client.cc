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
using namespace std;


class ProgressLoggerClient : public Client {

protected:
	void serializeObject(void* obj, char* serialized, int* len);

public:
	ProgressLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
 };

void ProgressLoggerClient::serializeObject(void* obj, char* serialized, int* len) {
	cout << "ProgressLoggerClient::serializeObject" << endl;
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

	client->prepareToSend((void*)pd, serialized, &len);

	bool isSucceed = client->sendMsgAndWait(serialized, len);

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

