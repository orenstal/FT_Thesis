/*
 * preparePacket.cc
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */


#include <click/config.h>
#include "client.hh"
#include "distributePacketRecords.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
//#include <clicknet/ip.h>
//#include <clicknet/udp.h>
//#include <clicknet/tcp.h>

#include <pthread.h>
#include <netinet/in.h>
#include <iostream>
#include<sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>


#include <iostream>

//#include "pals_manager.hh"
#include "preparePacket.hh"
#include "../common/pal_api/pals_manager.hh"

#define GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE 2
#define MASTER_MB_ID 1

using namespace std;
CLICK_DECLS

enum { H_MB_STATE_CALL, H_RECOVERY_MODE_CALL, H_TO_MASTER_CALL, H_ENFORCE_TO_MASTER_CALL };
enum { MASTER, RECOVERING, SLAVE, ERROR };

class DetLoggerPPClient : public Client {

private:
	void serializePalsManagerObject(int command, void* obj, char* serialized, int* len);
	void serializeGetPalsByMBIdAndPackId(int command, void* obj, char* serialized, int* len);
	void handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen, void* retValAsObj);

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	DetLoggerPPClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
};

void DetLoggerPPClient::serializePalsManagerObject(int command, void* obj, char* serialized, int* len) {
	cout << "[DetLoggerPPClient::serializePalsManagerObject] Start" << endl;
	PALSManager* pm = (PALSManager*)obj;

	cout << "start serializing det_logger client" << endl;
	cout << "serialized: " << serialized << ", len: " << *len << ", mbId: " << pm->getMBId() << endl;
	cout << ", packid: " << pm->getPacketId() << endl;
	cout << "pm->getGPalSize()" << pm->getGPalSize() << endl;
	cout << "pm->getSPalSize()" << pm->getSPalSize() << endl;

	PALSManager::serialize(pm, serialized, len);
	cout << "[DetLoggerPPClient::serializePalsManagerObject] End" << endl;
}

void DetLoggerPPClient::serializeGetPalsByMBIdAndPackId(int command, void* obj, char* serialized, int* len) {
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

void DetLoggerPPClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	cout << "DetLoggerPPClient::serializeObject" << endl;

//	if (command == STORE_COMMAND_TYPE) {
//		serializePalsManagerObject(command, obj, serialized, len);
//	}

	if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		serializeGetPalsByMBIdAndPackId(command, obj, serialized, len);
	}
}

void DetLoggerPPClient::handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen, void* retValAsObj) {
	cout << "DetLoggerPPClient::handleGetPalsByMBIdAndPackIdResponse" << endl;
//	cout << "retValLen: " << retValLen << endl;
	PALSManager* pm = static_cast<PALSManager*>(retValAsObj);
//	cout << "2" << endl;

//	cout << "pm == null? " << (pm == NULL) << endl;

	if (pm == NULL) {
		cout << "ERROR: pm can't be null!!" << endl;
		return;
	}

	PALSManager::deserialize(retVal, pm);
//	cout << "3" << endl;

	pm->printContent();
	*((PALSManager*)retValAsObj) = *pm;
	cout << "[DetLoggerPPClient::handleGetPalsByMBIdAndPackIdResponse] done" << endl;
}

void DetLoggerPPClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	cout << "DetLoggerPPClient::handleReturnValue" << endl;

//	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
//		cout << "nothing to handle." << endl;
//	}

	if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		handleGetPalsByMBIdAndPackIdResponse(command, retVal, len, retValAsObj);
	}
}


PreparePacket::PreparePacket()
{
	_mbState = ERROR;
}

PreparePacket::~PreparePacket()
{
}

int
PreparePacket::configure(Vector<String> &conf, ErrorHandler *errh)
{
	bool isMasterMode = true;

	if (Args(conf, this, errh)
	.read_p("MASTER", isMasterMode)
	.complete() < 0)
		return -1;

//	if (tci_word && !tci_word.equals("ANNO", 4)

	if (isMasterMode) {
		_mbState = MASTER;
	} else {
		_mbState = SLAVE;
	}
	cout << "isMaster mode? " << isMasterMode << endl;

	if (isSlave()) {
		cout << "[slave mode] connecting to det logger" << endl;
		detLoggerClient = new DetLoggerPPClient(9095, "10.0.0.5");
		detLoggerClient->connectToServer();
	}

    return 0;
}

uint64_t
PreparePacket::extractVlanByLevel(Packet *p, uint8_t level)
{
	cout << "[PreparePacket] Extracting vlan level: " << unsigned(level) << endl;

	if (p == 0) {
		cout << "[PreparePacket] Error: invalid packet" << endl;
		return 0;
	}

    assert(!p->mac_header() || p->mac_header() == p->data());
    if (level < 1 || level > 3) {
    	cout << "[PreparePacket] wrong level!" << endl;
    	return 0;
    }

    uint64_t tci = 0;
    const click_ether_vlan *vlan = reinterpret_cast<const click_ether_vlan *>(p->data()+4*level);
    if (vlan->ether_vlan_proto == htons(ETHERTYPE_8021Q)) {
		tci = vlan->ether_vlan_tci;
		cout << "[PreparePacket] extractVlanByLevel succeed. Returned tci: " << ntohs(tci) << endl;
		return ntohs(tci);
	} else {
		cout << "[PreparePacket] Error: invalid ethertype" << ntohs(vlan->ether_vlan_proto) << endl;
	    return 0;
    }
}

