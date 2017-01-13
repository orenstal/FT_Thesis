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

PreparePacket::PreparePacket()
{
}

PreparePacket::~PreparePacket()
{
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


CLICK_ENDDECLS
EXPORT_ELEMENT(PreparePacket PreparePacket-PreparePacket)
