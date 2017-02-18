/*
 * increaseVersionNumber.cc
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */


#include <click/config.h>
#include "client.hh"
#include "increaseVersionNumber.hh"
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

enum { H_MB_STATE_CALL, H_TO_MASTER_CALL, H_TO_SLAVE_CALL };

class PacketLoggerIVClient : public Client {

private:
	int getHeadersLen(Packet *p);
	int getPacketLen(Packet *p);

	void serializeWrappedPacketDataObject(int command, void* obj, char* serialized, int* len);

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	PacketLoggerIVClient(int port, char* address) : Client(port, address) {
		// do nothing
		printf("in PacketLoggerClient ctor");
	}

	WrappedPacketData* createWPD(Packet *p, bool onlyHeader);
 };

void PacketLoggerIVClient::serializeWrappedPacketDataObject(int command, void* obj, char* serialized, int* len) {
	DEBUG_STDOUT(cout << "PacketLoggerIVClient::serializeWrappedPacketDataObject" << endl);

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

	*len = sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t) + (sizeof(char) * size);

	DEBUG_STDOUT(cout << "len is: " << *len << endl);
}

void PacketLoggerIVClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	DEBUG_STDOUT(cout << "PacketLoggerIVClient::serializeObject" << endl);

	if (command == STORE_COMMAND_TYPE) {
		serializeWrappedPacketDataObject(command, obj, serialized, len);
	}
}


void PacketLoggerIVClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	DEBUG_STDOUT(cout << "PacketLoggerIVClient::handleReturnValue" << endl);

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		DEBUG_STDOUT(cout << "nothing to handle." << endl);
	}
}


int PacketLoggerIVClient::getHeadersLen(Packet *p) {
	DEBUG_STDOUT(cout << "[increaseVersion] getHeadersLen" << endl);

	int headersLen = 0;
	unsigned transport_header_len = 0;
	const click_ip *iph = p->ip_header();

	if (p->mac_header() == p->data()) {
		DEBUG_STDOUT(cout << "[increaseVersion] getHeadersLen: network header wasn't set. Using default sizes!!" << endl);
		return sizeof(click_ether) + sizeof(click_ip);
	}

	if (DEBUG) {
		cout << "has transport?" << endl;
		cout << p->has_transport_header() << endl;
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

int PacketLoggerIVClient::getPacketLen(Packet *p) {
	DEBUG_STDOUT(cout << "[increaseVersion] getPacketLen" << endl);

	int packetLen = p->end_data() - p->data();
	DEBUG_STDOUT(cout << "packet len: " << packetLen << endl);

	return packetLen;
}

WrappedPacketData* PacketLoggerIVClient::createWPD(Packet *p, bool onlyHeader) {
	DEBUG_STDOUT(cout << "[increaseVersion] fill wrapped packet data" << endl);

	int len;

	if (onlyHeader) {
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



IncreaseVersionNumber::IncreaseVersionNumber()
{
}

IncreaseVersionNumber::~IncreaseVersionNumber()
{
}

int
IncreaseVersionNumber::configure(Vector<String> &conf, ErrorHandler *errh)
{
	bool isMasterMode = true;
	bool onlyHeader = false;

	if (Args(conf, this, errh)
	.read_p("MASTER", isMasterMode)
	.read_p("ONLY_HEADER", onlyHeader)
	.complete() < 0)
		return -1;

	_isMasterMode = isMasterMode;
	_onlyHeader = onlyHeader;
	cout << "increaseVersionNumber configure" << endl;
	cout << "isMaster mode? " << _isMasterMode << endl;
	cout << "onlyHeader? " << _onlyHeader << endl;

	if (_isMasterMode) {
		client = new PacketLoggerIVClient(9097, "10.0.0.4");
		client->connectToServer();
	}

	cout << "Done configuration.." << endl;

    return 0;
}


void IncreaseVersionNumber::sendToLogger(WrappedPacketData* wpd) {
	DEBUG_STDOUT(cout << "IncreaseVersionNumber::sendToLogger" << endl);

	if (wpd == NULL) {
		DEBUG_STDOUT(cout << "ERROR: wpd is null" << endl);
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
IncreaseVersionNumber::smaction(Packet *p)	// main logic
{
	DEBUG_STDOUT(cout << "[IncreaseVersionNumber] Start increasing id version..." << endl);

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
		DEBUG_STDOUT(cout << "[IncreaseVersionNumber] increaseOuterVlan succeed." << endl);

	} else {
		DEBUG_STDOUT(cout << "[IncreaseVersionNumber] Error: invalid ethertype" << ntohs(vlan->ether_vlan_proto) << endl);
	}


	// we need to set the annotation in for supporting more than one entity in the middlebox that change the packet
	uint64_t packId = PACKID_ANNO(q);
	SET_PACKID_ANNO(q, packId+1);

	if (_isMasterMode) {
		DEBUG_STDOUT(cout << "start running master..." << endl);
		WrappedPacketData* wpd = ((PacketLoggerIVClient*)client)->createWPD(q, _onlyHeader);	//prepareTest1();
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

void IncreaseVersionNumber::changeModeToMaster() {
	_isMasterMode = true;
	printf("mode is changed to master\n");
	fflush(stdout);
}

void IncreaseVersionNumber::changeModeToSlave() {
	_isMasterMode = false;
	printf("mode is changed to slave\n");
	fflush(stdout);
}

bool IncreaseVersionNumber::isMaster() {
	return _isMasterMode;
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

String
IncreaseVersionNumber::read_handler(Element *e, void *thunk)
{
	IncreaseVersionNumber *ivn = (IncreaseVersionNumber *)e;
	switch ((intptr_t)thunk) {
	case H_MB_STATE_CALL:
		if (ivn->isMaster()) {
			return String("master");
		} else {
			return String("slave");
		}
	default:
		return "<error>";
	}
}

int
IncreaseVersionNumber::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
	IncreaseVersionNumber *ivn = (IncreaseVersionNumber *)e;
	String str = in_str;
	switch ((intptr_t)thunk) {
	case H_TO_MASTER_CALL:
		ivn->changeModeToMaster();
		return 0;
	case H_TO_SLAVE_CALL:
		ivn->changeModeToSlave();
		return 0;
	default:
		return errh->error("<internal>");
	}
}

void
IncreaseVersionNumber::add_handlers()
{
	add_read_handler("state", read_handler, H_MB_STATE_CALL);
	add_write_handler("toMaster", write_handler, H_TO_MASTER_CALL);
	add_write_handler("toSlave", write_handler, H_TO_SLAVE_CALL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IncreaseVersionNumber IncreaseVersionNumber-IncreaseVersionNumber)




