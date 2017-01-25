/*
 * packet_logger_client.cpp
 *
 *  Created on: Jan 1, 2017
 *      Author: Tal
 */

#include "packets_logger_client.hh"

void PacketLoggerClient::serializeWrappedPacketDataObject(int command, void* obj, char* serialized, int* len) {
	cout << "PacketLoggerClient::serializeWrappedPacketDataObject" << endl;
	WrappedPacketData* wpd = (WrappedPacketData*)obj;
	uint16_t size = wpd->size;

	uint64_t *q = (uint64_t*)serialized;
	*q = wpd->packetId;
	q++;

	uint16_t *p = (uint16_t*)q;
	*p = wpd->offset;
	p++;

	*p = size;
	p++;

	char *r = (char*)p;

	for (int i=0; i< size; i++, r++) {
		*r = wpd->data[i];
	}

	*len = sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t) + (sizeof(char) * size);

	cout << "len is: " << *len << endl;
}

void PacketLoggerClient::serializeGetPacketById(int command, void* obj, char* serialized, int* len) {
	uint16_t* mbIdInput = (uint16_t*)obj;
	uint16_t *q = (uint16_t*)serialized;
	*q = *mbIdInput;
	q++;
	mbIdInput++;

	uint64_t *packIdInput = (uint64_t*)mbIdInput;
	uint64_t *p = (uint64_t*)q;
	*p = *packIdInput;
	*len = sizeof(uint16_t) + sizeof(uint64_t);
}


void PacketLoggerClient::serializeReplayPacketsByIds(int command, void* obj, char* serialized, int* len) {
	ReplayPackets* replayData = (ReplayPackets*) obj;

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
		cout << "[" << i << "] = " << (*vec)[i] << endl;
		*vecInput = (*vec)[i];
		vecInput++;
	}

	*len = 2*sizeof(uint16_t) + sizeof(uint8_t) + MAX_ADDRESS_LEN + vec->size()*sizeof(uint64_t);
}



void PacketLoggerClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	cout << "PacketLoggerClient::serializeObject" << endl;

	if (command == STORE_COMMAND_TYPE) {
		serializeWrappedPacketDataObject(command, obj, serialized, len);
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		serializeGetPacketById(command, obj, serialized, len);
	} else if (command == REPLAY_PACKETS_BY_IDS_COMMAND_TYPE) {
		serializeReplayPacketsByIds(command, obj, serialized, len);
	}
}

void PacketLoggerClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	cout << "PacketLoggerClient::handleReturnValue" << endl;

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		cout << "nothing to handle." << endl;
	} else if (command == GET_PACKET_BY_PACKID_COMMAND_TYPE) {
		cout << "received packet: " << retVal << endl;

		char** returnedPacket = static_cast<char**>(retValAsObj);
		*returnedPacket = new char[len];
		memcpy(*returnedPacket, retVal, len);
		cout << "returnedPacket: " << *returnedPacket << endl;
	} else if (command == REPLAY_PACKETS_BY_IDS_COMMAND_TYPE) {
		cout << "received ack for packet replaying." << endl;
	}
}

void PacketLoggerClient::runTests(char* address) {
	cout << "starting progress logger client" << endl;
	cout << "address is: " << address << endl;

	PacketLoggerClient *client = new PacketLoggerClient(9097, address);	// "127.0.0.1"
	client->connectToServer();

	cout << "start running test 1..." << endl;
	WrappedPacketData* wpd = preparePacketLoggerClientTest1();
	runPacketLoggerClientTest(client, wpd);
	delete wpd;

	cout << "\nstart running test 2..." << endl;
	wpd = preparePacketLoggerClientTest2();
	runPacketLoggerClientTest(client, wpd);
	delete wpd;

	cout << "\nstart running test 3..." << endl;
	wpd = preparePacketLoggerClientTest3();
	runPacketLoggerClientTest(client, wpd);
	delete wpd;

	cout << "\nstart running test 4..." << endl;
	wpd = preparePacketLoggerClientTest4();
	runPacketLoggerClientTest(client, wpd);
	delete wpd;

	cout << "\nstart getting packet id 193..." << endl;
	void* inputs = prepareGetPacketTest(193);
	getPacket(client, inputs);
	delete (char*)inputs;

	cout << "\nstart getting packet id 195..." << endl;
	inputs = prepareGetPacketTest(195);
	getPacket(client, inputs);
	delete (char*)inputs;

	cout << "\nstart getting packet id 225..." << endl;
	inputs = prepareGetPacketTest(225);
	getPacket(client, inputs);
	delete (char*)inputs;

	cout << "\nstart replaying packets 195 and 225..." << endl;
	inputs = prepareReplayPacketsTest1();
	replayPackets(client, inputs);
	delete (ReplayPackets*)inputs;

	cout << "\nstart replaying packet 225..." << endl;
	inputs = prepareReplayPacketsTest2();
	replayPackets(client, inputs);
	delete (ReplayPackets*)inputs;

	cout << "\nstart replaying packets 193, 195 and 225..." << endl;
	inputs = prepareReplayPacketsTest3();
	replayPackets(client, inputs);
	delete (ReplayPackets*)inputs;

	cout << "\nstart replaying empty packet list..." << endl;
	inputs = prepareReplayPacketsTest4();
	replayPackets(client, inputs);
	delete (ReplayPackets*)inputs;
}


