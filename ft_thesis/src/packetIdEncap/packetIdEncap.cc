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
#include "client.hh"
#include "packetIdEncap.hh"
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>

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

#include "../common/wrappedPacketData/wrapped_packet_data.hh"

#define STORE_COMMAND_TYPE 0

#include <iostream>
using namespace std;
CLICK_DECLS


class PacketLoggerClient : public Client {

private:
	void serializeWrappedPacketDataObject(int command, void* obj, char* serialized, int* len);

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	PacketLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
 };

void PacketLoggerClient::serializeWrappedPacketDataObject(int command, void* obj, char* serialized, int* len) {
	DEBUG_STDOUT(cout << "PacketLoggerClient::serializeWrappedPacketDataObject" << endl);
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

	unsigned char *r = (unsigned char*)p;
	const unsigned char* data = wpd->data;
	memcpy(r, data, size);

	*len = sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t) + (sizeof(unsigned char) * size);

	DEBUG_STDOUT(cout << "len is: " << *len << endl);
}

void PacketLoggerClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	DEBUG_STDOUT(cout << "PacketLoggerClient::serializeObject" << endl);

	if (command == STORE_COMMAND_TYPE) {
		serializeWrappedPacketDataObject(command, obj, serialized, len);
	}
}

void PacketLoggerClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	DEBUG_STDOUT(cout << "PacketLoggerClient::handleReturnValue" << endl);

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		DEBUG_STDOUT(cout << "nothing to handle." << endl);
	}
}






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
	bool isMasterMode = true;
	bool onlyHeader = false;

	if (Args(conf, this, errh)
	.read_p("PRODUCER_ID", BoundedIntArg(1, 6), producerId)
	.read_p("MASTER", isMasterMode)
	.read_p("ONLY_HEADER", onlyHeader)
	.complete() < 0)
		return -1;


	_producerId = producerId;
	_isMasterMode = isMasterMode;
	_onlyHeader = onlyHeader;
	cout << "isMaster mode? " << _isMasterMode << endl;
	cout << "onlyHeader? " << _onlyHeader << endl;

	if (_isMasterMode) {
		client = new PacketLoggerClient(9097, "10.0.0.4");
		client->connectToServer();
	}

	// todo this code works and designed for listening to master/slave changes (not for the regular behavior)
//	pthread_t t1;
//	pthread_create(&t1, NULL, &PacketIdEncap::print_message, NULL);

	cout << "Done configuration.." << endl;



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

void* PacketIdEncap::print_message(void* args) {
	cout << "Hi from thread.." << endl;
	connectToServer(9097, "10.0.0.4");
	cout << "connected ??" << endl;
	while (1) {

	}

	return NULL;
}

int PacketIdEncap::connectToServer(int port, char* address) {
	struct sockaddr_in sock_addr_server;
	sock_addr_server.sin_family = AF_INET;
	sock_addr_server.sin_addr.s_addr = inet_addr(address); // = INADDR_ANY;
	sock_addr_server.sin_port = htons(port);
	memset(&(sock_addr_server.sin_zero), '\0', 8);

	int sockfd = -1;

	//Create socket
	sockfd = socket(AF_INET , SOCK_STREAM , 0);
	if (sockfd == -1) {
		cout << "ERROR: Could not create socket" << endl;
	}

	// activate keep-alive mechanism
//	int val = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_server , sizeof(sock_addr_server)) < 0) {
		cout << "ERROR: Connect failed." << endl;
		return -1;
	}

	cout<<"Connected successfully\n";

	return sockfd;
}


