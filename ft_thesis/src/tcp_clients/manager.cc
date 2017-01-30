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


/*
 * steps:
 * 	1. get all packet ids stored in det logger (as progress logger).
 * 	2. get all packet ids stored in slave progress logger.
 * 	3. request the packet from wrapped packet logger (by id).
 * 	4. request the pals related to this packet from master det logger.
 */


#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include "det_logger_client.hh"
#include "packets_logger_client.hh"
//#include "../common/pal_api/pals_manager.hh"
#include "../common/replayPackets/replay_packets.hh"
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
	ReplayPackets* createReplayPackets(MbData* slaveData, vector<uint64_t>* unporcessedPacketIds);
	void sendReplayPacketsRequest(PacketLoggerClient *client, ReplayPackets* replayPacketsData);
//	void serializeReplayPacketsObj(ReplayPackets* replayData, char* serialized, int* len);

	int connectToPacketLoggerServer(char* mbAddress, int mbPort);
	bool sendMsgToDstMb(int destMbSockfd, char* msgToSend, int length); // todo to be removed

public:
	Manager();
	void init();
	bool recover(uint16_t masterMbId);

	static void runTests(char* address);
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

bool Manager::recover(uint16_t masterMbId) {
	printf("[Manager::recover] Start\n");
//	cout << "Manager::recover" << endl;

	MbData* slaveData = getSlaveMbData(masterMbId);

	if (slaveData == NULL) {
		printf("ERROR: failed to get slave mb data\n");
//		cout << "ERROR: failed to get slave mb data" << endl;
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
//	vector<uint64_t>* slaveProcessedPacketIds = new vector<uint64_t>;
	vector<uint64_t>* unporcessedPacketIds = getUnprocessedPacketIds(masterProcessedPacketIds, slaveProcessedPacketIds);

	ReplayPackets* replayPackets = createReplayPackets(slaveData, unporcessedPacketIds);
	sendReplayPacketsRequest(packetLoggerClient, replayPackets);
	printf("about to delete replayPackets\n");
	delete replayPackets;

//	char* serialized = new char[5096];
//	int len;
//	char* retValAsObj = NULL;
//	serializeReplayPacketsObj(replayPackets, serialized, &len); //

	/*
	int packetLoggerServerSockfd = connectToPacketLoggerServer(PACKET_LOGGER_SERVER_ADDRESS, PACKET_LOGGER_SERVER_PORT);

	if (packetLoggerServerSockfd == -1) {
		printf("ERROR: failed to connect dest mb\n");
//		cout << "ERROR: failed to connect dest mb" << endl;
	}

	sendMsgToDstMb(packetLoggerServerSockfd, serialized, len);
	close(packetLoggerServerSockfd);
	*/

//	packetLoggerClient->sendMsg(serialized, len);
//	packetLoggerClient->sendMsgAndWait(serialized, len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE, static_cast<void*>(&retValAsObj));
	printf("[Manager::recover] End\n");
}



int Manager::connectToPacketLoggerServer(char* serverAddress, int serverPort) {
	printf("Manager::connectToPacketLoggerServer\n");
	printf("serverAddress is: %s, serverPort: %d\n", serverAddress, serverPort);
	fflush(stdout);

	struct sockaddr_in sock_addr_dst_server;
	sock_addr_dst_server.sin_family = AF_INET;
	sock_addr_dst_server.sin_addr.s_addr = inet_addr(serverAddress);
	sock_addr_dst_server.sin_port = htons(serverPort);
	memset(&(sock_addr_dst_server.sin_zero), '\0', 8);

	//Create socket
	int sockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (sockfd == -1) {
//		cout << "ERROR: Could not create socket" << endl;
		printf("ERROR: Could not create socket\n");
		return sockfd;
	}

	printf("Socket created\n");
//	cout << "Socket created" << endl;

	// activate keep-alive mechanism
//	int val = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_dst_server , sizeof(sock_addr_dst_server)) < 0) {
//		click << "ERROR: Connect failed." << endl;
		printf("ERROR: Connect failed.\n");
		return -1;
	}

	if (sockfd <= 0) {
		return -1;
	}

	printf("Connected successfully via sockfd %d\n", sockfd);
	return sockfd;
}

