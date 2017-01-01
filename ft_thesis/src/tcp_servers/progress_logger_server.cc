/*
 * progress_logger_server.cpp
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */


#include "server.hh"
#include "../common/progressData/progress_data.hh"
#include <map>
#include <vector>

#define PORT 9096	// port to listening on

using namespace std;

typedef struct ServerProgressData {
    vector<int> packet_ids_vector;
    uint8_t index;
} ServerProgressData;



typedef map<uint16_t, ServerProgressData* >::iterator SPDIterType;

class ProgressLoggerServer : public Server {

private:
	map<uint16_t, ServerProgressData* > progressData;

	ServerProgressData* getOrCreateServerProgressData(int mbId);
	void addPacketId(ProgressData* pd, ServerProgressData* spd);
	void printState();

protected:
	void* deserializeClientRequest(char* msg, int msgLen);
	bool processRequest(void*);
	void freeDeserializedObject(void* obj);

public:
	ProgressLoggerServer(int port) : Server(port) {
		// do nothing
	}
 };

void* ProgressLoggerServer::deserializeClientRequest(char* msg, int msgLen) {
	cout << "ProgressLoggerServer::deserializeClientRequest" << endl;

	ProgressData* pd = new ProgressData;
	uint16_t *q = (uint16_t*)msg;
	pd->mbId = *q;
	q++;

	uint64_t *p = (uint64_t*)q;
	pd->packetId = *p;

	return (void*)pd;
}

bool ProgressLoggerServer::processRequest(void* obj) {
	cout << "ProgressLoggerServer::processRequest" << endl;
	ProgressData* pd = (ProgressData*)obj;

	ServerProgressData* packetIds = getOrCreateServerProgressData(pd->mbId);
	addPacketId(pd, packetIds);
	printState();

	return true;
}


void ProgressLoggerServer::addPacketId(ProgressData* pd, ServerProgressData* spd) {

	spd->packet_ids_vector.push_back(pd->packetId);
	spd->index++;
}

ServerProgressData* ProgressLoggerServer::getOrCreateServerProgressData(int mbId) {
	ServerProgressData* spd;

	cout << "progressData.count(" << mbId << "): " << progressData.count(mbId) << endl;
	if (!progressData.count(mbId)) {
		spd = new ServerProgressData();
		progressData[mbId] = spd;
	} else {
		spd = progressData[mbId];
	}

	return spd;
}

void ProgressLoggerServer::printState() {
	for (SPDIterType iter = progressData.begin(); iter != progressData.end(); iter++) {
		cout << "mbId: " << iter->first << ":" << endl;
		ServerProgressData* spd = iter->second;

		int numOfPacketIds = spd->index;

		cout << "number of packet ids: " << numOfPacketIds << "\n";

		for (int i=0; i<numOfPacketIds; i++) {
			cout << spd->packet_ids_vector[i] << ", ";
		}

		cout << "\n-------------------------------------" << endl;
	}
}



void ProgressLoggerServer::freeDeserializedObject(void* obj) {
	delete (ProgressData*)obj;
}



int main(int argc, char *argv[])
{
	cout << "start" << endl;

	ProgressLoggerServer *server = new ProgressLoggerServer(PORT);
	server->init();
	server->run();
	return 0;
}


