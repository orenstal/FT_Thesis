/*
 * recovery.cpp
 *
 *  Created on: Jan 15, 2017
 *      Author: Tal
 */


#ifndef DET_LOGGER_CLIENT_CC_
#define DET_LOGGER_CLIENT_CC_

#ifndef PACKETS_LOGGER_CLIENT_CC_
#define PACKETS_LOGGER_CLIENT_CC_


#include "server.hh"
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "../tcp_clients/det_logger_client.hh"
#include "../tcp_clients/packets_logger_client.hh"
#include "../common/replayPackets/replay_packets.hh"
#include "../common/deletePackets/delete_packets.hh"
#include "../tcp_clients/client.hh"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <set>
#include <inttypes.h>
#include <limits>

#define STORE_COMMAND_TYPE 0
#define GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE 1
#define GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE 2
#define DELETE_PACKETS_COMMAND_TYPE 3

#define REPLAY_PACKETS_TSA_COMMAND_TYPE 4
#define CLEAR_PACKETS_TSA_COMMAND_TYPE 5
#define REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE 6
#define STOP_MASTER_TSA_COMMAND_TYPE 7

#define DET_LOGGER_SERVER_PORT 9095
#define DET_LOGGER_SERVER_ADDRESS "10.0.0.5" // "127.0.0.1"
#define DET_LOGGER_SLAVE_SERVER_ADDRESS "10.0.0.8"	// "127.0.0.1"
#define PACKET_LOGGER_SERVER_PORT 9097
#define PACKET_LOGGER_SERVER_ADDRESS "10.0.0.4"	//"127.0.0.1"
#define MANAGER_ADDRESS "10.0.0.9"	//"127.0.0.1"
#define MB_PORT 9999

#define CLEANUP_MANAGER_PORT 9001

using namespace std;

typedef struct MbData {
    uint16_t mbId;
    char ipAddress[MAX_ADDRESS_LEN];
    uint8_t addressLen;
    int port;

} MbData;

class DetLoggerClient;
class PacketLoggerClient;



class CleanupManagerServer : public Server {
private:
	map<uint16_t, MbData* > mbData;
	map<uint16_t, uint16_t > masterSlaveMapping;

	string command;

	pthread_spinlock_t commandLock;

	DetLoggerClient *detLoggerClient;
	DetLoggerClient *slaveDetLoggerClient;
	PacketLoggerClient *packetLoggerClient;

	MbData* getSlaveMbData(uint16_t masterMbId);

	static vector<uint64_t>* getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId, bool allVersions);
	static void* prepareGetProcessedPackets(uint16_t mbId, bool allVersions);

	static vector<uint64_t>* getUnprocessedPacketIds(
	vector<uint64_t>* masterProcessedPacketIds, vector<uint64_t>* slaveProcessedPacketIds, set<uint64_t> *alreadyReplayedPackets);

	static vector<uint64_t>* getCommonProcessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
	vector<uint64_t>* slaveProcessedPacketIds, int maxPacketsToDelete, set<uint64_t> *alreadyReplayedPackets, vector<uint64_t>* orphanSlavePackets);

	static bool addPacket(set<int> *packetBasesToDelete, vector<uint64_t>* commonProcessed, set<uint64_t> *alreadyReplayedPackets, int maxPacketsBasesToDelete, uint64_t packetToAdd);

	static void setSlaveOrphanPacketIds(uint64_t packetId, vector<uint64_t>::iterator slaveIter,
				vector<uint64_t>* slaveProcessedPacketIds, vector<uint64_t>* orphanSlavePackets);

	ReplayPackets* createReplayPackets(MbData* slaveData, vector<uint64_t>* unporcessedPacketIds);
	void sendReplayPacketsRequest(PacketLoggerClient *client, ReplayPackets* replayPacketsData);

	DeletePackets* createDeletePacketsData(vector<uint64_t>* packetIdsToDelete);
	void sendDeletePacketsPacketsRequest(PacketLoggerClient *client, DeletePackets* deletePacketsData);

	int connectToPacketLoggerServer(char* mbAddress, int mbPort);
	void deleteFirstPacketsRequest(DetLoggerClient *client, uint16_t mbId, uint32_t totalFirstPacketsToBeDeleted);

	void* deserializeTsaMbId(char* msg);
	void* deserializeStopMasterTsaRequest(int command, char* msg, int msgLen);

	bool replayAndClearPackets(uint16_t masterMbId);
	bool processTsaReplayRequest(void* obj, char* retVal, int* retValLen);
	bool processTsaClearRequest(void* obj, char* retVal, int* retValLen);
	bool processTsaReplayAndClearRequest(void* obj, char* retVal, int* retValLen);
	bool processTsaStopMasterRequest(void* obj, char* retVal, int* retValLen);

	uint16_t getMbIdAs16BitsInt(char *mbId);

protected:
	void* deserializeClientRequest(int command, char* msg, int msgLen);
	bool processRequest(void*, int command, char* retVal, int* retValLen);
	void freeDeserializedObject(void* obj, int command);

public:
	CleanupManagerServer(int port) : Server(port) {
		initSpinLock(&commandLock);
	}

	void initCleanupServer();
	void freeMbData();
	int replay(uint16_t masterMbId, set<uint64_t> *alreadyReplayedPackets);
	int clearReplayedPackets(uint16_t masterMbId, int maxPacketsBasesToDelete, set<uint64_t> *alreadyReplayedPackets);

	int runReplayTest(set<uint64_t> *alreadyReplayedPackets, uint16_t masterMbId);
	void runClearTest(int maxPacketsToDelete, set<uint64_t> *alreadyReplayedPackets, uint16_t masterMbId);
	void runManagerAutomatically(uint16_t masterMbId, int timeIntervalInMs);
	void runManagerManually(uint16_t masterMbId);

	static uint16_t getMasterMbId(string command);

	void connectToServersForRecovery();
};


