/*
 * packetIdEncap.cc
 *
 *  Created on: Nov 26, 2016
 *      Author: Tal
 */

/*
 * ethervlanencap.{cc,hh} -- encapsulates packet in Ethernet header
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "packetIdEncap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>


#include <iostream>
using namespace std;
CLICK_DECLS

PacketIdEncap::PacketIdEncap()
{
	_seqNum = 128;
}

PacketIdEncap::~PacketIdEncap()
{
}

int
PacketIdEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
	int producerId = 0;
	if (Args(conf, this, errh)
	.read_p("PRODUCER_ID", BoundedIntArg(1, 6), producerId)
	.complete() < 0)
		return -1;

	_producerId = producerId;
    return 0;
}

bool PacketIdEncap::isValidSeqNum(uint32_t candidate) {
	// we want to ignore the seven right-most bits (because they part of the inner vlan and
	// therefore valid (no chance to get 0 or 4095 thanks for the version bits).
	candidate = candidate >> 7;

	// then we want to check the 12 bits that constitute the second vlan (4095 = 111111111111).
	candidate &= 4095;

	// finally we check that the middle vlan isn't 0 or 4095 (preserved for 802.1Q).
	if (candidate == 4095 || candidate == 0) {
		return false;
	}

	return true;
}

// sequence number constitutes of 2^28 -1 bits (except 3 bits for producer id and 5 bits for version).
// Overall we have 36 bits (12 for each vlan level).
uint32_t PacketIdEncap::getNexSeqNum() {
	bool isValid = false;

	while (!isValid) {
		if (_seqNum > MAX_SEQ_NUM) {
			_seqNum = 128;	// the first valid sequence valid
//			cout << "start again... MAX_SEQ_NUM: " << MAX_SEQ_NUM << endl;
		}

		if (isValidSeqNum(_seqNum)) {
			isValid = true;
		} else {
			_seqNum += 128;
		}
	}

	return _seqNum++;
}


uint16_t PacketIdEncap::createInnerVlan(uint32_t seqNumber) {
	uint16_t temp;

	// extract the 9 left-most bits among the 28 bits (although there are 32 bits, only the first 28 are in use)
	temp = seqNumber >> 19;

	// concatenate the 9 bits from temp to the 3 digits from _producerId.
	return (_producerId << 9) | temp;
}


uint16_t PacketIdEncap::createMiddleVlan(uint32_t seqNumber) {
	uint16_t temp;

	// remove the 7 right-most bits (they are part of the inner vlan)
	temp = seqNumber >> 7;

	// extract the 12 digits of the middle vlan. 524160 = (00000000000000000000111111111111).
	return (temp & 4095);
}


uint16_t PacketIdEncap::createOuterVlan(uint32_t seqNumber) {
	uint16_t temp;

	// extract the 7 right-most bits
	temp = seqNumber & 127;

	// concatenate 5 zero's for initial version (valid values are 1-30)
	temp = (temp << 5);

	return (temp | 1);
}


uint64_t PacketIdEncap::createId() {

	// nextSeqNum constitute of at most 28 bits.
	uint32_t nextSeqNum = getNexSeqNum();

	uint64_t innerVlan = createInnerVlan(nextSeqNum);
	uint64_t middleVlan = createMiddleVlan(nextSeqNum);
	uint64_t outerVlan = createOuterVlan(nextSeqNum);

	uint64_t unified = (innerVlan << 24) | (middleVlan << 12) | outerVlan;

	cout << "unified id: " << unified << ", seqNum: " << nextSeqNum << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl;

	return unified;
}

Packet *
PacketIdEncap::pushVlanLayer(Packet *p, uint16_t vlan_tci)
{
	cout << "[PacketIdEncap] Start pushing vlan layer with tci: " << vlan_tci << endl;

	if (p == 0) {
		cout << "[PacketIdEncap] Error: invalid packet" << endl;
		return p;
	}

	assert(!p->mac_header() || p->mac_header() == p->data());
	uint16_t tci = htons(vlan_tci);

	if ((tci & htons(0xFFF)) == 0) {
		p->set_mac_header(p->data(), sizeof(click_ether));
		cout << "[PacketIdEncap] Problem while pusing vlan tci: " << vlan_tci << endl;
		return p;
	} else if (WritablePacket *q = p->push(4)) {
		memmove(q->data(), q->data() + 4, 12);
		click_ether_vlan *vlan = reinterpret_cast<click_ether_vlan *>(q->data()+4);
		vlan->ether_vlan_proto = htons(ETHERTYPE_8021Q);
		vlan->ether_vlan_tci = tci;
		q->set_mac_header(q->data(), sizeof(vlan));
		cout << "[PacketIdEncap] push vlan tci succeed." << endl;
		return q;
	} else {
		cout << "[PacketIdEncap] Error: failed to push vlan tci: " << vlan_tci << endl;
		return 0;
	}
}


Packet *
PacketIdEncap::smaction(Packet *p)	// main logic - should be changed
{

	if (!p->has_transport_header())
		return p;

	// nextSeqNum constitute of at most 28 bits.
	uint32_t nextSeqNum = getNexSeqNum();

	uint64_t innerVlan = createInnerVlan(nextSeqNum);
	uint64_t middleVlan = createMiddleVlan(nextSeqNum);
	uint64_t outerVlan = createOuterVlan(nextSeqNum);

	uint64_t unified = (innerVlan << 24) | (middleVlan << 12) | outerVlan;

	cout << "unified id: " << unified << ", seqNum: " << nextSeqNum << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl;

	Packet *q = pushVlanLayer(p, innerVlan);
	q = pushVlanLayer(q, middleVlan);
	q = pushVlanLayer(q, outerVlan);

	SET_PACKID_ANNO(q, unified);
	cout << "set packet id anno: " << PACKID_ANNO(q) << endl;
	return q;

//	if (_use_anno)
//	_ethh.ether_vlan_tci = VLAN_TCI_ANNO(p);
//    if ((_ethh.ether_vlan_tci & htons(0x0FFF)) == _native_vlan) {
//	if (WritablePacket *q = p->push_mac_header(sizeof(click_ether))) {
//	    memcpy(q->data(), &_ethh, 12);
//	    q->ether_header()->ether_type = _ethh.ether_vlan_encap_proto;
//	    return q;
//	} else
//	    return 0;
//    }
//    if (WritablePacket *q = p->push_mac_header(sizeof(click_ether_vlan))) {
//	memcpy(q->data(), &_ethh, sizeof(click_ether_vlan));
//	return q;
//    } else
//	return 0;
}


void
PacketIdEncap::push(int, Packet *p)
{
    if (Packet *q = smaction(p))
	output(0).push(q);
}

Packet *
PacketIdEncap::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}


void
PacketIdEncap::add_handlers()
{
	add_data_handlers("producer_id", Handler::h_read | Handler::h_write, &_producerId);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketIdEncap PacketIdEncap-PacketIdEncap)


