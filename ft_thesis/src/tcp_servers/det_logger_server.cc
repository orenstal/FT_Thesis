/*
 * det_logger_server.cpp
 *
 *  Created on: Dec 31, 2016
 *      Author: Tal
 */

#include "../common/pal_api/pals_manager.hh"
#include "../tcp_clients/packets_logger_client.hh"
#include "../common/replayPackets/replay_packets.hh"
#include "server.hh"
#include <map>
#include <vector>
#include <set>

#define PORT 9095	// port to listening on
#define GPAL_VAL_SIZE 50

#define SLAVE_ADDRESS "10.0.0.7"
#define SLAVE_ADDRESS_LEN 8
#define SLAVE_PORT 9999
#define SLAVE_ID 2

#define STORE_COMMAND_TYPE 0
#define GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE 1
#define GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE 2
#define DELETE_PACKETS_COMMAND_TYPE 3

using namespace std;

// todo i need to use semaphores instead of mutexes so i'll be able to use multiple readers at the same time
// todo add volatile to all indices ??

typedef struct ServerProgressData {
    vector<uint64_t> packet_ids_vector;
    uint32_t index;
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
	ServerProgressData* getOrCreateServerProgressData(uint16_t mbId);
	ServerProgressData* getServerProgressData(uint16_t mbId);
	void addPacketId(uint64_t pid, ServerProgressData* spd);
	void printProgressDataState();

	// pals content
	PacketData* getOrCreatePacketData(PALSManager* pm);
	PacketData* getPacketData(uint16_t mbId, uint64_t packId);
	void updatePacketData(PacketData* packetData, PALSManager* pm);
	void convertPacketDataToPM(PALSManager* pm, PacketData* packetData);
	void printState();

	void* deserializeClientStoreRequest(int command, char* msg, int msgLen);
	bool processStoreRequest(void* obj, char* retVal, int* retValLen);
	bool processGetProcessedPacketsRequest(void* obj, char* retVal, int* retValLen);
	void getProcessedPacketsForFirstVersion(ServerProgressData* spd, uint64_t* returnValue, int* retValLen);
	void getProcessedPacketsForAllVersions(ServerProgressData* spd, uint64_t* returnValue, int* retValLen);
	bool processGetPalsRequest(void* obj, char* retVal, int* retValLen);
	bool processDeleteFirstPacketsRequest(void* obj, char* retVal, int* retValLen);

	void deleteFirstPackets(uint16_t mbId, uint32_t totalPacketsToRemove, ServerProgressData* spd);
	void deletePacketFromMbData(mbDataMap *mbData, uint64_t packetToRemove);
	void freePacketData(PacketData* packetData);

protected:
	void* deserializeClientRequest(int command, char* msg, int msgLen);
	bool processRequest(void*, int command, char* retVal, int* retValLen);
	void freeDeserializedObject(void* obj, int command);

public:
	DetLoggerServer(int port) : Server(port) {
		// do nothing
	}
 };

void* DetLoggerServer::deserializeClientRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "DetLoggerServer::deserializeClientRequest" << endl);

	if (command == STORE_COMMAND_TYPE) {
		return deserializeClientStoreRequest(command, msg, msgLen);
	} else if (command == GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE) {
		return (void*)msg;
	} else if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		return (void*)msg;
	} else if (command == DELETE_PACKETS_COMMAND_TYPE) {
		return (void*)msg;
	}

	return NULL;
}

void* DetLoggerServer::deserializeClientStoreRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "DetLoggerServer::deserializeClientStoreRequest" << endl);
	PALSManager* pm = new PALSManager();
	PALSManager::deserialize(msg, pm);
	return (void*)pm;
}

bool DetLoggerServer::processStoreRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "DetLoggerServer::processStoreRequest" << endl);
	PALSManager* pm = (PALSManager*)obj;

	if (DEBUG) {
		pm->printContent();
	}

	PacketData* packetData = getOrCreatePacketData(pm);
	updatePacketData(packetData, pm);

	ServerProgressData* packetIds = getOrCreateServerProgressData(pm->getMBId());
	addPacketId(pm->getPacketId(), packetIds);

	if (DEBUG) {
		printState();
	} else {
		cout << "Partial Progress Data State:" << endl;
		for (SPDIterType iter = progressData.begin(); iter != progressData.end(); iter++) {
			ServerProgressData* spd = iter->second;
			cout << "mbId: " << iter->first << ", number of packet ids: " << spd->index << endl;
		}
	}

	// ack/nack will be sent anyway.
	*retValLen = 0;

	return true;
}