void* PreparePacket::prepareGetPalsByMBIdAndPackId(uint16_t mbId, uint64_t packId) {
	cout << "preparing get pals request for mbId: " << mbId << ", packId: " << packId << endl;

	char* input = new char[sizeof(uint16_t)+sizeof(uint64_t)+1];
	uint16_t* mbIdInput = (uint16_t*)input;
	*mbIdInput = mbId;
	mbIdInput++;

	uint64_t* packIdInput = (uint64_t*)mbIdInput;
	*packIdInput = packId;

	return (void*)input;
}

void* PreparePacket::getPals(uint64_t packId) {

	char serialized[SERVER_BUFFER_SIZE];
	int len;
	PALSManager* pm = new PALSManager();
	void* inputs = prepareGetPalsByMBIdAndPackId(MASTER_MB_ID, packId);
	detLoggerClient->prepareToSend(inputs, serialized, &len, GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE);
	bool isSucceed = detLoggerClient->sendMsgAndWait(serialized, len, GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE, static_cast<void*>(pm));

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

//	cout << "1" << endl;
//	pm->printContent();
//	cout << "done" << endl;

	delete (char*)inputs;
	return (void*)pm;
}


Packet *
PreparePacket::smaction(Packet *p)	// main logic
{
	uint64_t innerVlan = extractVlanByLevel(p, 3);
	uint64_t middleVlan = extractVlanByLevel(p, 2);
	uint64_t outerVlan = extractVlanByLevel(p, 1);

	uint64_t unifiedId;
	if (innerVlan > 0 && middleVlan > 0 && outerVlan > 0) {
		unifiedId = (innerVlan << 24) | (middleVlan << 12) | outerVlan;
	} else {
		unifiedId = 0;
	}

	cout << "unified id: " << unifiedId << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl;

	SET_PACKID_ANNO(p, unifiedId);
	cout << "set packet id was completed. getAnno is: " << PACKID_ANNO(p) << endl;


	PALSManager* pm;

	if (isMaster()) {
		cout << "[master mode] create dummy pal" << endl;
		pm = new PALSManager();

		//test
		pm->createGPalAndAdd(1, "test\0");

		gpal* newGPalsList = pm->getGPalList();
		int test = newGPalsList[0].var_id;
		cout << "gpal var id: " << test << "." << endl;
		char* text = newGPalsList[0].val;
		cout << "gpal val is: " << text << endl;
		cout << "done master mode" << endl;
	} else if (isSlave()) {
		cout << "[slave mode] start getting pals for mbId: " << MASTER_MB_ID << ", packId: " << unifiedId << endl;
		pm = (PALSManager*)getPals(unifiedId);
		cout << "now pm is:" << endl;
		pm->printContent();
		cout << "done slave mode" << endl;
	}

	SET_PALS_MANAGER_REFERENCE_ANNO(p, (uintptr_t)pm);
	cout << "set pals_manager was completed." << endl;


	return p;

}


void
PreparePacket::push(int, Packet *p)
{
    if (Packet *q = smaction(p))
	output(0).push(q);
}

Packet *
PreparePacket::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}

void PreparePacket::recover() {
	_mbState = RECOVERING;
	printf("start recovering...\n");
	fflush(stdout);
}

bool PreparePacket::isRecover() {
	return (_mbState == RECOVERING);
}

bool PreparePacket::changeModeToMaster() {
	if (_mbState == RECOVERING) {
		return false;
	}

	doChangeModeToMaster();
	return true;
}

void PreparePacket::doChangeModeToMaster() {
	_mbState = MASTER;
	printf("mode is changed to master\n");
	fflush(stdout);
}


bool PreparePacket::isMaster() {
	return (_mbState == MASTER);
}

bool PreparePacket::isSlave() {
	return (_mbState == SLAVE);
}


String
PreparePacket::read_handler(Element *e, void *thunk)
{
	PreparePacket *pp = (PreparePacket *)e;
	switch ((intptr_t)thunk) {
	case H_MB_STATE_CALL:
		if (pp->isMaster()) {
			return String("master");
		} else if (pp->isRecover()){
			return String("recovering");
		} else {
			return String("slave");
		}
	default:
		return "<error>";
	}
}

int
PreparePacket::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
	PreparePacket *pp = (PreparePacket *)e;
	String str = in_str;
	switch ((intptr_t)thunk) {
	case H_TO_MASTER_CALL:
		if (pp->changeModeToMaster()) {
			return 0;
		} else {
			return errh->error("ERROR: mb is under recovery.");
		}
	case H_ENFORCE_TO_MASTER_CALL:
		pp->doChangeModeToMaster();
		return 0;
	case H_RECOVERY_MODE_CALL:
		pp->recover();
		return 0;
	default:
		return errh->error("<internal>");
	}
}

void
PreparePacket::add_handlers()
{
	add_data_handlers("mb_id", Handler::h_read | Handler::h_write, &_mbId);

	add_read_handler("state", read_handler, H_MB_STATE_CALL);
	add_write_handler("toMaster", write_handler, H_TO_MASTER_CALL);
	add_write_handler("enforceToMaster", write_handler, H_ENFORCE_TO_MASTER_CALL);
	add_write_handler("recover", write_handler, H_RECOVERY_MODE_CALL);
}



CLICK_ENDDECLS
ELEMENT_REQUIRES(TCPClient)
EXPORT_ELEMENT(PreparePacket PreparePacket-PreparePacket)