void* CleanupManagerServer::deserializeStopMasterTsaRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "[CleanupManagerServer::deserializeStopMasterTsaRequest] Start" << endl);
//	cout << "[CleanupManagerServer::deserializeStopMasterTsaRequest] Start" << endl;

	char* inputs = new char[2*sizeof(uint16_t)];
	memset(inputs, '\0', 2*sizeof(uint16_t));

	char mbId[msgLen];
	memset(mbId, '\0', msgLen);

	int i=0;
	for (; i<msgLen; i++) {
		if (msg[i] == ',') {
			break;
		} else {
			mbId[i] = msg[i];
		}
	}
	uint16_t oldMasterMbId = getMbIdAs16BitsInt(mbId);

	memset(mbId, '\0', msgLen);
	int j=0;
	for (i++; i<msgLen; i++, j++) {
		mbId[j] = msg[i];
	}

	uint16_t newMasterMbId = getMbIdAs16BitsInt(mbId);

	uint16_t *q = (uint16_t*)inputs;
	*q = oldMasterMbId;
	q++;
	*q = newMasterMbId;

//	cout << "[CleanupManagerServer::deserializeStopMasterTsaRequest] End" << endl;
	return (void*)inputs;
}

void* CleanupManagerServer::deserializeTsaMbId(char* msg) {
	DEBUG_STDOUT(cout << "[CleanupManagerServer::deserializeTsaMbId] Start" << endl);
	cout << "[CleanupManagerServer::deserializeTsaMbId] Start" << endl;
	cout << "msg: " << msg << endl;

	char* input = new char[sizeof(uint16_t)];
	memset(input, '\0', sizeof(uint16_t));

	uint16_t mbId = getMbIdAs16BitsInt(msg);

	uint16_t *q = (uint16_t*)input;
	*q = mbId;

	cout << "mbId: " << mbId << endl;
	cout << "[CleanupManagerServer::deserializeTsaMbId] End" << endl;
	return (void*)input;
}

uint16_t CleanupManagerServer::getMbIdAs16BitsInt(char *mbId) {
	cout << "mbId: " << mbId << endl;
	int mb = atoi(mbId);
	uint16_t mb16Bits = mb;

	cout << "mb as int: " << mb << endl;
	cout << "mb as 16 bits int: " << mb16Bits << endl;
	cout << "sizeof(mb16Bits) == sizeof(uint16_t)? " << (sizeof(mb16Bits) == sizeof(uint16_t)) << endl;

	return mb;
}

void* CleanupManagerServer::deserializeClientRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "Manager::deserializeClientStoreRequest" << endl);
	cout << "Manager::deserializeClientRequest" << endl;
	cout << "command: " << command << endl;

	if (command == REPLAY_PACKETS_TSA_COMMAND_TYPE) {
		return deserializeTsaMbId(msg);
	} else if (command == CLEAR_PACKETS_TSA_COMMAND_TYPE){
		return deserializeTsaMbId(msg);
	} else if (command == REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE){
		return deserializeTsaMbId(msg);
	} else if (command == STOP_MASTER_TSA_COMMAND_TYPE){
		return deserializeStopMasterTsaRequest(command, msg, msgLen);
	}

	cout << "WARNING: invalid command !!" << endl;
	return NULL;
}

bool CleanupManagerServer::replayAndClearPackets(uint16_t masterMbId) {
	set<uint64_t> *alreadyReplayedPackets = new set<uint64_t>;

	DEBUG_STDOUT(cout << "start replaying masterMbId: " << masterMbId << endl);
	cout << "Invoke replay master mb id: " << masterMbId << endl;

	int totalReplayedPackets = replay(masterMbId, alreadyReplayedPackets);
	cout << "Invoke clear" << endl;
	int totalDeletedPackets = clearReplayedPackets(masterMbId, totalReplayedPackets, alreadyReplayedPackets);

	alreadyReplayedPackets->clear();
	delete alreadyReplayedPackets;

	bool hadPacketsToReplayOrDelete = (totalReplayedPackets > 0 || totalDeletedPackets > 0);
	return hadPacketsToReplayOrDelete;
}

bool CleanupManagerServer::processTsaReplayAndClearRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "[Manager::processTsaReplayAndClearRequest] Start" << endl);
	uint16_t masterMbId = 0;

	// extract master mb id from tsa client request
	int *p = (int*)obj;
	masterMbId = *p;

	replayAndClearPackets(masterMbId);

	DEBUG_STDOUT(cout << "[Manager::processTsaReplayAndClearRequest] End" << endl);
	return true;
}

bool CleanupManagerServer::processTsaReplayRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "[Manager::processTsaReplayRequest] Start" << endl);
	uint16_t masterMbId = 0;

	// extract master mb id from tsa client request
	int *p = (int*)obj;
	masterMbId = *p;


	set<uint64_t> alreadyReplayedPackets;

	DEBUG_STDOUT(cout << "start replaying masterMbId: " << masterMbId << endl);
	cout << "Invoke replay master mb id: " << masterMbId << endl;

	replay(masterMbId, &alreadyReplayedPackets);

	DEBUG_STDOUT(cout << "[Manager::processTsaReplayRequest] End" << endl);
	return true;
}

