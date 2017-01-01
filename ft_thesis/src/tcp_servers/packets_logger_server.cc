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

#define PORT 9097	// port to listening on

using namespace std;

typedef struct SinglePacketVersionData {
    uint8_t version;
	uint16_t offset;
    uint16_t size;
    char* data;
} SinglePacketData;


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

protected:
	void* deserializeClientRequest(char* msg, int msgLen);
	bool processRequest(void*);
	void freeDeserializedObject(void* obj);

public:
	PacketLoggerServer(int port) : Server(port) {
		// do nothing
	}
 };

void* PacketLoggerServer::deserializeClientRequest(char* msg, int msgLen) {
	cout << "PacketLoggerServer::deserializeClientRequest" << endl;

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

bool PacketLoggerServer::processRequest(void* obj) {
	cout << "ProgressLoggerServer::processRequest" << endl;
	WrappedPacketData* wpd = (WrappedPacketData*)obj;

	PacketVersionsData* packetVersions = getOrCreateServerProgressData(wpd->packetId);
	addPacketVersion(packetVersions, wpd);
	printState();

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





