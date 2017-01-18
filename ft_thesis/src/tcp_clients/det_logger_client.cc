#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "../common/pal_api/pals_manager.hh"
#include "client.hh"

#define STORE_COMMAND_TYPE 0
#define GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE 1
#define GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE 2

using namespace std;

class DetLoggerClient : public Client {

private:
	void serializePalsManagerObject(int command, void* obj, char* serialized, int* len);
	void serializeGetPalsByMBIdAndPackId(int command, void* obj, char* serialized, int* len);
	void handleGetPacketIdsResponse(int command, char* retVal, int len);
	void handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen);

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command);

public:
	DetLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}

//	void serializePalsManager (PALSManager* pm, char* serialized, int* len);
 };

void DetLoggerClient::serializePalsManagerObject(int command, void* obj, char* serialized, int* len) {
	cout << "DetLoggerClient::serializePalsManagerObject" << endl;
	PALSManager* pm = (PALSManager*)obj;

	cout << "start serializing det_logger client" << endl;
	PALSManager::serialize(pm, serialized, len);
}

void DetLoggerClient::serializeGetPalsByMBIdAndPackId(int command, void* obj, char* serialized, int* len) {
	uint16_t* mbIdInput = (uint16_t*)obj;
	uint16_t *q = (uint16_t*)serialized;
	*q = *mbIdInput;
	q++;
	mbIdInput++;

	uint64_t *packIdInput = (uint64_t*)mbIdInput;
	uint64_t *p = (uint64_t*)q;
	*p = *packIdInput;
	*len = sizeof(uint16_t) + sizeof(uint64_t);
}

void DetLoggerClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	cout << "DetLoggerClient::serializeObject" << endl;

	if (command == STORE_COMMAND_TYPE) {
		serializePalsManagerObject(command, obj, serialized, len);
	} else if (command == GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE) {
		uint16_t *q = (uint16_t*)serialized;
		*q = *((uint16_t*)obj);
		*len = sizeof(uint16_t);
	} else if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		serializeGetPalsByMBIdAndPackId(command, obj, serialized, len);
	}
}

void DetLoggerClient::handleGetPacketIdsResponse(int command, char* retVal, int len) {
	cout << "DetLoggerClient::handleGetPacketIdsResponse" << endl;
	cout << "len is: " << len << ", retVal is: " << retVal << endl;

	uint64_t* ret = (uint64_t*)retVal;
	uint32_t numOfPacketIds = len/sizeof(uint64_t*);
	cout << "numOfPacketIds: " << numOfPacketIds << ", received packet ids are:" << endl;

	for (uint32_t i =0; i<numOfPacketIds; i++) {
		cout << "ret[" << i << "]: " << ret[i] << endl;
	}

	cout << "done handling" << endl;
}

void DetLoggerClient::handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen) {
	cout << "DetLoggerClient::handleGetPalsByMBIdAndPackIdResponse" << endl;
	PALSManager* pm = new PALSManager();
	PALSManager::deserialize(retVal, pm);

	pm->printContent();
	cout << "[DetLoggerClient::handleGetPalsByMBIdAndPackIdResponse] done" << endl;
}

void DetLoggerClient::handleReturnValue(int status, char* retVal, int len, int command) {
	cout << "DetLoggerClient::handleReturnValue" << endl;

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		cout << "nothing to handle." << endl;
	} else if (command == GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE) {
		handleGetPacketIdsResponse(command, retVal, len);
	} else if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		handleGetPalsByMBIdAndPackIdResponse(command, retVal, len);
	}
}

PALSManager* prepareTest1() {
	cout << "preparing test 1" << endl;

	PALSManager* pm = new PALSManager(1, 331L);
	gpal gp_1 = {1, "test 1\0"};
	pm->addGPal(&gp_1);
	return pm;
}

PALSManager* prepareTest2() {
	cout << "preparing test 2" << endl;

	PALSManager* pm = new PALSManager(1, 332L);
	gpal gp_1 = {1, "first_gpal_1\0"};
	spal sp_1 = {1, 1};
	spal sp_3 = {1, 3};
	spal sp_4 = {2, 1};
	gpal gp_3 = {2, "first_gpal_2\0"};

	pm->addGPal(&gp_1);
	pm->addSPal(&sp_1);
	pm->createSPalAndAdd(1,2);
	pm->addSPal(&sp_3);
	pm->createGPalAndAdd(1, "second_gpal_1\0");
	pm->addSPal(&sp_4);
	pm->addGPal(&gp_3);

	return pm;
}