bool CleanupManagerServer::processTsaClearRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "[Manager::processTsaClearRequest] Start" << endl);
	uint16_t masterMbId = 0;

	// extract master mb id from tsa client request
	int *p = (int*)obj;
	masterMbId = *p;

	set<uint64_t> alreadyReplayedPackets;
	cout << "Invoke clear" << endl;
	clearReplayedPackets(masterMbId, numeric_limits<int>::max(), &alreadyReplayedPackets);

	DEBUG_STDOUT(cout << "[Manager::processTsaClearRequest] End" << endl);
	return true;
}

bool CleanupManagerServer::processTsaStopMasterRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "[Manager::processTsaClearRequest] Start" << endl);
	uint16_t oldMasterMbId = 0;
	uint16_t newMasterMbId = 0;

	// extract master mb id from tsa client request
	uint16_t *p = (uint16_t*)obj;
	oldMasterMbId = *p;
	p++;
	newMasterMbId = *p;

	DEBUG_STDOUT(cout << "start replaying masterMbId: " << masterMbId << endl);
	cout << "Invoke replay master mb id: " << oldMasterMbId << endl;

	bool hasPacketsFromOldMaster = replayAndClearPackets(oldMasterMbId);

	cout << "*** wait until all master mb will be replayed and cleared before switching master mb" << endl;
	while (hasPacketsFromOldMaster) {
		hasPacketsFromOldMaster = replayAndClearPackets(oldMasterMbId);
	}
	cout << "*** all master packets were played and cleared! Start switching master: " << oldMasterMbId << " to new master: " << newMasterMbId << endl;

	replayAndClearPackets(newMasterMbId);

	DEBUG_STDOUT(cout << "[Manager::processTsaClearRequest] End" << endl);
	return true;
}


bool CleanupManagerServer::processRequest(void* obj, int command, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "command is: " << command << endl);

	if (command == REPLAY_PACKETS_TSA_COMMAND_TYPE) {
		return processTsaReplayRequest(obj, retVal, retValLen);
	} else if (command == CLEAR_PACKETS_TSA_COMMAND_TYPE) {
		return processTsaClearRequest(obj, retVal, retValLen);
	} else if (command == REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE) {
		return processTsaReplayAndClearRequest(obj, retVal, retValLen);
	} else if (command == STOP_MASTER_TSA_COMMAND_TYPE) {
		return processTsaStopMasterRequest(obj, retVal, retValLen);
	}

	return false;
}


void CleanupManagerServer::freeDeserializedObject(void* obj, int command) {

	if (command == REPLAY_PACKETS_TSA_COMMAND_TYPE) {
		delete (char*)obj;
	} else if (command == CLEAR_PACKETS_TSA_COMMAND_TYPE) {
		delete (char*)obj;
	} else if (command == REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE){
		delete (char*)obj;
	} else if (command == STOP_MASTER_TSA_COMMAND_TYPE) {
		delete (char*)obj;
	}
}





void CleanupManagerServer::initCleanupServer() {
	masterSlaveMapping.insert(make_pair(6,7));
	masterSlaveMapping.insert(make_pair(7,10));
	MbData* mb6Data = new MbData;
	mb6Data->mbId = 6;
	mb6Data->addressLen = 8;
	memset(mb6Data->ipAddress, '\0', MAX_ADDRESS_LEN);
	memcpy(mb6Data->ipAddress, "10.0.0.6", mb6Data->addressLen);
	mb6Data->port = MB_PORT;
	mbData.insert(make_pair(6, mb6Data));

	MbData* mb7Data = new MbData;
	mb7Data->mbId = 7;
	mb7Data->addressLen = 8;
	memset(mb7Data->ipAddress, '\0', MAX_ADDRESS_LEN);
	memcpy(mb7Data->ipAddress, "10.0.0.7", mb7Data->addressLen);
	mb7Data->port = MB_PORT;
	mbData.insert(make_pair(7, mb7Data));

	MbData* mb10Data = new MbData;
	mb10Data->mbId = 10;
	mb10Data->addressLen = 9;
	memset(mb10Data->ipAddress, '\0', MAX_ADDRESS_LEN);
	memcpy(mb10Data->ipAddress, "10.0.0.10", mb10Data->addressLen);
	mb10Data->port = MB_PORT;
	mbData.insert(make_pair(10, mb10Data));

	detLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SERVER_ADDRESS);
	slaveDetLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SLAVE_SERVER_ADDRESS);
	packetLoggerClient = new PacketLoggerClient(PACKET_LOGGER_SERVER_PORT, PACKET_LOGGER_SERVER_ADDRESS);
	connectToServersForRecovery();

}

void CleanupManagerServer::freeMbData() {
	delete mbData[6];
	delete mbData[7];
	delete mbData[10];
}

int CleanupManagerServer::replay(uint16_t masterMbId, set<uint64_t> *alreadyReplayedPackets) {
	printf("[Manager::replay] Start\n");

	int totalReplayedPackets = 0;
	MbData* slaveData = getSlaveMbData(masterMbId);

	if (slaveData == NULL) {
		printf("ERROR: failed to get slave mb data\n");
		return -1;
	}

	printf("slaveData id is: %" PRIu16 ", address is: %s, port is: %d\n", slaveData->mbId, slaveData->ipAddress, slaveData->port);

	vector<uint64_t>* masterProcessedPacketIds = CleanupManagerServer::getProcessedPacketIds(detLoggerClient, &masterMbId, false);
	uint32_t numOfFirstPacketsToBeReplayed = masterProcessedPacketIds->size();

	uint16_t slaveMbId = slaveData->mbId;
	vector<uint64_t>* slaveProcessedPacketIds = CleanupManagerServer::getProcessedPacketIds(slaveDetLoggerClient, &slaveMbId, false);
	vector<uint64_t>* unporcessedPacketIds = getUnprocessedPacketIds(masterProcessedPacketIds, slaveProcessedPacketIds, alreadyReplayedPackets);

	if (unporcessedPacketIds->size() > 0) {
		ReplayPackets* replayPackets = createReplayPackets(slaveData, unporcessedPacketIds);
		sendReplayPacketsRequest(packetLoggerClient, replayPackets);
		DEBUG_STDOUT(printf("about to delete replayPackets\n"););
		delete replayPackets;

		totalReplayedPackets = unporcessedPacketIds->size();
	}

	printf("%d packets were replayed successfully\n", unporcessedPacketIds->size());

	delete masterProcessedPacketIds;
	delete slaveProcessedPacketIds;

	printf("[Manager::replay] End\n");

	return totalReplayedPackets;
}