bool DetLoggerServer::processGetProcessedPacketsRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "DetLoggerServer::processGetProcessedPacketsRequest" << endl);

	// extract the mbId from the client request
	uint16_t mbId = 0;
	bool allVersions = true;

	// extract mbId and packId from client request
	uint16_t *q = (uint16_t*)obj;
	mbId = *q;
	q++;

	bool *p = (bool*)q;
	allVersions = *p;

	DEBUG_STDOUT(cout << "mbId is: " << mbId << ", allVersions: " << allVersions << endl);

	ServerProgressData* spd = getServerProgressData(mbId);

	if (spd == NULL) {
		DEBUG_STDOUT(cout << "WARNING: spd is NULL !!" << endl);
		*retValLen = 0;
		return true;
	}

	uint64_t* returnValue = (uint64_t*)retVal;

	if (allVersions) {
		getProcessedPacketsForAllVersions(spd, returnValue, retValLen);
	} else {
		getProcessedPacketsForFirstVersion(spd, returnValue, retValLen);
	}

	DEBUG_STDOUT(cout << "retValLen: " << *retValLen << ", spd->index: " << spd->index << ", sizeof(uint64_t*): " << sizeof(uint64_t*) << endl);

	return true;
}

void DetLoggerServer::getProcessedPacketsForFirstVersion(ServerProgressData* spd, uint64_t* returnValue, int* retValLen) {

	// todo add mutex
	vector<uint64_t> packetIds = spd->packet_ids_vector;
	// todo free mutex

	uint32_t returnedValIndex=0;
	set<uint64_t> processedBases;

	for (uint32_t i=0; i< spd->index; i++) {
		uint64_t packetIdBase = packetIds[i] >> 5;

		if (processedBases.find(packetIdBase) == processedBases.end()) {
			returnValue[returnedValIndex] = packetIds[i];
			DEBUG_STDOUT(cout << "returnValue[" << returnedValIndex << "]: " << returnValue[returnedValIndex] << endl);
			processedBases.insert(packetIdBase);
			returnedValIndex++;
		} else {
			DEBUG_STDOUT(cout << "packetIdBase: " << packetIdBase << " was already appeared. current version is: " << (packetIds[i] & 31) << endl);
		}
	}

	*retValLen = returnedValIndex * sizeof(uint64_t*);
}


void DetLoggerServer::getProcessedPacketsForAllVersions(ServerProgressData* spd, uint64_t* returnValue, int* retValLen) {

	// todo add mutex
	vector<uint64_t> packetIds = spd->packet_ids_vector;
	// todo free mutex

	for (uint32_t i=0; i< spd->index; i++) {
		returnValue[i] = packetIds[i];
		DEBUG_STDOUT(cout << "returnValue[" << returnedValIndex << "]: " << returnValue[returnedValIndex] << endl);
	}

	*retValLen = spd->index * sizeof(uint64_t*);
}



bool DetLoggerServer::processGetPalsRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "DetLoggerServer::processGetPalsRequest" << endl);
	uint16_t mbId = 0;
	uint64_t packId = 0;

	// extract mbId and packId from client request
	uint16_t *q = (uint16_t*)obj;
	mbId = *q;
	q++;

	uint64_t *p = (uint64_t*)q;
	packId = *p;

	DEBUG_STDOUT(cout << "mbId is: " << mbId << ", packId: " << packId << endl);
	PacketData* packetData = getPacketData(mbId, packId);

	if (packetData == NULL) {
		DEBUG_STDOUT(cout << "WARNING: packetData is NULL !!" << endl);
		*retValLen = 0;
		return false;
	}

	PALSManager* pm = new PALSManager(mbId, packId);
//	pm->setMBId(mbId);
//	pm->setPacketId(packId);

	convertPacketDataToPM(pm, packetData);
	PALSManager::serialize(pm, retVal, retValLen);

	DEBUG_STDOUT(cout << "retValLen: " << *retValLen << endl);

	delete pm;
	return true;
}

