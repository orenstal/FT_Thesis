#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "../common/pal_api/pals_manager.hh"
#include "client.hh"
using namespace std;

class DetLoggerClient : public Client {

protected:
	void serializeObject(void* obj, char* serialized, int* len);

public:
	DetLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}

//	void serializePalsManager (PALSManager* pm, char* serialized, int* len);
 };

void DetLoggerClient::serializeObject(void* obj, char* serialized, int* len) {
	cout << "DetLoggerClient::serializeObject" << endl;
	PALSManager* pm = (PALSManager*)obj;

	cout << "start serializing det_logger client" << endl;
	PALSManager::serialize(pm, serialized, len);
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

void runTestAndCompare(DetLoggerClient *client) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	PALSManager* pm = prepareTest1();

	client->prepareToSend((void*)pm, serialized, &len);

	PALSManager* pm1 = new PALSManager(1, 444L);
	PALSManager::deserialize(serialized+7, pm1);

	bool isSucceed = client->sendMsgAndWait(serialized, len);

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

	delete pm;
}

void runTest(DetLoggerClient *client, PALSManager* pm) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)pm, serialized, &len);

	bool isSucceed = client->sendMsgAndWait(serialized, len);

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

	return 0;
}
