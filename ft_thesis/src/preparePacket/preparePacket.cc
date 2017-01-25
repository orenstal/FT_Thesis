/*
 * preparePacket.cc
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>


#include <iostream>

//#include "pals_manager.hh"
#include "preparePacket.hh"
#include "../common/pal_api/pals_manager.hh"

using namespace std;
CLICK_DECLS

enum { H_MB_STATE_CALL, H_RECOVERY_MODE_CALL, H_TO_MASTER_CALL, H_ENFORCE_TO_MASTER_CALL };
enum { MASTER, RECOVERING, SLAVE, ERROR };

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

	PALSManager* pm = new PALSManager();

	//test
	pm->createGPalAndAdd(1, "test\0");

	SET_PALS_MANAGER_REFERENCE_ANNO(p, (uintptr_t)pm);
	cout << "set pals_manager was completed." << endl;

	gpal* newGPalsList = pm->getGPalList();
	int test = newGPalsList[0].var_id;
	cout << "gpal var id: " << test << "." << endl;
	char* text = newGPalsList[0].val;
	cout << "gpal val is: " << text << endl;


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
EXPORT_ELEMENT(PreparePacket PreparePacket-PreparePacket)
