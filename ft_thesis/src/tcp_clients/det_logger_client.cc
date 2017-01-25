
#include "det_logger_client.hh"


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

void DetLoggerClient::handleGetPacketIdsResponse(int command, char* retVal, int len, void* retValAsObj) {
	cout << "DetLoggerClient::handleGetPacketIdsResponse" << endl;
	cout << "len is: " << len << ", retVal is: " << retVal << endl;

	vector<uint64_t> * vectorRetVal = static_cast<vector<uint64_t>*>(retValAsObj);
	cout << "starting with vectorRetVal size: " << vectorRetVal->size() << endl;

	uint64_t* ret = (uint64_t*)retVal;
	uint32_t numOfPacketIds = len/sizeof(uint64_t*);	//todo should be sizeof(uint64_t) ??
	cout << "numOfPacketIds: " << numOfPacketIds << ", received packet ids are:" << endl;

	for (uint32_t i =0; i<numOfPacketIds; i++) {
		cout << "ret[" << i << "]: " << ret[i] << endl;
		vectorRetVal->push_back(ret[i]);
	}
	cout << "leaving with vectorRetVal size: " << vectorRetVal->size() << endl;
	cout << "done handling" << endl;
}

void DetLoggerClient::handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen, void* retValAsObj) {
	cout << "DetLoggerClient::handleGetPalsByMBIdAndPackIdResponse" << endl;
	PALSManager* pm = static_cast<PALSManager*>(retValAsObj);

	PALSManager::deserialize(retVal, pm);

	pm->printContent();
	*((PALSManager*)retValAsObj) = *pm;
	cout << "[DetLoggerClient::handleGetPalsByMBIdAndPackIdResponse] done" << endl;
}

void DetLoggerClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	cout << "DetLoggerClient::handleReturnValue" << endl;

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		cout << "nothing to handle." << endl;
	} else if (command == GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE) {
		handleGetPacketIdsResponse(command, retVal, len, retValAsObj);
	} else if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		handleGetPalsByMBIdAndPackIdResponse(command, retVal, len, retValAsObj);
	}
}

void DetLoggerClient::runTests(char* address) {
	cout << "starting det logger client" << endl;

	DetLoggerClient *client = new DetLoggerClient(9095, address);	// "127.0.0.1"
	client->connectToServer();

	cout << "start running test 1..." << endl;
	runTestAndCompare(client);

	cout << "\nstart running test 2..." << endl;
	PALSManager* pm = prepareDetLoggerTest2();
	runDetLoggerTest(client, pm);
	delete pm;

	cout << "\nstart running test 3..." << endl;
	pm = prepareDetLoggerTest3();
	runDetLoggerTest(client, pm);
	delete pm;

	cout << "\nstart running test 4..." << endl;
	PALSManager* pm1 = prepareDetLoggerTest4();
	runDetLoggerTest(client, pm1);
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
}

PALSManager* DetLoggerClient::prepareDetLoggerTest1() {
	cout << "preparing test 1" << endl;

	PALSManager* pm = new PALSManager(1, 331L);
	gpal gp_1 = {1, "test 1\0"};
	pm->addGPal(&gp_1);
	return pm;
}

PALSManager* DetLoggerClient::prepareDetLoggerTest2() {
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


PALSManager* DetLoggerClient::prepareDetLoggerTest3() {
	cout << "preparing test 3" << endl;

	PALSManager* pm = new PALSManager(1, 332L);
	spal sp_1 = {1, 10};

	pm->addSPal(&sp_1);
	pm->createSPalAndAdd(1,2);

	return pm;
}

PALSManager* DetLoggerClient::prepareDetLoggerTest4() {
	cout << "preparing test 4" << endl;

	PALSManager* pm = new PALSManager(2, 331L);
	gpal gp_1 = {1, "test 4\0"};
	pm->addGPal(&gp_1);
	return pm;
}

void* DetLoggerClient::prepareGetPalsTest(uint16_t mbId, uint64_t packId) {
	cout << "preparing get pals test for mbId: " << mbId << ", packId: " << packId << endl;

	char* input = new char[sizeof(uint16_t)+sizeof(uint64_t)+1];
	uint16_t* mbIdInput = (uint16_t*)input;
	*mbIdInput = mbId;
	mbIdInput++;

	uint64_t* packIdInput = (uint64_t*)mbIdInput;
	*packIdInput = packId;

	return (void*)input;
}

void DetLoggerClient::runTestAndCompare(DetLoggerClient *client) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	PALSManager* pm = prepareDetLoggerTest1();

	client->prepareToSend((void*)pm, serialized, &len, STORE_COMMAND_TYPE);

	PALSManager* pm1 = new PALSManager(1, 444L);
	PALSManager::deserialize(serialized+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX, pm1);

	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE, NULL);

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

void DetLoggerClient::runDetLoggerTest(DetLoggerClient *client, PALSManager* pm) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)pm, serialized, &len, STORE_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE, NULL);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

void DetLoggerClient::getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;
	vector<uint64_t> retValAsObj;
//	uint64_t *test = new uint64_t();
//	*test = 1;
//	retValAsObj.push_back(test);

	client->prepareToSend((void*)mbId, serialized, &len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

//	retValAsObj = *reinterpret_cast<vector<uint64_t*> *>(voidPointerName);

	if (retValAsObj.empty()) {
		cout << "still empty!!" << endl;
	}
//	cout << "1" << endl;
//	vector<uint64_t> vec = *retValAsObj;
//	cout << "2" << endl;
//	cout << "vec is: " << vec << endl;
	cout << "retValAsObj size is: " << retValAsObj.size() << endl;

	for (int i=0; i<retValAsObj.size(); i++) {
		cout << retValAsObj[i] << endl;
	}

	cout << "done" << endl;

}

void DetLoggerClient::getPals(DetLoggerClient *client, void* msgToSend) {

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	PALSManager retValAsObj;

	client->prepareToSend(msgToSend, serialized, &len, GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

	cout << "1" << endl;
	retValAsObj.printContent();
	cout << "done" << endl;

}


//int main(int argc, char *argv[]) {
//	DetLoggerClient::runTests("127.0.0.1");
//	return 0;
//}
