/*
 * packet_logger_server.cpp
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */

#include "server.hh"
#include "../common/wrappedPacketData/wrapped_packet_data.hh"
#include "../common/replayPackets/replay_packets.hh"
#include <map>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>


#define MAX_ADDRESS_LEN 16
#define PORT 9097	// port to listening on
#define STORE_COMMAND_TYPE 0
#define GET_PACKET_BY_PACKID_COMMAND_TYPE 1
#define REPLAY_PACKETS_BY_IDS_COMMAND_TYPE 2


using namespace std;

typedef struct SinglePacketVersionData {
    uint8_t version;
	uint16_t offset;
    uint16_t size;
    char* data;
} SinglePacketData;


// comparator for sorting the packet versions according to version field (from 1 to 30)
bool spvdComparator(SinglePacketVersionData* a, SinglePacketVersionData* b) {
	return (a->version < b->version);
}

typedef struct PacketVersionsData {
    vector<SinglePacketVersionData*> packet_versions;
    uint8_t index;
} PacketVersionsData;

typedef map<uint64_t, PacketVersionsData* >::iterator PVDIterType;

class PacketLoggerServer : public Server {

private:
	// the key is packet id base (the received packet id from the client without the 5 right-most digits
	// which represent the packet version).
	map<uint64_t, PacketVersionsData* > packetsVersions;

	PacketVersionsData* getOrCreateServerProgressData(uint64_t packetId);
	void addPacketVersion(PacketVersionsData* packetVersions, WrappedPacketData* wpd);
	void printState();

	void* deserializeClientStoreRequest(int command, char* msg, int msgLen);
	void* deserializeClientReplayRequest(int command, char* msg, int msgLen);
	bool processStoreRequest(void* obj, char* retVal, int* retValLen);
	bool processGetPacketByIdRequest(void* obj, char* retVal, int* retValLen);
	bool processReplayRequest(void* obj, char* retVal, int* retValLen);

	bool getPacket(uint64_t packId, char* retVal, int* retValLen);
	int connectTodestMb(char* mbAddress, int mbPort);
	bool sendMsgToDstMb(int destMbSockfd, char* msgToSend, int length);

protected:
	void* deserializeClientRequest(int command, char* msg, int msgLen);
	bool processRequest(void*, int command, char* retVal, int* retValLen);
	void freeDeserializedObject(void* obj, int command);

public:
	PacketLoggerServer(int port) : Server(port) {
		// do nothing
	}
 };

void* PacketLoggerServer::deserializeClientStoreRequest(int command, char* msg, int msgLen) {
	cout << "PacketLoggerServer::deserializeClientStoreRequest" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	uint64_t *q = (uint64_t*)msg;
	wpd->packetId = *q;
	q++;

	uint16_t *p = (uint16_t*)q;
	wpd->offset = *p;
	p++;

	wpd->size = *p;
	p++;

	char* data = new char[wpd->size];
	char *r = (char*)p;

	for (int i=0; i< wpd->size; i++, r++) {
		data[i] = *r;
	}

	wpd->data = data;
	return (void*)wpd;
}


void* PacketLoggerServer::deserializeClientReplayRequest(int command, char* msg, int msgLen) {
	cout << "PacketLoggerServer::deserializeClientReplayRequest" << endl;

	ReplayPackets* replayData = new ReplayPackets;
	uint16_t *q = (uint16_t*)msg;
	replayData->mbId = *q;
	q++;

	replayData->port = *q;
	q++;

	uint8_t *l = (uint8_t*)q;
	replayData->addressLen = *l;
	l++;

	char* p = (char*)l;
	memset(replayData->address, '\0', MAX_ADDRESS_LEN);
	memcpy(replayData->address, p, replayData->addressLen);
	p+= MAX_ADDRESS_LEN;

	replayData->packetIds = new vector<uint64_t>;

	int vectorLen = msgLen - (2*sizeof(uint16_t) + MAX_ADDRESS_LEN*sizeof(char));
	int totalNumOfPacketIdsToReplay = vectorLen / sizeof(uint64_t);

	cout << "vectorLen: " << vectorLen << ", totalNumOfPacketIdsToReplay: " << totalNumOfPacketIdsToReplay << endl;

	uint64_t *v = (uint64_t*)p;

	for (int i=0; i<totalNumOfPacketIdsToReplay; i++) {
		cout << "received v[" << i << "] is: " << v[i] << "; ";
		replayData->packetIds->push_back(v[i]);
	}

	cout << "replayData: mbId: " << replayData->mbId << ", port: " << replayData->port << ", address: " << replayData->address << ", vector: ";
	for (int i=0; i< replayData->packetIds->size(); i++) {
		cout << "[" << i << "] = " << (*replayData->packetIds)[i] << endl;
	}

	return (void*)replayData;
}


