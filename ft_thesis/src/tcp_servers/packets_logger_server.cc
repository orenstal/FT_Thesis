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
    unsigned char* data;
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

	bool getPacket(uint64_t packId, unsigned char* retVal, int* retValLen);
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
	DEBUG_STDOUT(cout << "PacketLoggerServer::deserializeClientStoreRequest" << endl);

	WrappedPacketData* wpd = new WrappedPacketData;
	uint64_t *q = (uint64_t*)msg;
	wpd->packetId = *q;
	q++;

	uint16_t *p = (uint16_t*)q;
	wpd->offset = *p;
	p++;

	wpd->size = *p;
	p++;

	unsigned char* packetData = (unsigned char*) p;
	unsigned char* data = new unsigned char[wpd->size];
	memcpy(data, packetData, wpd->size);

	wpd->data = data;
	return (void*)wpd;
}


void* PacketLoggerServer::deserializeClientReplayRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "PacketLoggerServer::deserializeClientReplayRequest" << endl);

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

	DEBUG_STDOUT(cout << "vectorLen: " << vectorLen << ", totalNumOfPacketIdsToReplay: " << totalNumOfPacketIdsToReplay << endl);

	uint64_t *v = (uint64_t*)p;

	for (int i=0; i<totalNumOfPacketIdsToReplay; i++) {
		replayData->packetIds->push_back(v[i]);
	}

#ifdef DEBUG
	cout << "replayData: mbId: " << replayData->mbId << ", port: " << replayData->port << ", address: " << replayData->address << ", vector: " << endl;

	for (int i=0; i< replayData->packetIds->size(); i++) {
		cout << "[" << i << "] = " << (*replayData->packetIds)[i] << endl;
	}
#endif

	return (void*)replayData;
}


void* PacketLoggerServer::deserializeClientRequest(int command, char* msg, int msgLen) {
	DEBUG_STDOUT(cout << "PacketLoggerServer::deserializeClientStoreRequest" << endl);

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
	DEBUG_STDOUT(cout << "ProgressLoggerServer::processStoreRequest" << endl);
	WrappedPacketData* wpd = (WrappedPacketData*)obj;

	PacketVersionsData* packetVersions = getOrCreateServerProgressData(wpd->packetId);
	addPacketVersion(packetVersions, wpd);
#ifdef DEBUG
	printState();
#endif

	// ack/nack will be sent anyway.
	*retValLen = 0;

	return true;
}

bool PacketLoggerServer::processGetPacketByIdRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "PacketLoggerServer::processGetPacketByIdRequest" << endl);
	uint64_t packId = 0;

	// extract packId from client request
	uint64_t *p = (uint64_t*)obj;
	packId = *p;

	DEBUG_STDOUT(cout << "packId: " << packId << endl);
	getPacket(packId, (unsigned char*)retVal, retValLen);

	DEBUG_STDOUT(cout << "packet is: " << retVal << endl);
	return true;
}


bool PacketLoggerServer::processReplayRequest(void* obj, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "ProgressLoggerServer::processReplayRequest" << endl);
	ReplayPackets* replayData = (ReplayPackets*)obj;
	vector<uint64_t>* packetsToReplay = replayData->packetIds;

	DEBUG_STDOUT(cout << "packetsToReplay.size: " << packetsToReplay->size() << endl);

	char address[replayData->addressLen+1];
	memset(address, '\0', replayData->addressLen+1);
	memcpy(address, replayData->address, replayData->addressLen);

	DEBUG_STDOUT(cout << "address : " << address << endl);

	unsigned char packet[SERVER_BUFFER_SIZE];
	int packetLen;

	for (int i=0; i<packetsToReplay->size(); i++) {

		int destMbSockfd = connectTodestMb(address, replayData->port);
		DEBUG_STDOUT(cout << "destMbSockfd: " << destMbSockfd << endl);

		if (destMbSockfd == -1) {
			cout << "ERROR: failed to connect dest mb" << endl;
			return false;
		}

		memset(packet, '\0', SERVER_BUFFER_SIZE);
		packetLen = 0;

		uint64_t packId = (*packetsToReplay)[i];
		DEBUG_STDOUT(cout << "sending packet id: " << packId << endl);

		if (getPacket(packId, packet, &packetLen)) {
			if (packetLen > 0) {
				DEBUG_STDOUT(cout << "sending to " << address << ", port: " << replayData->port << " packet of len: " << packetLen << ", content: " << packet << endl);
				sendMsgToDstMb(destMbSockfd, (char*)packet, packetLen);
			}
		}

		close(destMbSockfd);
	}

	DEBUG_STDOUT(cout << "[ProgressLoggerServer::processReplayRequest] done" << endl);

	// ack/nack will be sent anyway.
	*retValLen = 0;

	return true;
}


bool PacketLoggerServer::processRequest(void* obj, int command, char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "command is: " << command << endl);

	if (command == STORE_COMMAND_TYPE) {
		return processStoreRequest(obj, retVal, retValLen);
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		return processGetPacketByIdRequest(obj, retVal, retValLen);
	} else if (command == REPLAY_PACKETS_BY_IDS_COMMAND_TYPE) {
		return processReplayRequest(obj, retVal, retValLen);
	}

	return false;
}

