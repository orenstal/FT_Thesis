/*
 * packet_logger_server.cpp
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */

#include "server.hh"
#include "../common/wrappedPacketData/wrapped_packet_data.hh"
#include <map>
#include <vector>
#include <algorithm>

#define PORT 9097	// port to listening on
#define STORE_COMMAND_TYPE 0
#define GET_PACKET_BY_PACKID_COMMAND_TYPE 1


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
	bool processStoreRequest(void* obj, char* retVal, int* retValLen);
	bool processGetPacketByIdRequest(void* obj, char* retVal, int* retValLen);

	bool getPacket(uint64_t packId, char* retVal, int* retValLen);

protected:
	void* deserializeClientRequest(int command, char* msg, int msgLen);
	bool processRequest(void*, int command, char* retVal, int* retValLen);
	void freeDeserializedObject(void* obj);

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

void* PacketLoggerServer::deserializeClientRequest(int command, char* msg, int msgLen) {
	cout << "PacketLoggerServer::deserializeClientStoreRequest" << endl;

	if (command == STORE_COMMAND_TYPE) {
		return deserializeClientStoreRequest(command, msg, msgLen);
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		return (void*)msg;
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

bool PacketLoggerServer::processRequest(void* obj, int command, char* retVal, int* retValLen) {
	cout << "command is: " << command << endl;

	if (command == STORE_COMMAND_TYPE) {
		return processStoreRequest(obj, retVal, retValLen);
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		return processGetPacketByIdRequest(obj, retVal, retValLen);
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
	sort(pvd.begin(), pvd.end(), spvdComparator);

	for (uint8_t i=0; i<packetVersionsLen; i++) {
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



void PacketLoggerServer::freeDeserializedObject(void* obj) {
	WrappedPacketData* wpd = (WrappedPacketData*)obj;
	delete wpd->data;
	delete wpd;
}



int main(int argc, char *argv[])
{
	cout << "start" << endl;

	PacketLoggerServer *server = new PacketLoggerServer(PORT);
	server->init();
	server->run();
	return 0;
}





