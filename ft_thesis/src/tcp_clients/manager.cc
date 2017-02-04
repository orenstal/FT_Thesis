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



#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "det_logger_client.hh"
#include "packets_logger_client.hh"
#include "../common/replayPackets/replay_packets.hh"
#include "../common/deletePackets/delete_packets.hh"
#include "client.hh"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <inttypes.h>


#define STORE_COMMAND_TYPE 0
#define GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE 1
#define GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE 2
#define DELETE_PACKETS_COMMAND_TYPE 3

#define DET_LOGGER_SERVER_PORT 9095
#define DET_LOGGER_SERVER_ADDRESS "10.0.0.5" // "127.0.0.1"
#define DET_LOGGER_SLAVE_SERVER_ADDRESS "10.0.0.8"	// "127.0.0.1"
#define PACKET_LOGGER_SERVER_PORT 9097
#define PACKET_LOGGER_SERVER_ADDRESS "10.0.0.4"	//"127.0.0.1"
#define MANAGER_ADDRESS "10.0.0.9"	//"127.0.0.1"
#define MB_PORT 9999

using namespace std;

typedef struct MbData {
    uint16_t mbId;
    char ipAddress[MAX_ADDRESS_LEN];
    uint8_t addressLen;
    int port;

} MbData;

class DetLoggerClient;
class PacketLoggerClient;

class Manager {
private:
	map<uint16_t, MbData* > mbData;
	map<uint16_t, uint16_t > masterSlaveMapping;

	MbData* getSlaveMbData(uint16_t masterMbId);

	static void connectToServersForRecovery(DetLoggerClient *detLoggerClient, DetLoggerClient *slaveDetLoggerClient, PacketLoggerClient *packetLoggerClient);
	static vector<uint64_t>* getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId);

	static vector<uint64_t>* getUnprocessedPacketIds(
	vector<uint64_t>* masterProcessedPacketIds, vector<uint64_t>* slaveProcessedPacketIds);

	static vector<uint64_t>* getCommonProcessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
	vector<uint64_t>* slaveProcessedPacketIds);

	ReplayPackets* createReplayPackets(MbData* slaveData, vector<uint64_t>* unporcessedPacketIds);
	void sendReplayPacketsRequest(PacketLoggerClient *client, ReplayPackets* replayPacketsData);

	DeletePackets* createDeletePacketsData(vector<uint64_t>* packetIdsToDelete);
	void sendDeletePacketsPacketsRequest(PacketLoggerClient *client, DeletePackets* deletePacketsData);

	int connectToPacketLoggerServer(char* mbAddress, int mbPort);

	void deleteFirstPacketsRequest(DetLoggerClient *client, uint16_t mbId, uint32_t totalFirstPacketsToBeDeleted);

public:
	Manager();
	void init();
	void freeMbData();
	bool replay(uint16_t masterMbId);
	bool clearReplayedPackets(uint16_t masterMbId);

	static void runReplayTest(Manager *manager);
	static void runClearTest(Manager *manager);
};

Manager::Manager()
{
}

void Manager::init() {
	masterSlaveMapping.insert(make_pair(1,2));
	MbData* mb1Data = new MbData;
	mb1Data->mbId = 1;
	mb1Data->addressLen = 8;
	memset(mb1Data->ipAddress, '\0', MAX_ADDRESS_LEN);
	memcpy(mb1Data->ipAddress, "10.0.0.6", mb1Data->addressLen);
	mb1Data->port = MB_PORT;
	mbData.insert(make_pair(1, mb1Data));

	MbData* mb2Data = new MbData;
	mb2Data->mbId = 2;
	mb2Data->addressLen = 8;
	memset(mb2Data->ipAddress, '\0', MAX_ADDRESS_LEN);
	memcpy(mb2Data->ipAddress, "10.0.0.7", mb2Data->addressLen);
	mb2Data->port = MB_PORT;
	mbData.insert(make_pair(2, mb2Data));

}