bool DetLoggerServer::processDeleteFirstPacketsRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "[DetLoggerServer::processDeletePacketsRequest]" << endl);
	uint16_t mbId;
	uint32_t totalPacketsToRemove = 0;	// the number of first packets to be removed

	// extract mbId from client request
	uint16_t *q = (uint16_t*)obj;
	mbId = *q;
	q++;

	uint32_t *tp = (uint32_t*)q;
	totalPacketsToRemove = *tp;
	tp++;

	DEBUG_STDOUT(cout << "mbId: " << mbId << ", total packets to delete: " << totalPacketsToRemove << endl);

	ServerProgressData* spd = getServerProgressData(mbId);

	if (spd != NULL) {
		deleteFirstPackets(mbId, totalPacketsToRemove, spd);
	}

	if (DEBUG) {
		DEBUG_STDOUT(cout << "state after deletion:" << endl;);
		printState();
	}

	// ack/nack will be sent anyway.
	*retValLen = 0;

	return true;
}



void DetLoggerServer::deleteFirstPackets(uint16_t mbId, uint32_t totalPacketsToRemove, ServerProgressData* spd) {
	DEBUG_STDOUT(cout << "[DetLoggerServer::deleteFirstPackets] Start" << endl);
	DEBUG_STDOUT(cout << "spd->index before: " << spd->index << endl);

	mbDataMap *mbData = NULL;
	if (detData.find(mbId) != detData.end()) {
		DEBUG_STDOUT(cout << "mbId " << mbId << " exist in detData" << endl);
		mbData = &(detData[mbId]);
	}

	for (int i=0; i<totalPacketsToRemove; i++) {
		uint64_t packetToRemove = spd->packet_ids_vector[i];
		deletePacketFromMbData(mbData, packetToRemove);
	}

	// todo I can reduce the memory consumption by using: http://stackoverflow.com/questions/7351899/remove-first-n-elements-from-a-stdvector

	// erase the first totalPacketsToRemove packet ids
	spd->packet_ids_vector.erase(spd->packet_ids_vector.begin(), spd->packet_ids_vector.begin() + totalPacketsToRemove);
	spd->index -= totalPacketsToRemove;

	DEBUG_STDOUT(cout << "spd->index after: " << spd->index << endl);
	DEBUG_STDOUT(cout << "[DetLoggerServer::deleteFirstPackets] End" << endl);
}

void DetLoggerServer::freePacketData(PacketData* packetData) {
	DEBUG_STDOUT(cout << "[DetLoggerServer::freePacketData] Start" << endl);
	if (packetData == NULL) {
		DEBUG_STDOUT(cout << "[DetLoggerServer::freePacketData] End (packetData is null)" << endl);
		return;
	}

	vector<gpal*>* gp_vector = &(packetData->gpal_vector);
	vector<spal*>* sp_vector = &(packetData->spal_vector);

	if (gp_vector != NULL) {
		for (int i=0; i< packetData->gpal_index; i++) {
			delete gp_vector->at(i);
			gp_vector->at(i) = NULL;
		}
	}
	DEBUG_STDOUT(cout << "[DetLoggerServer::freePacketData] gpals were freed" << endl);

	if (sp_vector != NULL) {
		for (int i=0; i< packetData->spal_index; i++) {
			delete sp_vector->at(i);
			sp_vector->at(i) = NULL;
		}
	}
	DEBUG_STDOUT(cout << "[DetLoggerServer::freePacketData] spals were freed" << endl);

	delete packetData;

	DEBUG_STDOUT(cout << "[DetLoggerServer::freePacketData] Start" << endl);
}


void DetLoggerServer::deletePacketFromMbData(mbDataMap *mbData, uint64_t packetToRemove) {
	DEBUG_STDOUT(cout << "[DetLoggerServer::deletePacketFromMbData] Start" << endl);

	if (mbData != NULL) {
		DEBUG_STDOUT(cout << "packetToRemove: " << packetToRemove << endl);

		if (mbData->find(packetToRemove) != mbData->end()) {
			DEBUG_STDOUT(cout << "packetToRemove: " << packetToRemove << endl);
			PacketData *packetData = mbData->at(packetToRemove);

			freePacketData(packetData);
			mbData->erase(packetToRemove);
		}
	}
	DEBUG_STDOUT(cout << "[DetLoggerServer::deletePacketFromMbData] End" << endl);
}