PALSManager* prepareTest3() {
	cout << "preparing test 3" << endl;

	PALSManager* pm = new PALSManager(1, 332L);
	spal sp_1 = {1, 10};

	pm->addSPal(&sp_1);
	pm->createSPalAndAdd(1,2);

	return pm;
}

PALSManager* prepareTest4() {
	cout << "preparing test 4" << endl;

	PALSManager* pm = new PALSManager(2, 331L);
	gpal gp_1 = {1, "test 4\0"};
	pm->addGPal(&gp_1);
	return pm;
}

void* prepareGetPalsTest(uint16_t mbId, uint64_t packId) {
	cout << "preparing get pals test for mbId: " << mbId << ", packId: " << packId << endl;

	char* input = new char[sizeof(uint16_t)+sizeof(uint64_t)+1];
	uint16_t* mbIdInput = (uint16_t*)input;
	*mbIdInput = mbId;
	mbIdInput++;

	uint64_t* packIdInput = (uint64_t*)mbIdInput;
	*packIdInput = packId;

	return (void*)input;
}

void runTestAndCompare(DetLoggerClient *client) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	PALSManager* pm = prepareTest1();

	client->prepareToSend((void*)pm, serialized, &len, STORE_COMMAND_TYPE);

	PALSManager* pm1 = new PALSManager(1, 444L);
	PALSManager::deserialize(serialized+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX, pm1);

	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

	gpal gp_1 = {1, "test 1"};
	gpal* prGList = pm->getGPalList();
	gpal* pr1GList = pm1->getGPalList();

	bool deserializeSucceed = true;

	if (prGList[0].var_id != pr1GList[0].var_id) {
		cout << "prGList[0].var_id: " << prGList[0].var_id << ", " << pr1GList[0].var_id << endl;
		deserializeSucceed = false;
	} else {
		for (int j=0; j<6; j++) {
			if (prGList[0].val[j] != pr1GList[0].val[j]) {
				cout << "val[" << j << "]: " << prGList[0].val[j] << ", pr1GList[0].val[j]: " << pr1GList[0].val[j] << endl;
				deserializeSucceed = false;
			}
		}
	}

	if (deserializeSucceed) {
		cout << "deserialize was succeeded" << endl;
	} else {
		cout << "deserialize was failed" << endl;
	}

	delete pm1;
	delete pm;
}

void runTest(DetLoggerClient *client, PALSManager* pm) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)pm, serialized, &len, STORE_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

void getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)mbId, serialized, &len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

void getPals(DetLoggerClient *client, void* msgToSend) {

	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend(msgToSend, serialized, &len, GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

int main () {
	cout << "starting det logger client" << endl;

	DetLoggerClient *client = new DetLoggerClient(9095, "127.0.0.1");
	client->connectToServer();

	cout << "start running test 1..." << endl;
	runTestAndCompare(client);

	cout << "\nstart running test 2..." << endl;
	PALSManager* pm = prepareTest2();
	runTest(client, pm);
	delete pm;

	cout << "\nstart running test 3..." << endl;
	pm = prepareTest3();
	runTest(client, pm);
	delete pm;

	cout << "\nstart running test 4..." << endl;
	PALSManager* pm1 = prepareTest4();
	runTest(client, pm1);
	delete pm1;

	cout << "\nstart getting mb 1 processed packets..." << endl;
	uint16_t* mbId = new uint16_t;
	*mbId = 1;
	getProcessedPacketIds(client, mbId);
	delete mbId;

	cout << "\nstart getting mb 2 processed packets..." << endl;
	mbId = new uint16_t;
	*mbId = 2;
	getProcessedPacketIds(client, mbId);
	delete mbId;

	cout << "\nstart getting pals for mbId 1, packId 331..." << endl;
	void* inputs = prepareGetPalsTest(1, 331);
	getPals(client, inputs);
	delete (char*)inputs;

	cout << "\nstart getting pals for mbId 1, packId 332..." << endl;
	inputs = prepareGetPalsTest(1, 332);
	getPals(client, inputs);
	delete (char*)inputs;

	cout << "\nstart getting pals for mbId 2, packId 331..." << endl;
	inputs = prepareGetPalsTest(2, 331);
	getPals(client, inputs);
	delete (char*)inputs;

	return 0;
}