// sequence number constitutes of 2^28 -1 bits (except 3 bits for producer id and 5 bits for version).
// Overall we have 36 bits (12 for each vlan level).
uint32_t PacketIdEncap::getNexSeqNum() {
	bool isValid = false;

	while (!isValid) {
		if (_seqNum > MAX_SEQ_NUM) {
			_seqNum = 128;	// the first valid sequence valid
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

	DEBUG_STDOUT(cout << "unified id: " << unified << ", seqNum: " << nextSeqNum << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl);
	return unified;
}

Packet *
PacketIdEncap::pushVlanLayer(Packet *p, uint16_t vlan_tci)
{
	DEBUG_STDOUT(cout << "[PacketIdEncap] ~Start pushing vlan layer with tci: " << vlan_tci << endl);

	if (p == 0) {
		cout << "[PacketIdEncap] Error: invalid packet" << endl;
		return p;
	}

	assert(!p->mac_header() || p->mac_header() == p->data());
	uint16_t tci = htons(vlan_tci);

	if ((tci & htons(0xFFF)) == 0) {
		p->set_mac_header(p->data(), sizeof(click_ether));
		DEBUG_STDOUT(cout << "[PacketIdEncap] Problem while pusing vlan tci: " << vlan_tci << endl);
		return p;

	} else if (WritablePacket *q = p->push(4)) {
		memmove(q->data(), q->data() + 4, 16);
		click_ether_vlan *vlan = reinterpret_cast<click_ether_vlan *>(q->data()+4);
		vlan->ether_vlan_proto = htons(ETHERTYPE_8021Q);
		vlan->ether_vlan_tci = tci;
		q->set_mac_header(q->data(), sizeof(vlan));
		DEBUG_STDOUT(cout << "[PacketIdEncap] push vlan tci succeed." << endl);
		return q;

	} else {
		cout << "[PacketIdEncap] Error: failed to push vlan tci: " << vlan_tci << endl;
		return 0;
	}
}


int PacketIdEncap::getHeadersLen(Packet *p) {
	DEBUG_STDOUT(cout << "getHeadersLen" << endl);

	int headersLen = 0;
	unsigned transport_header_len = 0;
	const click_ip *iph = p->ip_header();

	if (p->mac_header() == p->data()) {
		DEBUG_STDOUT(cout << "getHeadersLen: network header wasn't set. Using default sizes!!" << endl);
		return sizeof(click_ether) + sizeof(click_ip);
	}

	if (DEBUG) {
		cout << "has transport? " << p->has_transport_header() << endl;

		printf("data: %p, mac: %p, network: %p, transport: %p, end-data: %d", p->data(), p->mac_header(), p->network_header(), p->transport_header(), (p->end_data() - p->data()));
		printf(", network_header_offset: %d", p->network_header_offset());
		cout << endl;
	}

	if (p->has_transport_header()) {
		if (DEBUG) {
			cout << "p->transport_header_offset(): " << unsigned(p->transport_header_offset()) << endl;
			cout << "iph->ip_v: " << unsigned(iph->ip_v) << endl;
			cout << "iph->ip_hl: " << unsigned(iph->ip_hl) << endl;
			cout << "and now..? " << unsigned(p->ip_header_length()) << endl;
			cout << "iph->ip_len: " << ntohs(iph->ip_len) << endl;
			cout << "iph->ip_id: " << unsigned(iph->ip_id) << endl;
			cout << "iph->ip_p: " << unsigned(iph->ip_p) << endl;
		}

		if (iph->ip_p == IP_PROTO_TCP) {
			DEBUG_STDOUT(cout << "tcp packet" << endl);
			const click_tcp *tcph = p->tcp_header();
			transport_header_len = tcph->th_off << 2;

		} else if (iph->ip_p == IP_PROTO_UDP) {
			DEBUG_STDOUT(cout << "udp packet" << endl);
			const click_udp *udph = p->udp_header();
			transport_header_len = ntohs(udph->uh_ulen);

		} else {
			DEBUG_STDOUT(cout << "ignoring packet header len. iph->ip_p = " << unsigned(iph->ip_p) << endl);
			return -1;
		}

		DEBUG_STDOUT(cout << "transport header offset: " << p->transport_header_offset() << ", transport header len: " << transport_header_len << endl);
	}

	headersLen =  p->network_header_offset() + sizeof(click_ip) + transport_header_len;
	DEBUG_STDOUT(cout << "headersLen: " << headersLen << endl);

	return headersLen;
}

int PacketIdEncap::getPacketLen(Packet *p) {
	DEBUG_STDOUT(cout << "getPacketLen" << endl);

	int packetLen = p->end_data() - p->data();
	DEBUG_STDOUT(cout << "packet len: " << packetLen << endl);

	return packetLen;
}

WrappedPacketData* PacketIdEncap::createWPD(Packet *p) {
	DEBUG_STDOUT(cout << "fill wrapped packet data" << endl);

	int len;

	if (_onlyHeader) {
		len = getHeadersLen(p);
	} else {
		len = getPacketLen(p);
	}

	if (len < 0) {
		return NULL;
	}

	DEBUG_STDOUT(cout << "len is: " << len << endl);

	const unsigned char *packetData = p->data();

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = PACKID_ANNO(p);
	wpd->offset = 0;
	wpd->size = len;

	unsigned char* data = new unsigned char[wpd->size];
	memcpy(data, packetData, wpd->size);
	wpd->data = data;

	DEBUG_STDOUT(cout << "done filling wrapped packet data" << endl);
	return wpd;
}

void PacketIdEncap::sendToLogger(WrappedPacketData* wpd) {
	DEBUG_STDOUT(cout << "PacketIdEncap::sendToLogger" << endl);

	if (wpd == NULL) {
		cout << "ERROR: wpd is null" << endl;
		return;
	}

	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)wpd, serialized, &len, STORE_COMMAND_TYPE);
	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE, NULL);

	if (isSucceed) {
		DEBUG_STDOUT(cout << "succeed to send" << endl);
	} else {
		DEBUG_STDOUT(cout << "failed to send" << endl);
	}

}



Packet *
PacketIdEncap::smaction(Packet *p)	// main logic - should be changed
{

	// nextSeqNum constitute of at most 28 bits.
	uint32_t nextSeqNum = getNexSeqNum();

	uint64_t innerVlan = createInnerVlan(nextSeqNum);
	uint64_t middleVlan = createMiddleVlan(nextSeqNum);
	uint64_t outerVlan = createOuterVlan(nextSeqNum);

	uint64_t unified = (innerVlan << 24) | (middleVlan << 12) | outerVlan;

	DEBUG_STDOUT(cout << "unified id: " << unified << ", seqNum: " << nextSeqNum << ", inner: " << innerVlan << ", middle: " << middleVlan << ", outer: " << outerVlan << endl);

	Packet *q = pushVlanLayer(p, innerVlan);
	q = pushVlanLayer(q, middleVlan);
	q = pushVlanLayer(q, outerVlan);

	SET_PACKID_ANNO(q, unified);
	DEBUG_STDOUT(cout << "set packet id anno: " << PACKID_ANNO(q)  << endl);

	if (_isMasterMode) {
		DEBUG_STDOUT(cout << "start handling packet - master mode..." << endl);
		WrappedPacketData* wpd = createWPD(q);
		sendToLogger(wpd);

		if (wpd != NULL) {
			delete wpd->data;
			delete wpd;
		}
		DEBUG_STDOUT(cout << "done handling packet.." << endl);

	} else {
		DEBUG_STDOUT(cout << "Doesn't need to send packet to logger - slave mode" << endl);
	}

	return q;

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
ELEMENT_REQUIRES(TCPClient)
EXPORT_ELEMENT(PacketIdEncap PacketIdEncap-PacketIdEncap)


