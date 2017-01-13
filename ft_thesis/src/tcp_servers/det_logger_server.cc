/*
 * det_logger_server.cpp
 *
 *  Created on: Dec 31, 2016
 *      Author: Tal
 */

#include "../common/pal_api/pals_manager.hh"
#include "server.hh"
#include <map>
#include <vector>

#define PORT 9095	// port to listening on
#define GPAL_VAL_SIZE 50

using namespace std;


typedef struct ServerProgressData {
    vector<int> packet_ids_vector;
    uint8_t index;
} ServerProgressData;


typedef struct PacketData {
    vector<spal*> spal_vector;
    vector<gpal*> gpal_vector;
    uint8_t spal_index;
    uint8_t gpal_index;
} PacketData;

typedef map<uint16_t, ServerProgressData* >::iterator SPDIterType;

typedef map<uint16_t, map<uint64_t, PacketData*> >::iterator MbDataIterType;
typedef map<uint64_t, PacketData*>::iterator PacketDataIterType;

class DetLoggerServer : public Server {

private:
	map<uint16_t, map<uint64_t, PacketData*> > detData;
	map<uint16_t, ServerProgressData* > progressData;

	// progress data
	ServerProgressData* getOrCreateServerProgressData(int mbId);
	void addPacketId(uint64_t pid, ServerProgressData* spd);
	void printProgressDataState();

	// pals content
	PacketData* getPacketData(PALSManager* pm);
	void updatePacketData(PacketData* packetData, PALSManager* pm);
	void printState();

protected:
	void* deserializeClientRequest(char* msg, int msgLen);
	bool processRequest(void*);
	void freeDeserializedObject(void* obj);

public:
	DetLoggerServer(int port) : Server(port) {
		// do nothing
	}
 };

void* DetLoggerServer::deserializeClientRequest(char* msg, int msgLen) {
	cout << "DetLoggerServer::deserializeClientRequest" << endl;
	PALSManager* pm = new PALSManager();
	PALSManager::deserialize(msg, pm);

	return (void*)pm;
}

bool DetLoggerServer::processRequest(void* obj) {
	cout << "DetLoggerServer::processRequest" << endl;
	PALSManager* pm = (PALSManager*)obj;
	pm->printContent();

	PacketData* packetData = getPacketData(pm);
	updatePacketData(packetData, pm);

	ServerProgressData* packetIds = getOrCreateServerProgressData(pm->getMBId());
	addPacketId(pm->getPacketId(), packetIds);
	printState();

	return true;
}

void DetLoggerServer::updatePacketData(PacketData* packetData, PALSManager* pm) {
	// appending gpals
	gpal* newGPalsList = pm->getGPalList();
	for (int i=0; i< pm->getGPalSize(); i++) {
		gpal* gp = new gpal();
		gp->var_id = newGPalsList[i].var_id;
		memset(gp->val, 0, GPAL_VAL_SIZE);

		for (int j=0; j<GPAL_VAL_SIZE; j++) {
			gp->val[j] = newGPalsList[i].val[j];
		}
		packetData->gpal_vector.push_back(gp);
		packetData->gpal_index++;
	}

	// appending spals
	spal* newSPalsList = pm->getSPalList();
	for (int i=0; i< pm->getSPalSize(); i++) {
		spal* sp = new spal();
		sp->var_id = newSPalsList[i].var_id;
		sp->seq_num = newSPalsList[i].seq_num;

		packetData->spal_vector.push_back(sp);
		packetData->spal_index++;
	}
}

PacketData* DetLoggerServer::getPacketData(PALSManager* pm) {
	PacketData* packetData;

	cout << "detData.count(" << pm->getMBId() << "): " << detData.count(pm->getMBId()) << endl;
	if (!detData.count(pm->getMBId())) {
		packetData = new PacketData();
		map<uint64_t, PacketData*> mbData;
		mbData[pm->getPacketId()] = packetData;

		detData[pm->getMBId()] = mbData;
	} else {

		map<uint64_t, PacketData*> mbData = detData[pm->getMBId()];

		if (!mbData.count(pm->getPacketId())) {
			packetData = new PacketData();
			mbData[pm->getPacketId()] = packetData;
		} else {
			packetData = mbData[pm->getPacketId()];
		}
	}

	return packetData;
}


void DetLoggerServer::addPacketId(uint64_t pid, ServerProgressData* spd) {

	spd->packet_ids_vector.push_back(pid);
	spd->index++;
}

ServerProgressData* DetLoggerServer::getOrCreateServerProgressData(int mbId) {
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



void DetLoggerServer::printState() {
	for (MbDataIterType mbIter = detData.begin(); mbIter != detData.end(); mbIter++) {
		uint16_t mbId = mbIter->first;
		map<uint64_t, PacketData*> mbData = mbIter->second;

		for (PacketDataIterType pdIter = mbData.begin(); pdIter != mbData.end(); pdIter++) {
			cout << "mbId: " << mbId << ", packetId: " << pdIter->first << ":" << endl;
			PacketData* pd = pdIter->second;

			int gpalSize = pd->gpal_index;
			int spalSize = pd->spal_index;

			cout << "-------------------------------------" << endl;
			cout << "gpal list size: " << gpalSize << "\n";

			for (int i=0; i<gpalSize; i++) {
				cout << "gpal[" << i << "]\t" << pd->gpal_vector[i]->var_id << "\t";
				string val = string(pd->gpal_vector[i]->val);
				cout << val << endl;
			}

			cout << "\nspal list size: " << spalSize << "\n";

			for (int i=0; i<spalSize; i++) {
				cout << "spal[" << i << "]\t" << pd->spal_vector[i]->var_id << "\t" << pd->spal_vector[i]->seq_num << endl;
			}

			cout << "-------------------------------------" << endl;
		}
	}

	printProgressDataState();
}

void DetLoggerServer::printProgressDataState() {
	cout << "Progress Data State:" << endl;
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



void DetLoggerServer::freeDeserializedObject(void* obj) {
	delete (PALSManager*)obj;
}



int main(int argc, char *argv[])
{
	cout << "start" << endl;

	DetLoggerServer *server = new DetLoggerServer(PORT);
	server->init();
	server->run();
	return 0;
}