int CleanupManagerServer::clearReplayedPackets(uint16_t masterMbId, int maxPacketsBasesToDelete, set<uint64_t> *alreadyReplayedPackets) {
	printf("[Manager::clearReplayedPackets] Start\n");

	int totalDeletedPackets = 0;
	vector<uint64_t>* commonProcessedPacketIds;
	MbData* slaveData = getSlaveMbData(masterMbId);

	if (slaveData == NULL) {
		printf("ERROR: failed to get slave mb data\n");
		return -1;
	}

	printf("slaveData id is: %" PRIu16 ", address is: %s, port is: %d\n", slaveData->mbId, slaveData->ipAddress, slaveData->port);

	vector<uint64_t>* orphanSlavePackets = new vector<uint64_t>;
	vector<uint64_t>* masterProcessedPacketIds = CleanupManagerServer::getProcessedPacketIds(detLoggerClient, &masterMbId, true);

	uint16_t slaveMbId = slaveData->mbId;
	vector<uint64_t>* slaveProcessedPacketIds = CleanupManagerServer::getProcessedPacketIds(slaveDetLoggerClient, &slaveMbId, true);

	if (masterProcessedPacketIds->size() == 0) {
		// we want to delete orphan packets (although we don't expect to reach this code when slave.size>0)
		commonProcessedPacketIds =
				getCommonProcessedPacketIds(slaveProcessedPacketIds, slaveProcessedPacketIds, numeric_limits<int>::max(), alreadyReplayedPackets, orphanSlavePackets);
	} else {
		commonProcessedPacketIds =
				getCommonProcessedPacketIds(masterProcessedPacketIds, slaveProcessedPacketIds, numeric_limits<int>::max(), alreadyReplayedPackets, orphanSlavePackets);
	}

	if (commonProcessedPacketIds->size() == 0 && masterProcessedPacketIds->size() < 2*slaveProcessedPacketIds->size()) {
		// this case shouldn't happen - it means that we have a problem in which the lists are not cleared never (the number 2 is
		// the number of versions for each packet base).
		printf("about to delete all packets (%d) from master det logger\n", masterProcessedPacketIds->size());
		deleteFirstPacketsRequest(detLoggerClient, masterMbId, masterProcessedPacketIds->size());
		printf("about to delete all packets (%d) from slave det logger\n", slaveProcessedPacketIds->size());
		deleteFirstPacketsRequest(slaveDetLoggerClient, slaveMbId, slaveProcessedPacketIds->size());
	} else {
		// this part is for regular behavior
		if (commonProcessedPacketIds->size() > 0) {
			printf("about to delete first packets from det logger\n");
			deleteFirstPacketsRequest(detLoggerClient, masterMbId, commonProcessedPacketIds->size());
			printf("DONE - about to delete first packets from det logger\n");
		}

		if (orphanSlavePackets->size() > 0) {
			commonProcessedPacketIds->insert(commonProcessedPacketIds->end(), orphanSlavePackets->begin(), orphanSlavePackets->end());
		}

		if (commonProcessedPacketIds->size() > 0) {
	//		deleteFirstPacketsRequest(detLoggerClient, masterMbId, commonProcessedPacketIds->size());
			printf("about to delete first packets from slave det logger\n");
			deleteFirstPacketsRequest(slaveDetLoggerClient, slaveMbId, commonProcessedPacketIds->size());
			printf("DONE - about to delete first packets from slave det logger\n");

			printf("creating DeletePackets object\n");
			DeletePackets* deletePacketsData = createDeletePacketsData(commonProcessedPacketIds);
			printf("DONE - creating DeletePackets object\n");
			sendDeletePacketsPacketsRequest(packetLoggerClient, deletePacketsData);
			printf("SONE - sending deletePackets to packet logger\n");
			delete deletePacketsData;
		}

		printf("%d packets were deleted successfully\n", commonProcessedPacketIds->size());
		totalDeletedPackets = commonProcessedPacketIds->size();
	}


	printf("[Manager::clearReplayedPackets] End\n");

	delete masterProcessedPacketIds;
	delete slaveProcessedPacketIds;
	delete commonProcessedPacketIds;
	delete orphanSlavePackets;

	return totalDeletedPackets;
}



