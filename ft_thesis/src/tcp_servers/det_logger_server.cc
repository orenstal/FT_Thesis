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

#define STORE_COMMAND_TYPE 0

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

typedef map<uint64_t, PacketData*> mbDataMap;
typedef map<uint16_t, mbDataMap> detDataMap;

typedef detDataMap::iterator MbDataIterType;
typedef map<uint64_t, PacketData*>::iterator PacketDataIterType;

class DetLoggerServer : public Server {

private:
	detDataMap detData;
	map<uint16_t, ServerProgressData* > progressData;

	// progress data
	ServerProgressData* getOrCreateServerProgressData(int mbId);
	void addPacketId(uint64_t pid, ServerProgressData* spd);
	void printProgressDataState();

	// pals content
	PacketData* getPacketData(PALSManager* pm);
	void updatePacketData(PacketData* packetData, PALSManager* pm);
	void printState();

	bool processStoreRequest(void* obj);

protected:
	void* deserializeClientRequest(char* msg, int msgLen);
	bool processRequest(void*, int command);
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

bool DetLoggerServer::processStoreRequest(void* obj) {
	cout << "DetLoggerServer::processStoreRequest" << endl;
	PALSManager* pm = (PALSManager*)obj;
	pm->printContent();
	PacketData* packetData = getPacketData(pm);
	updatePacketData(packetData, pm);

	ServerProgressData* packetIds = getOrCreateServerProgressData(pm->getMBId());
	addPacketId(pm->getPacketId(), packetIds);
	printState();

	return true;
}

bool DetLoggerServer::processRequest(void* obj, int command) {
	cout << "command is: " << command << endl;

	if (command == STORE_COMMAND_TYPE) {
		return processStoreRequest(obj);
	}

	return false;
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

	if (detData.find(pm->getMBId()) == detData.end()) {
		cout << "creating a new packetData for new mb: " << pm->getMBId() << endl;
		packetData = new PacketData();
		mbDataMap mbData;
		mbData.insert(make_pair(pm->getPacketId(), packetData));
		detData.insert(make_pair(pm->getMBId(), mbData));
	} else {
		cout << "packetData of mb: " << pm->getMBId() << " is already exist." << endl;

		if (detData[pm->getMBId()].find(pm->getPacketId()) == detData[pm->getMBId()].end()) {
			cout << "creating a new item for packet id: " << pm->getPacketId() << endl;
			packetData = new PacketData();
			detData[pm->getMBId()].insert(make_pair(pm->getPacketId(), packetData));
		} else {
			cout << "updating item for existing packet id: " << pm->getPacketId() << endl;
			packetData = detData[pm->getMBId()][pm->getPacketId()];
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

	if (progressData.find(mbId) == progressData.end()) {
		spd = new ServerProgressData();
		progressData.insert(make_pair(mbId, spd));
	} else {
		spd = progressData[mbId];
	}

	return spd;
}



void DetLoggerServer::printState() {
	cout << "-------------------------------------" << endl;
	for (MbDataIterType mbIter = detData.begin(); mbIter != detData.end(); mbIter++) {
		uint16_t mbId = mbIter->first;

		for (PacketDataIterType pdIter = mbIter->second.begin(); pdIter != mbIter->second.end(); pdIter++) {
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