bool DetLoggerServer::processRequest(void* obj, int command, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "command is: " << command << endl);

	if (command == STORE_COMMAND_TYPE) {
		return processStoreRequest(obj, retVal, retValLen);
	} else if (command == GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE) {
		return processGetProcessedPacketsRequest(obj, retVal, retValLen);
	} else if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		return processGetPalsRequest(obj, retVal, retValLen);
	} else if (command == DELETE_PACKETS_COMMAND_TYPE) {
		return processDeleteFirstPacketsRequest(obj, retVal, retValLen);
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

void DetLoggerServer::convertPacketDataToPM(PALSManager* pm, PacketData* packetData) {
	DEBUG_STDOUT(cout << "DetLoggerServer::convertPacketDataToPM" << endl);

	uint8_t gpalsLen = packetData->gpal_index;
	DEBUG_STDOUT(cout << "gpalsLen: " << unsigned(gpalsLen) << endl);

	for (uint8_t i=0; i<gpalsLen; i++) {
		gpal* currGPal = packetData->gpal_vector[i];
		pm->createGPalAndAdd(currGPal->var_id, currGPal->val);

		DEBUG_STDOUT(cout << "gpal[" << unsigned(i) << "]->var_id: " << currGPal->var_id << endl);
	}
	DEBUG_STDOUT(cout << " done gpals" << endl);

	uint8_t spalsLen = packetData->spal_index;
	DEBUG_STDOUT(cout << "spalsLen: " << unsigned(spalsLen) << endl);

	for (uint8_t i=0; i<spalsLen; i++) {
		spal* currSPal = packetData->spal_vector[i];
		pm->createSPalAndAdd(currSPal->var_id, currSPal->seq_num);

		DEBUG_STDOUT(cout << "spal[" << unsigned(i) << "]->var_id: " << currSPal->var_id << endl);
	}

	DEBUG_STDOUT(cout << "[DetLoggerServer::convertPacketDataToPM] Done" << endl);
}

PacketData* DetLoggerServer::getOrCreatePacketData(PALSManager* pm) {
	PacketData* packetData;

	if (detData.find(pm->getMBId()) == detData.end()) {
		DEBUG_STDOUT(cout << "creating a new packetData for new mb: " << pm->getMBId() << endl);
		packetData = new PacketData();
		mbDataMap mbData;
		mbData.insert(make_pair(pm->getPacketId(), packetData));
		detData.insert(make_pair(pm->getMBId(), mbData));
	} else {
		DEBUG_STDOUT(cout << "packetData of mb: " << pm->getMBId() << " is already exist." << endl);

		if (detData[pm->getMBId()].find(pm->getPacketId()) == detData[pm->getMBId()].end()) {
			DEBUG_STDOUT(cout << "creating a new item for packet id: " << pm->getPacketId() << endl);
			packetData = new PacketData();
			detData[pm->getMBId()].insert(make_pair(pm->getPacketId(), packetData));
		} else {
			DEBUG_STDOUT(cout << "updating item for existing packet id: " << pm->getPacketId() << endl);
			packetData = detData[pm->getMBId()][pm->getPacketId()];
		}
	}

	return packetData;
}

PacketData* DetLoggerServer::getPacketData(uint16_t mbId, uint64_t packId) {
	DEBUG_STDOUT(cout << "DetLoggerServer::getPacketData" << endl);
	PacketData* packetData = NULL;

	if (detData.find(mbId) != detData.end()) {
		DEBUG_STDOUT(cout << "mbId " << mbId << " exist in detData" << endl);

		if (detData[mbId].find(packId) != detData[mbId].end()) {
			DEBUG_STDOUT(cout << "packId " << packId << " exist in detData[mbId]" << endl);
			packetData = detData[mbId][packId];
		}
	}

	return packetData;
}


void DetLoggerServer::addPacketId(uint64_t pid, ServerProgressData* spd) {
	spd->packet_ids_vector.push_back(pid);
	spd->index++;
}

ServerProgressData* DetLoggerServer::getOrCreateServerProgressData(uint16_t mbId) {
	ServerProgressData* spd;

	if (progressData.find(mbId) == progressData.end()) {
		spd = new ServerProgressData();
		progressData.insert(make_pair(mbId, spd));
	} else {
		spd = progressData[mbId];
	}

	return spd;
}

ServerProgressData* DetLoggerServer::getServerProgressData(uint16_t mbId) {

	if (progressData.find(mbId) != progressData.end()) {
		return progressData[mbId];
	}

	return NULL;
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



void DetLoggerServer::freeDeserializedObject(void* obj, int command) {
	if (command == STORE_COMMAND_TYPE) {
		delete (PALSManager*)obj;
	} else if (command == GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE) {
		// do nothing
	} else if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		// do nothing
	} else if (command == DELETE_PACKETS_COMMAND_TYPE) {
		// do nothing
	}
}



int main(int argc, char *argv[])
{
	cout << "start" << endl;

	DetLoggerServer *server = new DetLoggerServer(PORT);
	server->init();
	server->run();

	delete server;
	return 0;
}
