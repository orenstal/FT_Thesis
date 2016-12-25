/*
 * increaseVersionNumber.cc
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */

#include <click/config.h>
#include "increaseVersionNumber.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/packet_anno.hh>

#include "pals_manager.hh"
#include <iostream>
using namespace std;
CLICK_DECLS

IncreaseVersionNumber::IncreaseVersionNumber()
{
}

IncreaseVersionNumber::~IncreaseVersionNumber()
{
}



Packet *
IncreaseVersionNumber::smaction(Packet *p)	// main logic
{
	cout << "[IncreaseVersionNumber] Start increasing id version..." << endl;

	if (p == 0) {
		cout << "[IncreaseVersionNumber] Error: invalid packet" << endl;
		return p;
	}

	assert(!p->mac_header() || p->mac_header() == p->data());

	WritablePacket *q = (WritablePacket *)p;

	click_ether_vlan *vlan = reinterpret_cast<click_ether_vlan *>(q->data()+4);
	if (vlan->ether_vlan_proto == htons(ETHERTYPE_8021Q)) {
		uint16_t tci = ntohs(vlan->ether_vlan_tci);
		vlan->ether_vlan_tci = htons(tci+1);
		cout << "[IncreaseVersionNumber] increaseOuterVlan succeed." << endl;
	} else {
		cout << "[IncreaseVersionNumber] Error: invalid ethertype" << ntohs(vlan->ether_vlan_proto) << endl;
	}

	// TODO Do we need to call "SET_PACKID_ANNO(p, unifiedId);" ? For now I think I don't need.

	// TODO temp! this code is temporary (testing pals manager)
	PALSManager* pm =(PALSManager *)PALS_MANAGER_REFERENCE_ANNO(q);
	//pm->createGPalAndAdd(1, "test");
	gpal* gpalList = pm->getGPalList();
	if (gpalList[0].var_id != 1) {
		cout << "[IncreaseVersionNumber] Error: var_id is: " << gpalList[0].var_id << ", instead of 1" << endl;
	} else if (strcmp(gpalList[0].val, "test") != 0) {
		cout << "[IncreaseVersionNumber] Error: val is: " << gpalList[0].val << ", instead of 'test'" << endl;
	} else {
		cout << "[IncreaseVersionNumber] pals manager was extracted successfully." << endl;
	}

	return q;
}


void
IncreaseVersionNumber::push(int, Packet *p)
{
    if (Packet *q = smaction(p))
	output(0).push(q);
}

Packet *
IncreaseVersionNumber::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(IncreaseVersionNumber IncreaseVersionNumber-IncreaseVersionNumber)