WrappedPacketData* PacketLoggerClient::preparePacketLoggerClientTest1() {
	cout << "preparing test 1" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 193L;
	wpd->offset = 0;
	wpd->size = 15;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'a';
	}

	wpd->data = data;

	return wpd;
}


WrappedPacketData* PacketLoggerClient::preparePacketLoggerClientTest2() {
	cout << "preparing test 2" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 194L;
	wpd->offset = 10;
	wpd->size = 15;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'b';
	}

	wpd->data = data;

	return wpd;
}

WrappedPacketData* PacketLoggerClient::preparePacketLoggerClientTest3() {
	cout << "preparing test 3" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 195L;
	wpd->offset = 13;
	wpd->size = 10;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'c';
	}

	wpd->data = data;

	return wpd;
}

WrappedPacketData* PacketLoggerClient::preparePacketLoggerClientTest4() {
	cout << "preparing test 4" << endl;

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = 225L;
	wpd->offset = 0;
	wpd->size = 10;

	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++) {
		data[i] = 'd';
	}

	wpd->data = data;

	return wpd;
}

void PacketLoggerClient::runPacketLoggerClientTest(PacketLoggerClient *client, WrappedPacketData* wpd) {
	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)wpd, serialized, &len, STORE_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE, NULL);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}

void* PacketLoggerClient::prepareGetPacketTest(uint64_t packId) {
	cout << "preparing get packet test for packId: " << packId << endl;

	char* input = new char[sizeof(uint64_t)+1];
	uint64_t* packIdInput = (uint64_t*)input;
	*packIdInput = packId;

	return (void*)input;
}

void PacketLoggerClient::getPacket(PacketLoggerClient *client, void* msgToSend) {

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	char* retValAsObj = NULL;

	client->prepareToSend(msgToSend, serialized, &len, GET_PACKET_BY_PACKID_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, GET_PACKET_BY_PACKID_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

	cout << "retValAsObj is: " << retValAsObj << endl;
}

void* PacketLoggerClient::prepareReplayPacketsTest1() {
	cout << "preparing replay packets test 1" << endl;

	ReplayPackets* input = new ReplayPackets;
	input->mbId = 1;
	input->port = DEBUG_LISTEN_TO_PORT;
	input->addressLen = DEBUG_LISTEN_TO_ADDRESS_LEN;
	memset(input->address, '\0', MAX_ADDRESS_LEN);
	memcpy(input->address, DEBUG_LISTEN_TO_ADDRESS, DEBUG_LISTEN_TO_ADDRESS_LEN);
	input->packetIds = new vector<uint64_t>;

	input->packetIds->push_back(195);
	input->packetIds->push_back(225);

	return (void*)input;
}

void* PacketLoggerClient::prepareReplayPacketsTest2() {
	cout << "preparing replay packets test 2" << endl;

	ReplayPackets* input = new ReplayPackets;
	input->mbId = 1;
	input->port = DEBUG_LISTEN_TO_PORT;
	input->addressLen = DEBUG_LISTEN_TO_ADDRESS_LEN;
	memset(input->address, '\0', MAX_ADDRESS_LEN);
	memcpy(input->address, DEBUG_LISTEN_TO_ADDRESS, DEBUG_LISTEN_TO_ADDRESS_LEN);
	input->packetIds = new vector<uint64_t>;

	input->packetIds->push_back(225);

	return (void*)input;
}

void* PacketLoggerClient::prepareReplayPacketsTest3() {
	cout << "preparing replay packets test 3" << endl;

	ReplayPackets* input = new ReplayPackets;
	input->mbId = 1;
	input->port = DEBUG_LISTEN_TO_PORT;
	input->addressLen = DEBUG_LISTEN_TO_ADDRESS_LEN;
	memset(input->address, '\0', MAX_ADDRESS_LEN);
	memcpy(input->address, DEBUG_LISTEN_TO_ADDRESS, DEBUG_LISTEN_TO_ADDRESS_LEN);
	input->packetIds = new vector<uint64_t>;

	input->packetIds->push_back(193);
	input->packetIds->push_back(195);
	input->packetIds->push_back(225);

	return (void*)input;
}

void* PacketLoggerClient::prepareReplayPacketsTest4() {
	cout << "preparing replay packets test 4" << endl;

	ReplayPackets* input = new ReplayPackets;
	input->mbId = 1;
	input->port = DEBUG_LISTEN_TO_PORT;
	input->addressLen = DEBUG_LISTEN_TO_ADDRESS_LEN;
	memset(input->address, '\0', MAX_ADDRESS_LEN);
	memcpy(input->address, DEBUG_LISTEN_TO_ADDRESS, DEBUG_LISTEN_TO_ADDRESS_LEN);
	input->packetIds = new vector<uint64_t>;

	return (void*)input;
}

void PacketLoggerClient::replayPackets(PacketLoggerClient *client, void* msgToSend) {

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	char* retValAsObj = NULL;

	client->prepareToSend(msgToSend, serialized, &len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, REPLAY_PACKETS_BY_IDS_COMMAND_TYPE, static_cast<void*>(&retValAsObj));

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}
}

//int main(int argc, char *argv[]) {
//	PacketLoggerClient::runTests("127.0.0.1");
//	return 0;
//}