int CleanupManagerServer::connectToPacketLoggerServer(char* serverAddress, int serverPort) {
	DEBUG_STDOUT(printf("Manager::connectToPacketLoggerServer\n"));
	DEBUG_STDOUT(printf("serverAddress is: %s, serverPort: %d\n", serverAddress, serverPort));

	struct sockaddr_in sock_addr_dst_server;
	sock_addr_dst_server.sin_family = AF_INET;
	sock_addr_dst_server.sin_addr.s_addr = inet_addr(serverAddress);
	sock_addr_dst_server.sin_port = htons(serverPort);
	memset(&(sock_addr_dst_server.sin_zero), '\0', 8);

	//Create socket
	int sockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (sockfd == -1) {
		printf("ERROR: Could not create socket\n");
		return sockfd;
	}

	// activate keep-alive mechanism
//	int val = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_dst_server , sizeof(sock_addr_dst_server)) < 0) {
		printf("ERROR: Connect failed.\n");
		return -1;
	}

	if (sockfd <= 0) {
		return -1;
	}

	printf("Connected successfully to packet logger server (%s) via sockfd %d\n", serverAddress, sockfd);
	return sockfd;
}


void CleanupManagerServer::sendReplayPacketsRequest(PacketLoggerClient *client, ReplayPackets* replayPacketsData) {
	DEBUG_STDOUT(printf("[Manager::sendReplayPacketsRequest] Start\n"));

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	char* retValAsObj = NULL;

	client->prepareToSend((void*)replayPacketsData, serialized, &len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		DEBUG_STDOUT(cout << "succeed to send" << endl);
	} else {
		DEBUG_STDOUT(cout << "failed to send" << endl);
	}
	DEBUG_STDOUT(printf("[Manager::sendReplayPacketsRequest] End\n"));
}


ReplayPackets* CleanupManagerServer::createReplayPackets(MbData* slaveData, vector<uint64_t>* unporcessedPacketIds) {
	DEBUG_STDOUT(printf("[Manager::createReplayPackets] Start\n"));

	ReplayPackets* replayPackets = new ReplayPackets;
	replayPackets->mbId = slaveData->mbId;
	replayPackets->port = slaveData->port;
	replayPackets->addressLen = slaveData->addressLen;
	memset(replayPackets->address, '\0', MAX_ADDRESS_LEN);
	memcpy(replayPackets->address, slaveData->ipAddress, replayPackets->addressLen);

	replayPackets->packetIds = unporcessedPacketIds;

	if (DEBUG) {
		printf("replayPackets->mbId: %d, port: %d, addressLen: %d\n", replayPackets->mbId, replayPackets->port, replayPackets->addressLen);
		for (int i=0; i< MAX_ADDRESS_LEN; i++) {
			printf("%c", replayPackets[i]);
		}
		printf("\n");
		fflush(stdout);
	}

	if (unporcessedPacketIds == NULL) {
		DEBUG_STDOUT(printf("WARNING: replayPackets->packetIds is NULL !!\n"));
	}

	if (DEBUG) {
		printf("unporcessedPacketIds size is: %d", unporcessedPacketIds->size());
		printf("unporcessedPacketIds packet ids are: \n");

		for (int i=0; i<unporcessedPacketIds->size(); i++) {
			printf("%" PRIu64 ", ", unporcessedPacketIds->at(i));
		}

		printf("size is: %d", replayPackets->packetIds->size());
		printf("packet ids are: \n");

		for (int i=0; i<replayPackets->packetIds->size(); i++) {
			printf("%" PRIu64 ", ", replayPackets->packetIds->at(i));
		}

		printf("\n");
		printf("[Manager::createReplayPackets] End\n");
		fflush(stdout);
	}

	return replayPackets;
}

DeletePackets* CleanupManagerServer::createDeletePacketsData(vector<uint64_t>* packetIdsToDelete) {
	DEBUG_STDOUT(printf("[Manager::createDeletePacketsData] Start\n"));

	DeletePackets* deletePacketsData = new DeletePackets;
	deletePacketsData->packetIds = packetIdsToDelete;

	if (packetIdsToDelete == NULL) {
		DEBUG_STDOUT(printf("WARNING: deletePacketsData->packetIds is NULL !!\n"));
	}

	if (DEBUG) {
		printf("packetIdsToDelete size is: %d", packetIdsToDelete->size());
		printf("size is: %d", deletePacketsData->packetIds->size());
		printf("packet ids are: \n");

		for (int i=0; i<deletePacketsData->packetIds->size(); i++) {
			printf("%" PRIu64 ", ", deletePacketsData->packetIds->at(i));
		}

		printf("\n");
		printf("[Manager::createDeletePacketsData] End\n");
		fflush(stdout);
	}

	return deletePacketsData;
}


void CleanupManagerServer::sendDeletePacketsPacketsRequest(PacketLoggerClient *client, DeletePackets* deletePacketsData) {
	DEBUG_STDOUT(printf("[Manager::sendDeletePacketsPacketsRequest] Start\n"));

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	char* retValAsObj = NULL;

	client->prepareToSend((void*)deletePacketsData, serialized, &len, DELETE_PACKETS_BY_IDS_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, DELETE_PACKETS_BY_IDS_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		DEBUG_STDOUT(cout << "succeed to send" << endl);
	} else {
		DEBUG_STDOUT(cout << "failed to send" << endl);
	}
	DEBUG_STDOUT(printf("[Manager::sendDeletePacketsPacketsRequest] End\n"));
}


