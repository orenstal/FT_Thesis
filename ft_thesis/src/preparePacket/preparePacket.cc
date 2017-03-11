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
	DEBUG_STDOUT(cout << "[DetLoggerPPClient::serializePalsManagerObject] Start" << endl);
	PALSManager* pm = (PALSManager*)obj;

	if (DEBUG) {
		cout << "start serializing det_logger client" << endl;
		cout << "serialized: " << serialized << ", len: " << *len << ", mbId: " << pm->getMBId() << endl;
		cout << ", packid: " << pm->getPacketId() << endl;
		cout << "pm->getGPalSize()" << pm->getGPalSize() << endl;
		cout << "pm->getSPalSize()" << pm->getSPalSize() << endl;
	}

	PALSManager::serialize(pm, serialized, len);
	DEBUG_STDOUT(cout << "[DetLoggerPPClient::serializePalsManagerObject] End" << endl);
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
	DEBUG_STDOUT(cout << "DetLoggerPPClient::serializeObject" << endl);

	if (command == GET_PALS_BY_MBID_AND_PACKID_COMMAND_TYPE) {
		serializeGetPalsByMBIdAndPackId(command, obj, serialized, len);
	}
}

void DetLoggerPPClient::handleGetPalsByMBIdAndPackIdResponse(int command, char* retVal, int retValLen, void* retValAsObj) {
	DEBUG_STDOUT(cout << "DetLoggerPPClient::handleGetPalsByMBIdAndPackIdResponse" << endl);

	PALSManager* pm = static_cast<PALSManager*>(retValAsObj);

	if (pm == NULL) {
		cout << "ERROR: pm can't be null!!" << endl;
		return;
	}

	PALSManager::deserialize(retVal, pm);

	if (DEBUG) {
		pm->printContent();
	}

	*((PALSManager*)retValAsObj) = *pm;

	DEBUG_STDOUT(cout << "[DetLoggerPPClient::handleGetPalsByMBIdAndPackIdResponse] done" << endl);
}

void DetLoggerPPClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	DEBUG_STDOUT(cout << "DetLoggerPPClient::handleReturnValue" << endl);

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
	int vlanLevelOffset = 1;

	if (Args(conf, this, errh)
	.read_p("MASTER", isMasterMode)
	.read_p("VLAN_LEVEL_OFFSET", BoundedIntArg(0, 3), vlanLevelOffset)
	.complete() < 0)
		return -1;


	if (isMasterMode) {
		_mbState = MASTER;
	} else {
		_mbState = SLAVE;
	}
	cout << "isMaster mode? " << isMasterMode << endl;

	_vlanLevelOffset = vlanLevelOffset;

	cout << "vlan level offset is: " << _vlanLevelOffset << endl;

	if (isSlave()) {
		cout << "[slave mode] connecting to det logger" << endl;
		detLoggerClient = new DetLoggerPPClient(9095, "10.0.0.5");
		detLoggerClient->connectToServer();
	}

	cout << "Done configuration.." << endl;

    return 0;
}

uint64_t
PreparePacket::extractVlanByLevel(Packet *p, uint16_t level)
{
	DEBUG_STDOUT(cout << "[PreparePacket] Extracting vlan level: " << level << endl);

	if (p == 0) {
		cout << "[PreparePacket] Error: invalid packet" << endl;
		return 0;
	}

    assert(!p->mac_header() || p->mac_header() == p->data());
    if (level > 3) {
    	cout << "[PreparePacket] Error: wrong level!" << endl;
    	return 0;
    }

    uint64_t tci = 0;
    const click_ether_vlan *vlan = reinterpret_cast<const click_ether_vlan *>(p->data()+4*level);
    if (vlan->ether_vlan_proto == htons(ETHERTYPE_8021Q)) {
		tci = vlan->ether_vlan_tci;
		DEBUG_STDOUT(cout << "[PreparePacket] extractVlanByLevel succeed. Returned tci: " << ntohs(tci) << endl);
		return ntohs(tci);

	} else {
		cout << "[PreparePacket] Error: invalid ethertype" << ntohs(vlan->ether_vlan_proto) << " expected: " << htons(ETHERTYPE_8021Q) << endl;
	    return 0;
    }
}

void* PreparePacket::prepareGetPalsByMBIdAndPackId(uint16_t mbId, uint64_t packId) {
	DEBUG_STDOUT(cout << "preparing get pals request for mbId: " << mbId << ", packId: " << packId << endl);

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
		DEBUG_STDOUT(cout << "succeed to send" << endl);
	} else {
		DEBUG_STDOUT(cout << "failed to send" << endl);
	}

	delete (char*)inputs;
	return (void*)pm;
}


Packet *
PreparePacket::smaction(Packet *p)	// main logic
{
	uint16_t innerLevel = _vlanLevelOffset + 2;
	uint64_t innerVlan = extractVlanByLevel(p, innerLevel);
	uint64_t middleVlan = extractVlanByLevel(p, innerLevel-1);
	uint64_t outerVlan = extractVlanByLevel(p, innerLevel-2);

	uint64_t unifiedId;
	if (innerVlan > 0 && middleVlan > 0 && outerVlan > 0) {
		unifiedId = (innerVlan << 24) | (middleVlan << 12) | outerVlan;
	} else {
		unifiedId = 0;
	}

	DEBUG_STDOUT(cout << "unified id: " << unifiedId << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl);

	SET_PACKID_ANNO(p, unifiedId);
	DEBUG_STDOUT(cout << "set packet id was completed. getAnno is: " << PACKID_ANNO(p) << endl);


	PALSManager* pm;

	if (isMaster()) {
		DEBUG_STDOUT(cout << "[master mode] create dummy pal" << endl);
		pm = new PALSManager();

		//test
		pm->createGPalAndAdd(1, "test\0");

		if (DEBUG) {
			gpal* newGPalsList = pm->getGPalList();
			int test = newGPalsList[0].var_id;
			cout << "gpal var id: " << test << "." << endl;
			char* text = newGPalsList[0].val;

			cout << "gpal val is: " << text << endl;
			cout << "done master mode" << endl;
		}

	} else if (isSlave()) {
		DEBUG_STDOUT(cout << "[slave mode] start getting pals for mbId: " << MASTER_MB_ID << ", packId: " << unifiedId << endl);
		pm = (PALSManager*)getPals(unifiedId);

		if (DEBUG) {
			cout << "now pm is:" << endl;
			pm->printContent();
			cout << "done slave mode" << endl;
		}
	}

	SET_PALS_MANAGER_REFERENCE_ANNO(p, (uintptr_t)pm);
	DEBUG_STDOUT(cout << "set pals_manager was completed." << endl);

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
//	add_data_handlers("mb_id", Handler::h_read | Handler::h_write, &_mbId);
//	add_data_handlers("vlanLevelOffset", Handler::h_read | Handler::h_write, &_vlanLevelOffset);

	add_read_handler("state", read_handler, H_MB_STATE_CALL);
	add_write_handler("toMaster", write_handler, H_TO_MASTER_CALL);
	add_write_handler("enforceToMaster", write_handler, H_ENFORCE_TO_MASTER_CALL);
	add_write_handler("recover", write_handler, H_RECOVERY_MODE_CALL);
}



CLICK_ENDDECLS
ELEMENT_REQUIRES(TCPClient)
EXPORT_ELEMENT(PreparePacket PreparePacket-PreparePacket)