bool PacketLoggerServer::getPacket(uint64_t packId, unsigned char* retVal, int* retValLen) {
	DEBUG_STDOUT(cout << "PacketLoggerServer::getPacket" << endl);
	DEBUG_STDOUT(cout << "retVal location: " << &(*retVal) << endl);

	PacketVersionsData* packetVersions = NULL;
	*retValLen = 0;

	uint64_t packetIdBase = packId >> 5;
	uint8_t packetVersion = packId & 31;

	DEBUG_STDOUT(cout << "packet id base is: " << packetIdBase << ", packet version is: " << unsigned(packetVersion) << endl);

	if (packetsVersions.find(packetIdBase) != packetsVersions.end()) {
		DEBUG_STDOUT(cout << "packIdBase " << packetIdBase << " exist in packetsVersions" << endl);
		packetVersions = packetsVersions[packetIdBase];
	}

	if (packetVersions == NULL) {
		DEBUG_STDOUT(cout << "WARNING: received packet id doesn't exist!!" << endl);
		return false;
	}

	uint8_t packetVersionsLen = packetVersions->index;
	DEBUG_STDOUT(cout << "packetVersionsLen: " << unsigned(packetVersionsLen) << endl);

	if (packetVersion < packetVersionsLen) {
		packetVersionsLen = packetVersion;
	}

	DEBUG_STDOUT(cout << "~~ packetVersionsLen: " << unsigned(packetVersionsLen) << endl);

	vector<SinglePacketVersionData*> pvd = packetVersions->packet_versions;

	// sorting the vector according to the version number
	// todo basically this step is not necessary because the version order is kept.
	sort(pvd.begin(), pvd.end(), spvdComparator);

	// todo for debug usage
//	SinglePacketVersionData* spvd = pvd[0];
//	memcpy(retVal+(int)spvd->offset, spvd->data, (int)spvd->size);
//	*retValLen = max(*retValLen, (int)spvd->size);
	// end debug


	for (uint8_t i=0; i<packetVersionsLen; i++) {
		SinglePacketVersionData* spvd = pvd[i];
		DEBUG_STDOUT(cout << "i: " << unsigned(i) << ", version: " << unsigned(spvd->version) << ". About to copy data of size: " << spvd->size << ", to offset: " << retVal+(int)spvd->offset <<  endl);
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
	unsigned char* data = new unsigned char[size];	// todo I don't free this memory
	memcpy(data, wpd->data, size);

	singlePacketData->data = data;

	// add to packet versions vector
	packetVersions->packet_versions.push_back(singlePacketData);
	packetVersions->index++;
}

PacketVersionsData* PacketLoggerServer::getOrCreateServerProgressData(uint64_t packetId) {
	DEBUG_STDOUT(cout << "PacketLoggerServer::getOrCreateServerProgressData" << endl);
	DEBUG_STDOUT(cout << "packet id: " << packetId << endl);

	uint64_t packetIdBase = packetId >> 5;

	PacketVersionsData* pvd;

	DEBUG_STDOUT(cout << "packetsVersions.count(" << packetIdBase << "): " << packetsVersions.count(packetIdBase) << endl);

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
	DEBUG_STDOUT(cout << "PacketLoggerServer::connectTodestMb" << endl);
	DEBUG_STDOUT(cout << "mbAddress is: " << mbAddress << ", mbPort: " << mbPort << endl);

	struct sockaddr_in sock_addr_dst_mb;
	sock_addr_dst_mb.sin_family = AF_INET;
	sock_addr_dst_mb.sin_addr.s_addr = inet_addr(mbAddress);
	sock_addr_dst_mb.sin_port = htons(mbPort);
	memset(&(sock_addr_dst_mb.sin_zero), '\0', 8);

	//Create socket
	int sockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (sockfd == -1) {
		cout << "ERROR: Could not create socket" << endl;
		return sockfd;
	}

	// activate keep-alive mechanism
//	int val = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_dst_mb , sizeof(sock_addr_dst_mb)) < 0) {
		cout << "ERROR: Connect failed." << endl;
		return -1;
	}

	if (sockfd <= 0) {
		return -1;
	}

	DEBUG_STDOUT(cout << "Connected successfully via sockfd " << sockfd << endl);
	return sockfd;
}

bool PacketLoggerServer::sendMsgToDstMb(int destMbSockfd, char* msgToSend, int length) {

#ifdef DEBUG
	cout << "in sendMsg.. length: " << length << ", msg:";

	// print message content
	for(int i=0; i< length; i++) {
		printf("%c", msgToSend[i]);
	}

	printf("\n");
#endif

	int totalSentBytes = 0;

	if (length > SERVER_BUFFER_SIZE) {
		cout << "ERROR: can't send message that is longer than " << SERVER_BUFFER_SIZE << endl;
		return false;
	}


	// Write the message to the server
	while (totalSentBytes < length) {
		DEBUG_STDOUT(cout << "sending.." << endl);
		int ret = send(destMbSockfd, msgToSend, length - totalSentBytes, 0);

		if (ret == 0) {
			cout << "ERROR: The server is terminated. exit.." << endl;
			return false;
		}

		if (ret < 0) {
			// trying to send one more time after failing the first time
			ret = send(destMbSockfd, msgToSend, length - totalSentBytes, 0);

			if (ret == 0) {
				cout << "ERROR: The server is terminated. exit.." << endl;
				return false;
			}

			if (ret < 0) {
				return false;
			}
		}

		totalSentBytes += ret;
		msgToSend += ret;
	}

	DEBUG_STDOUT(cout << "totalSentBytes: " << totalSentBytes << endl);

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