void* PacketLoggerServer::deserializeClientRequest(int command, char* msg, int msgLen) {
	cout << "PacketLoggerServer::deserializeClientStoreRequest" << endl;

	if (command == STORE_COMMAND_TYPE) {
		return deserializeClientStoreRequest(command, msg, msgLen);
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		return (void*)msg;
	} else if (command == REPLAY_PACKETS_BY_IDS_COMMAND_TYPE){
		return deserializeClientReplayRequest(command, msg, msgLen);
	}

	return NULL;
}

bool PacketLoggerServer::processStoreRequest(void* obj, char* retVal, int* retValLen) {
	cout << "ProgressLoggerServer::processStoreRequest" << endl;
	WrappedPacketData* wpd = (WrappedPacketData*)obj;

	PacketVersionsData* packetVersions = getOrCreateServerProgressData(wpd->packetId);
	addPacketVersion(packetVersions, wpd);
	printState();

	// ack/nack will be sent anyway.
	*retValLen = 0;

	return true;
}

bool PacketLoggerServer::processGetPacketByIdRequest(void* obj, char* retVal, int* retValLen) {
	cout << "PacketLoggerServer::processGetPacketByIdRequest" << endl;
	uint64_t packId = 0;

	// extract packId from client request
	uint64_t *p = (uint64_t*)obj;
	packId = *p;

	cout << "packId: " << packId << endl;
	getPacket(packId, retVal, retValLen);

	cout << "packet is: " << retVal << endl;

	return true;
}


bool PacketLoggerServer::processReplayRequest(void* obj, char* retVal, int* retValLen) {
	cout << "ProgressLoggerServer::processReplayRequest" << endl;
	ReplayPackets* replayData = (ReplayPackets*)obj;
	vector<uint64_t>* packetsToReplay = replayData->packetIds;

	cout << "packetsToReplay.size: " << packetsToReplay->size() << endl;

	char address[replayData->addressLen+1];
	memset(address, '\0', replayData->addressLen+1);
	memcpy(address, replayData->address, replayData->addressLen);

//	int destMbSockfd = connectTodestMb(address, replayData->port);
//
//	if (destMbSockfd == -1) {
//		cout << "ERROR: failed to connect dest mb" << endl;
//	}
	cout << "address : " << address << endl;

	char packet[SERVER_BUFFER_SIZE];
	int packetLen;

	for (int i=0; i<packetsToReplay->size(); i++) {

		int destMbSockfd = connectTodestMb(address, replayData->port);
		cout << "destMbSockfd: " << destMbSockfd << endl;

		if (destMbSockfd == -1) {
			cout << "ERROR: failed to connect dest mb" << endl;
			return false;
		}

		memset(packet, '\0', SERVER_BUFFER_SIZE);
		packetLen = 0;
		cout << "1" << endl;

		uint64_t packId = (*packetsToReplay)[i];
		cout << "sending packet id: " << packId << endl;

		if (getPacket(packId, packet, &packetLen)) {
			if (packetLen > 0) {
				cout << "sending to " << address << ", port: " << replayData->port << " packet of len: " << packetLen << ", content: " << packet << endl;
				sendMsgToDstMb(destMbSockfd, packet, packetLen);
			}
		}

		close(destMbSockfd);
	}


	cout << "[ProgressLoggerServer::processReplayRequest] done" << endl;

	// ack/nack will be sent anyway.
	*retValLen = 0;

	return true;
}


bool PacketLoggerServer::processRequest(void* obj, int command, char* retVal, int* retValLen) {
	cout << "command is: " << command << endl;

	if (command == STORE_COMMAND_TYPE) {
		return processStoreRequest(obj, retVal, retValLen);
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		return processGetPacketByIdRequest(obj, retVal, retValLen);
	} else if (command == REPLAY_PACKETS_BY_IDS_COMMAND_TYPE) {
		return processReplayRequest(obj, retVal, retValLen);
	}

	return false;
}

bool PacketLoggerServer::getPacket(uint64_t packId, char* retVal, int* retValLen) {
	cout << "PacketLoggerServer::getPacket" << endl;
	cout << "retVal location: " << &(*retVal) << endl;
	PacketVersionsData* packetVersions = NULL;
	*retValLen = 0;

	uint64_t packetIdBase = packId >> 5;
	uint8_t packetVersion = packId & 31;

	cout << "packet id base is: " << packetIdBase << ", packet version is: " << unsigned(packetVersion) << endl;

	if (packetsVersions.find(packetIdBase) != packetsVersions.end()) {
		cout << "packIdBase " << packetIdBase << " exist in packetsVersions" << endl;
		packetVersions = packetsVersions[packetIdBase];
	}

	if (packetVersions == NULL) {
		cout << "WARNING: received packet id doesn't exist!!" << endl;
		return false;
	}

	uint8_t packetVersionsLen = packetVersions->index;
	cout << "packetVersionsLen: " << unsigned(packetVersionsLen) << endl;

	if (packetVersion < packetVersionsLen) {
		packetVersionsLen = packetVersion;
	}

	cout << "~~ packetVersionsLen: " << unsigned(packetVersionsLen) << endl;


	vector<SinglePacketVersionData*> pvd = packetVersions->packet_versions;

	// sorting the vector according to the version number
	// todo basically this step is not necessary because the version order is kept.
//	sort(pvd.begin(), pvd.end(), spvdComparator);

	for (uint8_t i=0; i<packetVersionsLen; i++) {
		cout << "in" << endl;
		SinglePacketVersionData* spvd = pvd[i];
		cout << "i: " << unsigned(i) << ", version: " << unsigned(spvd->version) << ". About to copy data of size: " << spvd->size << ", to offset: " << retVal+(int)spvd->offset <<  endl;
		memcpy(retVal+(int)spvd->offset, spvd->data, (int)spvd->size);
		*retValLen = max(*retValLen, (int)spvd->size);
	}

	return true;
}