bool Manager::sendMsgToDstMb(int packetLoggerServerSockfd, char* msgToSend, int length) {
	printf("in sendMsg.. length: %d, msg:", length);

	// print message content
	for(int i=0; i< length; i++) {
		printf("%c", msgToSend[i]);
//		cout << serialized[i];
	}

	printf("\n");
//	cout << endl;


	int totalSentBytes = 0;

	if (length > SERVER_BUFFER_SIZE) {
		printf("ERROR: can't send message that is longer than %d\n", SERVER_BUFFER_SIZE);
		return false;
	}

	// Write the message to the server
	while (totalSentBytes < length) {
		printf("sending..\n");
		int ret = send(packetLoggerServerSockfd, msgToSend, length - totalSentBytes, 0);

		if (ret == 0) {
			printf("ERROR: The server is terminated. exit..\n");
//			cout << "ERROR: The server is terminated. exit..";
			return false;
		}

		if (ret < 0) {
			// trying to send one more time after failing the first time
			ret = send(packetLoggerServerSockfd, msgToSend, length - totalSentBytes, 0);

			if (ret == 0) {
				printf("ERROR: The server is terminated. exit..\n");
				return false;
			}

			if (ret < 0) {
				return false;
			}
		}

		totalSentBytes += ret;
		msgToSend += ret;
	}

	printf("totalSentBytes: %d\n", totalSentBytes);

	return true;

}

void Manager::sendReplayPacketsRequest(PacketLoggerClient *client, ReplayPackets* replayPacketsData) {
	printf("[Manager::sendReplayPacketsRequest] Start\n");

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	char* retValAsObj = NULL;
//	void* msgToSend = (void*)replayPacketsData;

	client->prepareToSend((void*)replayPacketsData, serialized, &len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}
	printf("[Manager::sendReplayPacketsRequest] End\n");
}

/*void Manager::serializeReplayPacketsObj(ReplayPackets* replayData, char* serialized, int* len) {

	uint16_t *q = (uint16_t*)serialized;
	*q = replayData->mbId;
	q++;

	*q = replayData->port;
	q++;

	uint8_t *l = (uint8_t*)q;
	*l = replayData->addressLen;
	l++;

	char* p = (char*)l;
	memcpy(p, replayData->address, replayData->addressLen);
	p+= MAX_ADDRESS_LEN;

	uint64_t* vecInput = (uint64_t*)p;

	vector<uint64_t>* vec = replayData->packetIds;
	for (int i=0; i< vec->size(); i++) {
		printf("[%d] = ", i);
		printf("%" PRIu64 "\n", vec->at(i));
//		printf("[%d] = %d\n", i, (*vec)[i]);
//		cout << "[" << i << "] = " << (*vec)[i] << endl;
		*vecInput = (*vec)[i];
		vecInput++;
	}

	*len = 2*sizeof(uint16_t) + sizeof(uint8_t) + MAX_ADDRESS_LEN + vec->size()*sizeof(uint64_t);
}
*/

ReplayPackets* Manager::createReplayPackets(MbData* slaveData, vector<uint64_t>* unporcessedPacketIds) {
	printf("[Manager::createReplayPackets] Start\n");
	ReplayPackets* replayPackets = new ReplayPackets;
	replayPackets->mbId = slaveData->mbId;
	replayPackets->port = slaveData->port;
	replayPackets->addressLen = slaveData->addressLen;
	memset(replayPackets->address, '\0', MAX_ADDRESS_LEN);
	memcpy(replayPackets->address, slaveData->ipAddress, replayPackets->addressLen);

	replayPackets->packetIds = unporcessedPacketIds;

	printf("replayPackets->mbId: %d, port: %d, addressLen: %d\n", replayPackets->mbId, replayPackets->port, replayPackets->addressLen);
	for (int i=0; i< MAX_ADDRESS_LEN; i++) {
		printf("%c", replayPackets[i]);
	}
	printf("\n");

	printf("1\n");
	fflush(stdout);
	if (unporcessedPacketIds == NULL) {
//	if (replayPackets->packetIds == NULL) {
		printf("WARNING: replayPackets->packetIds is NULL !!\n");
		fflush(stdout);
	}

	printf("unporcessedPacketIds size is: %d", unporcessedPacketIds->size());
	fflush(stdout);
	printf("unporcessedPacketIds packet ids are: \n");
	fflush(stdout);
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

	return replayPackets;
}