// Erase from the master vector all the packets ids that are processed by the slave
vector<uint64_t>* CleanupManagerServer::getUnprocessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
		vector<uint64_t>* slaveProcessedPacketIds, set<uint64_t> *alreadyReplayedPackets) {
	DEBUG_STDOUT(printf("[Manager::getUnprocessedPacketIds] Start\n"));

	vector<uint64_t>::iterator masterIter = masterProcessedPacketIds->begin();
	vector<uint64_t>::iterator slaveIter = slaveProcessedPacketIds->begin();

	printf("[Manager::getUnprocessedPacketIds] master size before is: %d\n", masterProcessedPacketIds->size());
	printf("[Manager::getUnprocessedPacketIds] slave size before is: %d\n", slaveProcessedPacketIds->size());

	int spr = 2;

	while (masterIter != masterProcessedPacketIds->end() &&
			slaveIter != slaveProcessedPacketIds->end()) {

		uint64_t packetBase = (*masterIter) >> 5;

		if (*masterIter == *slaveIter) {
			masterIter = masterProcessedPacketIds->erase(masterIter);
			alreadyReplayedPackets->erase(packetBase);
		} else {
			if (alreadyReplayedPackets->find(packetBase) == alreadyReplayedPackets->end()) {
				if (spr > 0) {
					printf("adding %" PRIu64 " to alreadyReplayedPackets\n", packetBase);
					spr--;
				}
				alreadyReplayedPackets->insert(packetBase);
			} else {
				printf("packet: %" PRIu64 " is omitted from being replayed again!\n", *masterIter);
				masterIter = masterProcessedPacketIds->erase(masterIter);
			}

			masterIter++;
		}

		slaveIter++;
	}

	masterIter = masterProcessedPacketIds->begin();
	while (masterIter != masterProcessedPacketIds->end()) {

		uint64_t packetBase = (*masterIter) >> 5;

		if (DEBUG) {
			if (spr > 0) {
				printf("~adding %" PRIu64 " to alreadyReplayedPackets\n", packetBase);
				spr--;
			}
		}

		alreadyReplayedPackets->insert(packetBase);
		masterIter++;
	}

	if (DEBUG) {
		spr = 2;
		set<uint64_t>::iterator alreadyIter = alreadyReplayedPackets->begin();
		while (alreadyIter != alreadyReplayedPackets->end()) {
			if (spr > 0) {
				printf("~~~alreadyReplayedPackets[%d] = %" PRIu64 "\n", spr, *alreadyIter);
				spr--;
			}

			alreadyIter++;
		}
	}

	printf("returned vector size is: %d\n", masterProcessedPacketIds->size());
	printf("alreadyReplayedPackets size is: %d\n", alreadyReplayedPackets->size());
	DEBUG_STDOUT(printf("returned vector size is: %d\n", masterProcessedPacketIds->size()));
	DEBUG_STDOUT(printf("[Manager::getUnprocessedPacketIds] End\n"));

	return masterProcessedPacketIds;
}


vector<uint64_t>* CleanupManagerServer::getCommonProcessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
		vector<uint64_t>* slaveProcessedPacketIds, int maxPacketsBasesToDelete, set<uint64_t> *alreadyReplayedPackets,
		vector<uint64_t>* orphanSlavePackets) {
	DEBUG_STDOUT(printf("[Manager::getCommonProcessedPacketIds] Start\n"));

	vector<uint64_t>::iterator masterIter = masterProcessedPacketIds->begin();
	vector<uint64_t>::iterator slaveIter = slaveProcessedPacketIds->begin();

	vector<uint64_t>* commonProcessed = new vector<uint64_t>;
	set<int> packetBasesToDelete;
	int actualNumOfPacketsToDelete = 0;

	printf("[Manager::getCommonProcessedPacketIds] master size before is: %d\n", masterProcessedPacketIds->size());
	printf("[Manager::getCommonProcessedPacketIds] slave size before is: %d\n", slaveProcessedPacketIds->size());
	printf("maxPacketsBasesToDelete: %d\n", maxPacketsBasesToDelete);

//	int spr = 2;
//	uint64_t masterTemp = 0;
//	int counter = 0;

	while (masterIter != masterProcessedPacketIds->end() &&
			slaveIter != slaveProcessedPacketIds->end()) {

		if (*masterIter == *slaveIter) {
			if (addPacket(&packetBasesToDelete, commonProcessed, alreadyReplayedPackets, maxPacketsBasesToDelete, *slaveIter)) {
				actualNumOfPacketsToDelete++;
				slaveIter++;
			}
		} else {
			setSlaveOrphanPacketIds(*masterIter, slaveIter, slaveProcessedPacketIds, orphanSlavePackets);

			if (orphanSlavePackets->size() > 0) {
				printf("adding special packet: %" PRIu64 "\n", *masterIter);
				if (addPacket(&packetBasesToDelete, commonProcessed, alreadyReplayedPackets, maxPacketsBasesToDelete, *masterIter)) {
					actualNumOfPacketsToDelete++;
//					slaveIter+= (counter+1);
				}
			}

			break;
		}

		masterIter++;
	}

	printf("orphanSlavePackets size: %d\n", orphanSlavePackets->size());
	printf("%d different packets should be deleted (%d different bases)\n", actualNumOfPacketsToDelete, packetBasesToDelete.size());
	printf("commonProcessed size: %d\n", commonProcessed->size());
	DEBUG_STDOUT(printf("returned vector size is: %d\n", slaveProcessedPacketIds->size()));
	DEBUG_STDOUT(printf("[Manager::getCommonProcessedPacketIds] End\n"));

	return commonProcessed;
}

bool CleanupManagerServer::addPacket(set<int> *packetBasesToDelete, vector<uint64_t>* commonProcessed, set<uint64_t> *alreadyReplayedPackets, int maxPacketsBasesToDelete, uint64_t packetToAdd) {

	uint64_t packetBase = (packetToAdd) >> 5;

	if (packetBasesToDelete->size()<maxPacketsBasesToDelete) {
		packetBasesToDelete->insert(packetBase);
	}

	if (packetBasesToDelete->find(packetBase) != packetBasesToDelete->end()) {
		commonProcessed->push_back(packetToAdd);
		if (alreadyReplayedPackets->find(packetBase) != alreadyReplayedPackets->end()) {
			alreadyReplayedPackets->erase(packetBase);
		}

		return true;
	}

	// This is the first packet with base that exceed maximal number of packet bases to delete
	printf("addPacket returns false!!\n");
	return false;
}