void Manager::freeMbData() {
	delete mbData[1];
	delete mbData[2];
}

bool Manager::replay(uint16_t masterMbId) {
	printf("[Manager::replay] Start\n");

	MbData* slaveData = getSlaveMbData(masterMbId);

	if (slaveData == NULL) {
		printf("ERROR: failed to get slave mb data\n");
		return false;
	}

	printf("slaveData id is: %" PRIu16 ", address is: %s, port is: %d\n", slaveData->mbId, slaveData->ipAddress, slaveData->port);

	DetLoggerClient *detLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SERVER_ADDRESS);
	DetLoggerClient *slaveDetLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SLAVE_SERVER_ADDRESS);
	PacketLoggerClient *packetLoggerClient = new PacketLoggerClient(PACKET_LOGGER_SERVER_PORT, PACKET_LOGGER_SERVER_ADDRESS);
	connectToServersForRecovery(detLoggerClient, slaveDetLoggerClient, packetLoggerClient);

	vector<uint64_t>* masterProcessedPacketIds = Manager::getProcessedPacketIds(detLoggerClient, &masterMbId);
	uint32_t numOfFirstPacketsToBeReplayed = masterProcessedPacketIds->size();

	uint16_t slaveMbId = slaveData->mbId;
	vector<uint64_t>* slaveProcessedPacketIds = Manager::getProcessedPacketIds(slaveDetLoggerClient, &slaveMbId);
	vector<uint64_t>* unporcessedPacketIds = getUnprocessedPacketIds(masterProcessedPacketIds, slaveProcessedPacketIds);

	ReplayPackets* replayPackets = createReplayPackets(slaveData, unporcessedPacketIds);
	sendReplayPacketsRequest(packetLoggerClient, replayPackets);

	DEBUG_STDOUT(printf("about to delete replayPackets\n"););
	delete replayPackets;
	delete masterProcessedPacketIds;
	delete slaveProcessedPacketIds;
	delete detLoggerClient;
	delete slaveDetLoggerClient;
	delete packetLoggerClient;

	printf("[Manager::replay] End\n");

	return true;
}


bool Manager::clearReplayedPackets(uint16_t masterMbId) {
	printf("[Manager::clearReplayedPackets] Start\n");

	MbData* slaveData = getSlaveMbData(masterMbId);

	if (slaveData == NULL) {
		printf("ERROR: failed to get slave mb data\n");
		return false;
	}

	printf("slaveData id is: %" PRIu16 ", address is: %s, port is: %d\n", slaveData->mbId, slaveData->ipAddress, slaveData->port);

	DetLoggerClient *detLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SERVER_ADDRESS);
	DetLoggerClient *slaveDetLoggerClient = new DetLoggerClient(DET_LOGGER_SERVER_PORT, DET_LOGGER_SLAVE_SERVER_ADDRESS);
	PacketLoggerClient *packetLoggerClient = new PacketLoggerClient(PACKET_LOGGER_SERVER_PORT, PACKET_LOGGER_SERVER_ADDRESS);
	connectToServersForRecovery(detLoggerClient, slaveDetLoggerClient, packetLoggerClient);

	vector<uint64_t>* masterProcessedPacketIds = Manager::getProcessedPacketIds(detLoggerClient, &masterMbId);

	uint16_t slaveMbId = slaveData->mbId;
	vector<uint64_t>* slaveProcessedPacketIds = Manager::getProcessedPacketIds(slaveDetLoggerClient, &slaveMbId);
	vector<uint64_t>* commonPorcessedPacketIds = getCommonProcessedPacketIds(masterProcessedPacketIds, slaveProcessedPacketIds);

	deleteFirstPacketsRequest(detLoggerClient, masterMbId, commonPorcessedPacketIds->size());
	deleteFirstPacketsRequest(slaveDetLoggerClient, slaveMbId, commonPorcessedPacketIds->size());

	DeletePackets* deletePacketsData = createDeletePacketsData(commonPorcessedPacketIds);
	sendDeletePacketsPacketsRequest(packetLoggerClient, deletePacketsData);

	printf("[Manager::clearReplayedPackets] End\n");

	delete deletePacketsData;
	delete masterProcessedPacketIds;
	delete slaveProcessedPacketIds;
	delete detLoggerClient;
	delete slaveDetLoggerClient;
	delete packetLoggerClient;

	return true;
}