void PacketLoggerServer::addPacketVersion(PacketVersionsData* packetVersions, WrappedPacketData* wpd) {

	SinglePacketVersionData* singlePacketData = new SinglePacketVersionData();

	singlePacketData->version = (wpd->packetId & 31);	// extract packet version
	singlePacketData->offset = wpd->offset;
	singlePacketData->size = wpd->size;

	// copy data content
	uint16_t size = wpd->size;
	char* data = new char[size];	// todo I don't free this memory
	memset(data, 0, size);

	for (int i=0; i< size; i++) {
		data[i] = wpd->data[i];
	}

	singlePacketData->data = data;

	// add to packet versions vector
	packetVersions->packet_versions.push_back(singlePacketData);
	packetVersions->index++;
}

PacketVersionsData* PacketLoggerServer::getOrCreateServerProgressData(uint64_t packetId) {
	cout << "PacketLoggerServer::getOrCreateServerProgressData" << endl;
	cout << "packet id: " << packetId << endl;
	uint64_t packetIdBase = packetId >> 5;

	PacketVersionsData* pvd;

	cout << "packetsVersions.count(" << packetIdBase << "): " << packetsVersions.count(packetIdBase) << endl;
	if (!packetsVersions.count(packetIdBase)) {
		pvd = new PacketVersionsData();
		packetsVersions[packetIdBase] = pvd;
	} else {
		pvd = packetsVersions[packetIdBase];
	}

	return pvd;
}

void PacketLoggerServer::printState() {
	for (PVDIterType iter = packetsVersions.begin(); iter != packetsVersions.end(); iter++) {
		cout << "packet id base: " << iter->first << ":" << endl;
		PacketVersionsData* pvd = iter->second;

		int numOfPacketVersions = pvd->index;

		cout << "number of packet versions: " << numOfPacketVersions << "\n";

		for (int i=0; i<numOfPacketVersions; i++) {
			SinglePacketVersionData* spvd = pvd->packet_versions[i];
			cout << "version: " << int(spvd->version) << ", offset: " << spvd->offset << ", size: " << spvd->size << ", data: ";

			// print data content
			char dataToPrint[spvd->size+1];
			for (int j=0; j< spvd->size; j++) {
				dataToPrint[j] = spvd->data[j];
			}
			dataToPrint[spvd->size] = '\0';

			cout << string(dataToPrint) << endl;
		}

		cout << "-------------------------------------" << endl;
	}
}



void PacketLoggerServer::freeDeserializedObject(void* obj, int command) {

	if (command == STORE_COMMAND_TYPE) {
		WrappedPacketData* wpd = (WrappedPacketData*)obj;
		delete wpd->data;
		delete wpd;
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		// do nothing
	} else if (command == REPLAY_PACKETS_BY_IDS_COMMAND_TYPE){
		ReplayPackets* replayPacket = (ReplayPackets*)obj;
		delete replayPacket;
	}
}

int PacketLoggerServer::connectTodestMb(char* mbAddress, int mbPort) {
	printf("PacketLoggerServer::connectTodestMb\n");
	printf("mbAddress is: %s, mbPort: %d\n", mbAddress, mbPort);
	fflush(stdout);

	struct sockaddr_in sock_addr_dst_mb;
	sock_addr_dst_mb.sin_family = AF_INET;
	sock_addr_dst_mb.sin_addr.s_addr = inet_addr(mbAddress);
	sock_addr_dst_mb.sin_port = htons(mbPort);
	memset(&(sock_addr_dst_mb.sin_zero), '\0', 8);

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
	if (connect(sockfd , (struct sockaddr *)&sock_addr_dst_mb , sizeof(sock_addr_dst_mb)) < 0) {
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

bool PacketLoggerServer::sendMsgToDstMb(int destMbSockfd, char* msgToSend, int length) {
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
		int ret = send(destMbSockfd, msgToSend, length - totalSentBytes, 0);

		if (ret == 0) {
			printf("ERROR: The server is terminated. exit..\n");
//			cout << "ERROR: The server is terminated. exit..";
			return false;
		}

		if (ret < 0) {
			// trying to send one more time after failing the first time
			ret = send(destMbSockfd, msgToSend, length - totalSentBytes, 0);

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



int main(int argc, char *argv[])
{
	cout << "start" << endl;

	PacketLoggerServer *server = new PacketLoggerServer(PORT);
	server->init();
	server->run();
	return 0;
}