void CleanupManagerServer::setSlaveOrphanPacketIds(uint64_t packetId,
		vector<uint64_t>::iterator slaveIter, vector<uint64_t>* slaveProcessedPacketIds,
		vector<uint64_t>* orphanSlavePackets) {
	DEBUG_STDOUT(printf("[Manager::setSlaveOrphanPacketIds] Start\n"));
	printf("[Manager::setSlaveOrphanPacketIds] Start\n");

	int counter = 0;
	bool isFound = false;

	while (slaveIter != slaveProcessedPacketIds->end()) {

		if (*slaveIter != packetId) {
			orphanSlavePackets->push_back(*slaveIter);
			counter ++;
		} else {
			isFound = true;
			break;
		}

		slaveIter++;
	}

	if (!isFound) {
		printf("can't find packet id (%" PRIu16 ") in slave. clearing orphan vector..\n", packetId);
		orphanSlavePackets->clear();
	}

	printf("[Manager::setSlaveOrphanPacketIds] End\n");
	DEBUG_STDOUT(printf("[Manager::setSlaveOrphanPacketIds] End\n"));
}



MbData* CleanupManagerServer::getSlaveMbData(uint16_t masterMbId) {
	DEBUG_STDOUT(printf("[Manager::getSlaveMbData] Start\n"));

	MbData* slaveData = NULL;

	if (masterSlaveMapping.find(masterMbId) != masterSlaveMapping.end()) {
		DEBUG_STDOUT(printf("1\n"));

		uint16_t slaveMbId = masterSlaveMapping[masterMbId];
		DEBUG_STDOUT(printf("slaveMbId: %" PRIu16 "\n", slaveMbId));

		if (mbData.find(slaveMbId) != mbData.end()) {
			DEBUG_STDOUT(printf("2\n"));
			slaveData = mbData[slaveMbId];
			DEBUG_STDOUT(printf("slaveMbId: %" PRIu16 "\n", slaveData->mbId));
		}
	}

	DEBUG_STDOUT(printf("[Manager::getSlaveMbData] End\n"));
	return slaveData;
}

void CleanupManagerServer::connectToServersForRecovery() {
	printf("starting connecting to servers...\n");

	detLoggerClient->connectToServer();
	slaveDetLoggerClient->connectToServer();
	packetLoggerClient->connectToServer();


	printf("done connecting to servers...\n");
}

void* CleanupManagerServer::prepareGetProcessedPackets(uint16_t mbId, bool allVersions) {
	DEBUG_STDOUT(printf("[Manager::prepareGetProcessedPackets] Start\n"));

	char* input = new char[sizeof(uint16_t)+sizeof(bool)+1];
	uint16_t* mbIdInput = (uint16_t*)input;
	*mbIdInput = mbId;
	mbIdInput++;

	bool* allVersionsInput = (bool*)mbIdInput;
	*allVersionsInput = allVersions;

	return (void*)input;
}

vector<uint64_t>* CleanupManagerServer::getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId, bool allVersions) {
	DEBUG_STDOUT(printf("[Manager::getProcessedPacketIds] Start\n"));
	DEBUG_STDOUT(printf("mbId: %" PRIu16 "\n", *mbId));

	printf("[Manager::getProcessedPacketIds] Start\n");

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	vector<uint64_t> *processedPacketIds = new vector<uint64_t>;

	client->prepareToSend(prepareGetProcessedPackets(*mbId, allVersions), serialized, &len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE, static_cast<void*>(processedPacketIds));

	if (isSucceed) {
		DEBUG_STDOUT(printf("succeed to send\n"));
	} else {
		DEBUG_STDOUT(printf("failed to send\n"));
	}

	if (DEBUG) {
		for (int i=0; i<processedPacketIds->size(); i++) {
			printf("processedPacketIds[%d] = ", i);
			printf("%" PRIu64 "\n", processedPacketIds->at(i));
		}

		printf("[Manager::getProcessedPacketIds] End\n");
	}

	return processedPacketIds;

}

void CleanupManagerServer::deleteFirstPacketsRequest(DetLoggerClient *client, uint16_t mbId, uint32_t totalFirstPacketsToBeDeleted) {
	DEBUG_STDOUT(printf("[Manager::deleteFirstPacketsRequest] Start\n"));
	DEBUG_STDOUT(printf("mbId: %" PRIu16 "\n", mbId));
	DEBUG_STDOUT(printf("totalFirstPacketsToBeDeleted: %" PRIu32 "\n", totalFirstPacketsToBeDeleted));

	char serialized[SERVER_BUFFER_SIZE];
	int len;

	char input[sizeof(uint16_t) + sizeof(uint32_t)];
	uint16_t* mbIdInput = (uint16_t*)input;
	*mbIdInput = mbId;
	mbIdInput++;

	uint32_t* totalPacketsToBeDeletedInput = (uint32_t*)mbIdInput;
	*totalPacketsToBeDeletedInput = totalFirstPacketsToBeDeleted;

	client->prepareToSend((void*)input, serialized, &len, DELETE_PACKETS_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, DELETE_PACKETS_COMMAND_TYPE, NULL);

	if (isSucceed) {
		DEBUG_STDOUT(printf("succeed to send\n"));
	} else {
		DEBUG_STDOUT(printf("failed to send\n"));
	}

	DEBUG_STDOUT(printf("[Manager::deleteFirstPacketsRequest] End\n"));
}