int Manager::connectToPacketLoggerServer(char* serverAddress, int serverPort) {
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


void Manager::sendReplayPacketsRequest(PacketLoggerClient *client, ReplayPackets* replayPacketsData) {
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


ReplayPackets* Manager::createReplayPackets(MbData* slaveData, vector<uint64_t>* unporcessedPacketIds) {
	DEBUG_STDOUT(printf("[Manager::createReplayPackets] Start\n"));

	ReplayPackets* replayPackets = new ReplayPackets;
	replayPackets->mbId = slaveData->mbId;
	replayPackets->port = slaveData->port;
	replayPackets->addressLen = slaveData->addressLen;
	memset(replayPackets->address, '\0', MAX_ADDRESS_LEN);
	memcpy(replayPackets->address, slaveData->ipAddress, replayPackets->addressLen);

	replayPackets->packetIds = unporcessedPacketIds;

#ifdef DEBUG
	printf("replayPackets->mbId: %d, port: %d, addressLen: %d\n", replayPackets->mbId, replayPackets->port, replayPackets->addressLen);
	for (int i=0; i< MAX_ADDRESS_LEN; i++) {
		printf("%c", replayPackets[i]);
	}
	printf("\n");
	fflush(stdout);
#endif

	if (unporcessedPacketIds == NULL) {
		DEBUG_STDOUT(printf("WARNING: replayPackets->packetIds is NULL !!\n"));
	}

#ifdef DEBUG
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
#endif

	return replayPackets;
}

DeletePackets* Manager::createDeletePacketsData(vector<uint64_t>* packetIdsToDelete) {
	DEBUG_STDOUT(printf("[Manager::createDeletePacketsData] Start\n"));

	DeletePackets* deletePacketsData = new DeletePackets;
	deletePacketsData->packetIds = packetIdsToDelete;

	if (packetIdsToDelete == NULL) {
		DEBUG_STDOUT(printf("WARNING: deletePacketsData->packetIds is NULL !!\n"));
	}

#ifdef DEBUG
	printf("packetIdsToDelete size is: %d", packetIdsToDelete->size());
	printf("size is: %d", deletePacketsData->packetIds->size());
	printf("packet ids are: \n");

	for (int i=0; i<deletePacketsData->packetIds->size(); i++) {
		printf("%" PRIu64 ", ", deletePacketsData->packetIds->at(i));
	}

	printf("\n");
	printf("[Manager::createDeletePacketsData] End\n");
	fflush(stdout);
#endif

	return deletePacketsData;
}


void Manager::sendDeletePacketsPacketsRequest(PacketLoggerClient *client, DeletePackets* deletePacketsData) {
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
vector<uint64_t>* Manager::getUnprocessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
		vector<uint64_t>* slaveProcessedPacketIds) {
	DEBUG_STDOUT(printf("[Manager::getUnprocessedPacketIds] Start\n"));

	vector<uint64_t>::iterator masterIter = masterProcessedPacketIds->begin();
	vector<uint64_t>::iterator slaveIter = slaveProcessedPacketIds->begin();

	while (masterIter != masterProcessedPacketIds->end() &&
			slaveIter != slaveProcessedPacketIds->end()) {

		if (*masterIter == *slaveIter) {
			masterIter = masterProcessedPacketIds->erase(masterIter);
		} else {
			masterIter++;
		}

		slaveIter++;
	}

	DEBUG_STDOUT(printf("returned vector size is: %d\n", masterProcessedPacketIds->size()));
	DEBUG_STDOUT(printf("[Manager::getUnprocessedPacketIds] End\n"));

	return masterProcessedPacketIds;
}

vector<uint64_t>* Manager::getCommonProcessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
		vector<uint64_t>* slaveProcessedPacketIds) {
	DEBUG_STDOUT(printf("[Manager::getCommonProcessedPacketIds] Start\n"));

	vector<uint64_t>::iterator masterIter = masterProcessedPacketIds->begin();
	vector<uint64_t>::iterator slaveIter = slaveProcessedPacketIds->begin();

	while (masterIter != masterProcessedPacketIds->end() &&
			slaveIter != slaveProcessedPacketIds->end()) {

		if (*masterIter != *slaveIter) {
			masterIter = masterProcessedPacketIds->erase(masterIter);
		} else {
			masterIter++;
		}

		slaveIter++;
	}

	DEBUG_STDOUT(printf("returned vector size is: %d\n", masterProcessedPacketIds->size()));
	DEBUG_STDOUT(printf("[Manager::getCommonProcessedPacketIds] End\n"));

	return masterProcessedPacketIds;
}

MbData* Manager::getSlaveMbData(uint16_t masterMbId) {
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

void Manager::connectToServersForRecovery(DetLoggerClient *detLoggerClient, DetLoggerClient *slaveDetLoggerClient, PacketLoggerClient *packetLoggerClient) {
	printf("starting connecting to servers...\n");

	detLoggerClient->connectToServer();
	slaveDetLoggerClient->connectToServer();
	packetLoggerClient->connectToServer();


	printf("done connecting to servers...\n");
}

vector<uint64_t>* Manager::getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId) {
	DEBUG_STDOUT(printf("[Manager::getProcessedPacketIds] Start\n"));
	DEBUG_STDOUT(printf("mbId: %" PRIu16 "\n", *mbId));

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	vector<uint64_t> *processedPacketIds = new vector<uint64_t>;

	client->prepareToSend((void*)mbId, serialized, &len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE, static_cast<void*>(processedPacketIds));

	if (isSucceed) {
		DEBUG_STDOUT(printf("succeed to send\n"));
	} else {
		DEBUG_STDOUT(printf("failed to send\n"));
	}

#ifdef DEBUG
	for (int i=0; i<processedPacketIds->size(); i++) {
		printf("processedPacketIds[%d] = ", i);
		printf("%" PRIu64 "\n", processedPacketIds->at(i));
	}

	printf("[Manager::getProcessedPacketIds] End\n");
#endif

	return processedPacketIds;

}

void Manager::deleteFirstPacketsRequest(DetLoggerClient *client, uint16_t mbId, uint32_t totalFirstPacketsToBeDeleted) {
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

void Manager::runReplayTest(Manager *manager) {
	printf("start replaying mb 1\n");
	manager->replay(1);
}

void Manager::runClearTest(Manager *manager) {
	printf("start replaying mb 1\n");
	manager->clearReplayedPackets(1);
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
	char address[MAX_ADDRESS_LEN];
	memset(address, '\0', MAX_ADDRESS_LEN);
	setAddress(argc, argv, address);

	if (*argv[1] == 'm') {

		Manager *manager = new Manager();
		manager->init();

		while (true) {
			cout << "\n--------------------------------" << endl;
			cout << "Enter 'r' for replay packets, 'c' for clear common packets or 'e' for exit" << endl;
			string command;
			getline(cin, command);

			if (command[0] == 'r') {
				Manager::runReplayTest(manager);
			} else if (command[0] == 'c') {
				Manager::runClearTest(manager);
			} else if (command[0] == 'e') {
				break;
			}
		}

		manager->freeMbData();
		delete manager;

	} else if (*argv[1] == 'd') {
		DetLoggerClient::runTests(address);
	} else if (*argv[1] == 'p') {
		PacketLoggerClient::runTests(address);
	}

	return 0;
}