// Erase from the master vector all the packets ids that are processed by the slave
vector<uint64_t>* Manager::getUnprocessedPacketIds(vector<uint64_t>* masterProcessedPacketIds,
		vector<uint64_t>* slaveProcessedPacketIds) {
	printf("[Manager::getUnprocessedPacketIds] Start\n");

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

	printf("returned vector size is: %d\n", masterProcessedPacketIds->size());

	printf("[Manager::getUnprocessedPacketIds] End\n");
	return masterProcessedPacketIds;
}

MbData* Manager::getSlaveMbData(uint16_t masterMbId) {
	printf("[Manager::getSlaveMbData] Start\n");
	MbData* slaveData = NULL;

	if (masterSlaveMapping.find(masterMbId) != masterSlaveMapping.end()) {
		printf("1\n");

		uint16_t slaveMbId = masterSlaveMapping[masterMbId];
		printf("slaveMbId: %" PRIu16 "\n", slaveMbId);

		if (mbData.find(slaveMbId) != mbData.end()) {
			printf("2\n");
			slaveData = mbData[slaveMbId];

			printf("slaveMbId: %" PRIu16 "\n", slaveData->mbId);
		}
	}

	printf("[Manager::getSlaveMbData] End\n");
	return slaveData;
}

void Manager::connectToServersForRecovery(DetLoggerClient *detLoggerClient, DetLoggerClient *slaveDetLoggerClient, PacketLoggerClient *packetLoggerClient) {
	printf("starting connecting to servers...\n");
//	cout << "starting connecting to servers..." << endl;

	detLoggerClient->connectToServer();
	slaveDetLoggerClient->connectToServer();
	packetLoggerClient->connectToServer();

	printf("done connecting to servers...\n");
	//	cout << "done connecting to servers..." << endl;
}

vector<uint64_t>* Manager::getProcessedPacketIds(DetLoggerClient *client, uint16_t* mbId) {
	printf("[Manager::getProcessedPacketIds] Start\n");
	printf("mbId: %" PRIu16 "\n", *mbId);
	char serialized[SERVER_BUFFER_SIZE];
	int len;
	vector<uint64_t> *processedPacketIds = new vector<uint64_t>;

	client->prepareToSend((void*)mbId, serialized, &len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PROCESSED_PACKET_IDS_BY_MBID_COMMAND_TYPE, static_cast<void*>(processedPacketIds));

	if (isSucceed) {
		printf("succeed to send\n");
//		cout << "succeed to send" << endl;
	} else {
		printf("failed to send\n");
//		cout << "failed to send" << endl;
	}

	for (int i=0; i<processedPacketIds->size(); i++) {
		printf("processedPacketIds[%d] = ", i);
		printf("%" PRIu64 "\n", processedPacketIds->at(i));
//		printf("processedPacketIds[%d] = %" PRIu64 "\n", i, processedPacketIds->at(i));
//		cout << "processedPacketIds[" << i << "] = " << processedPacketIds[i] << endl;
	}

	printf("[Manager::getProcessedPacketIds] End\n");
	return processedPacketIds;

}

void Manager::runTests(char* address) {
	printf("start recovering mb 1\n");

	Manager *manager = new Manager();
	manager->init();
	manager->recover(1);
	printf("press ctrl+c to exit\n");
	fflush(stdout);
	while(1){}
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

	if (*argv[1] == 'm')
		Manager::runTests(address);
	else if (*argv[1] == 'd')
		DetLoggerClient::runTests(address);
	else if (*argv[1] == 'p')
		PacketLoggerClient::runTests(address);

	return 0;
}