int CleanupManagerServer::runReplayTest(set<uint64_t> *alreadyReplayedPackets, uint16_t masterMbId) {
	printf("start replaying mb %d\n", masterMbId);
	return replay(masterMbId, alreadyReplayedPackets);
}

/*
void Manager::runClearTest(Manager *manager, DetLoggerClient *detLoggerClient, DetLoggerClient *slaveDetLoggerClient, PacketLoggerClient *packetLoggerClient, int maxPacketsToDelete, set<uint64_t> *alreadyReplayedPackets, uint16_t masterMbId) {
	printf("start clearing mb %d\n", masterMbId);
	manager->clearReplayedPackets(masterMbId, maxPacketsToDelete, alreadyReplayedPackets);
}

void Manager::runManagerAutomatically(uint16_t masterMbId, int timeIntervalInMs) {
	cout << "sleepInterval is: " << timeIntervalInMs << " ms" << endl;
	cout << "masterMbId is: " << masterMbId << endl;
	set<uint64_t> *alreadyReplayedPackets = new set<uint64_t>;

	while(command.compare("hi")) {
		usleep(timeIntervalInMs*1000);
		cout << "Invoke replay" << endl;
		int maxPacketsToDelete = runReplayTest(alreadyReplayedPackets, masterMbId);
		cout << "Invoke clear" << endl;
		runClearTest(maxPacketsToDelete, alreadyReplayedPackets, masterMbId);
	}

	alreadyReplayedPackets->clear();
	delete alreadyReplayedPackets;
}

void Manager::runManagerManually(DetLoggerClient *slaveDetLoggerClient, PacketLoggerClient *packetLoggerClient) {

	set<uint64_t> *alreadyReplayedPackets = new set<uint64_t>;

	while (true) {
		cout << "\n--------------------------------" << endl;
		cout << "Enter 'r <masterMbId>' for replay packets, 'c <masterMbId>' for clear common packets or 'e' for exit" << endl;
		string command;
		getline(cin, command);

		if (command[0] == 'r') {
			cout << "command size is: " << command.size() << ", command is: " << command << endl;
			int masterMbId = getMasterMbId(command);
			Manager::runReplayTest(manager, detLoggerClient, slaveDetLoggerClient, packetLoggerClient, alreadyReplayedPackets, masterMbId);
		} else if (command[0] == 'c') {
			int masterMbId = getMasterMbId(command);
			Manager::runClearTest(manager, detLoggerClient, slaveDetLoggerClient, packetLoggerClient, numeric_limits<int>::max(), alreadyReplayedPackets, masterMbId);
		} else if (command[0] == 'e') {
			break;
		}
	}

	alreadyReplayedPackets->clear();
	delete alreadyReplayedPackets;
}*/

uint16_t CleanupManagerServer::getMasterMbId(string command) {
	cout << "command size is: " << command.size() << ", command is: " << command << endl;
	uint16_t masterMbId = 0;
	if (command.size() == 3) {
		masterMbId = atoi(&command[2]);
	}

	cout << "returned masterMbId: " << masterMbId << endl;
	return masterMbId;
}

void setAddress(int argc, char *argv[], char* address) {
	if (argc < 2) {
		printf("Wrong usage: manager.cc [m/p/d]\n");
		exit(1);
	}

	if (argc == 2) {
		if (*argv[1] == 'm') {
			memcpy(address, MANAGER_ADDRESS, 8);
		} else if (*argv[1] == 'd') {
			memcpy(address, DET_LOGGER_SERVER_ADDRESS, 8);
		} else if (*argv[1] == 'p') {
			memcpy(address, PACKET_LOGGER_SERVER_ADDRESS, 8);
		}
	} else if (argc == 3) {
		memcpy(address, "127.0.0.1", 9);
	} else {
		memcpy(address, argv[2], atoi(argv[3]));
	}
}
#endif
#endif

int main(int argc, char *argv[])
{
	cout << "start" << endl;

	CleanupManagerServer *server = new CleanupManagerServer(CLEANUP_MANAGER_PORT);
	server->init();
	server->initCleanupServer();
	server->run();

	delete server;
	return 0;
}

/*
int main(int argc, char *argv[])
{
	char address[MAX_ADDRESS_LEN];
	memset(address, '\0', MAX_ADDRESS_LEN);
	setAddress(argc, argv, address);

	if (*argv[1] == 'm') {

		Manager *manager = new Manager();
		manager->init();

		DetLoggerClient *detLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SERVER_ADDRESS);
		DetLoggerClient *slaveDetLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SLAVE_SERVER_ADDRESS);
		PacketLoggerClient *packetLoggerClient = new PacketLoggerClient(PACKET_LOGGER_SERVER_PORT, PACKET_LOGGER_SERVER_ADDRESS);
		Manager::connectToServersForRecovery(detLoggerClient, slaveDetLoggerClient, packetLoggerClient);

		if (argc == 4) {
			int sleepInterval = atoi(argv[2]);
			uint16_t masterMbId = atoi(argv[3]);
			Manager::runManagerAutomatically(manager, masterMbId, sleepInterval, detLoggerClient, slaveDetLoggerClient, packetLoggerClient);
		} else {
			Manager::runManagerManually(manager, detLoggerClient, slaveDetLoggerClient, packetLoggerClient);
		}

		manager->freeMbData();
		delete manager;

		delete detLoggerClient;
		delete slaveDetLoggerClient;
		delete packetLoggerClient;

	} else if (*argv[1] == 'd') {
		DetLoggerClient::runTests(address);
	} else if (*argv[1] == 'p') {
		PacketLoggerClient::runTests(address);
	}

	return 0;
}
*/
