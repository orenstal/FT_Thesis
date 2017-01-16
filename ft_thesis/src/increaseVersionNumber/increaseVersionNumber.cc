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
//#include "../common/pal_api/pals_manager.hh"

#define STORE_COMMAND_TYPE 0

#include <iostream>
using namespace std;
CLICK_DECLS


class PacketLoggerIVClient : public Client {

private:
	int getHeadersLen(Packet *p);
	int getPacketLen(Packet *p);

protected:
	void serializeObject(void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command);

public:
	PacketLoggerIVClient(int port, char* address) : Client(port, address) {
		// do nothing
		printf("in PacketLoggerClient ctor");
	}

	WrappedPacketData* createWPD(Packet *p, bool onlyHeader);
 };

void PacketLoggerIVClient::serializeObject(void* obj, char* serialized, int* len) {
	click_chatter("[increaseVersion] PacketLoggerClient::serializeObject");
//	cout << "PacketLoggerClient::serializeObject" << endl;
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

//	cout << "len is: " << *len << endl;
	click_chatter("len is: %d", *len);
}

void PacketLoggerIVClient::handleReturnValue(int status, char* retVal, int len, int command) {
	cout << "PacketLoggerIVClient::handleReturnValue" << endl;

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		cout << "nothing to handle." << endl;
	}
}


int PacketLoggerIVClient::getHeadersLen(Packet *p) {
	cout << "[increaseVersion] getHeadersLen" << endl;

	int headersLen = 0;
	unsigned transport_header_len = 0;
	const click_ip *iph = p->ip_header();


	if (p->mac_header() == p->data()) {
		cout << "[increaseVersion] getHeadersLen: network header wasn't set. Using default sizes!!" << endl;
		return sizeof(click_ether) + sizeof(click_ip);
	}


	cout << "has transport?" << endl;
	cout << p->has_transport_header() << endl;

	printf("data: %p, mac: %p, network: %p, transport: %p, end-data: %d", p->data(), p->mac_header(), p->network_header(), p->transport_header(), (p->end_data() - p->data()));
	printf(", network_header_offset: %d", p->network_header_offset());
	cout << endl;

	if (p->has_transport_header()) {

		cout << "p->transport_header_offset(): " << unsigned(p->transport_header_offset()) << endl;
		cout << "iph->ip_v: " << unsigned(iph->ip_v) << endl;
		cout << "iph->ip_hl: " << unsigned(iph->ip_hl) << endl;
		cout << "and now..? " << unsigned(p->ip_header_length()) << endl;
		cout << "iph->ip_len: " << ntohs(iph->ip_len) << endl;
		cout << "iph->ip_id: " << unsigned(iph->ip_id) << endl;
		cout << "iph->ip_p: " << unsigned(iph->ip_p) << endl;

		if (iph->ip_p == IP_PROTO_TCP) {
			cout << "tcp packet" << endl;
			const click_tcp *tcph = p->tcp_header();
			transport_header_len = tcph->th_off << 2;
		} else if (iph->ip_p == IP_PROTO_UDP) {
			cout << "udp packet" << endl;
			const click_udp *udph = p->udp_header();
			transport_header_len = ntohs(udph->uh_ulen);
		} else {
			cout << "ignoring packet header len. iph->ip_p = " << unsigned(iph->ip_p) << endl;
			return -1;
		}

		cout << "transport header offset: " << p->transport_header_offset() << ", transport header len: " << transport_header_len << endl;
	}

	headersLen =  p->network_header_offset() + sizeof(click_ip) + transport_header_len;
	cout << "headersLen: " << headersLen << endl;

	return headersLen;
}

int PacketLoggerIVClient::getPacketLen(Packet *p) {
	cout << "[increaseVersion] getPacketLen" << endl;
	int packetLen = p->end_data() - p->data();
	cout << "packet len: " << packetLen << endl;

	return packetLen;
}

WrappedPacketData* PacketLoggerIVClient::createWPD(Packet *p, bool onlyHeader) {
	cout << "[increaseVersion] fill wrapped packet data" << endl;

	int len;

	if (onlyHeader) {
		len = getHeadersLen(p);
	} else {
		len = getPacketLen(p);
	}

	if (len < 0) {
		return NULL;
	}

	cout << "len is: " << len << endl;

	// todo note that upon building the packet we need to change it to unsigned char!!
	const unsigned char *packetData = p->data();

	WrappedPacketData* wpd = new WrappedPacketData;
	wpd->packetId = PACKID_ANNO(p);
	wpd->offset = 0;
	wpd->size = len;

	char* data = new char[wpd->size];
//	char data[wpd->size];
	memset(data, 0, wpd->size);

	for (int i=0; i< wpd->size; i++, packetData++) {
		cout << isprint(packetData[i]) << ",";
		cout << static_cast<unsigned>(packetData[i]) << endl;
		data[i] = packetData[i];
	}
	cout << endl;
//	cout << "sizeof(char): " << sizeof(char) << ", sizeof(char*): " << sizeof(char*) << ", sizeof(unsigned char): " << sizeof(unsigned char) << ", sizeof(unsigned char*): " << sizeof(unsigned char*) << endl;

	wpd->data = data;

	cout << "done filling wrapped packet data" << endl;
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

//	if (tci_word && !tci_word.equals("ANNO", 4)

	_isMasterMode = isMasterMode;
	_onlyHeader = onlyHeader;
	cout << "isMaster mode? " << _isMasterMode << endl;
	cout << "onlyHeader? " << _onlyHeader << endl;

	if (_isMasterMode) {
		client = new PacketLoggerIVClient(9097, "10.0.0.4");
		client->connectToServer();
	}

	// todo this code works and designed for listening to master/slave changes (not for the regular behavior)
//	pthread_t t1;
//	pthread_create(&t1, NULL, &PacketIdEncap::print_message, NULL);

	cout << "continue.." << endl;



    return 0;
}


void IncreaseVersionNumber::sendToLogger(WrappedPacketData* wpd) {
	cout << "IncreaseVersionNumber::sendToLogger" << endl;

	if (wpd == NULL) {
		cout << "ERROR: wpd is null" << endl;
		return;
	}

	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)wpd, serialized, &len, STORE_COMMAND_TYPE);

	bool isSucceed = client->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

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

//	if (!p->has_mac_header()) {
//		cout << "[increaseVersion] p hasn't mac header !!" << endl;
//	} else if (!p->has_network_header()) {
//		cout << "[increaseVersion] p hasn't network header !!" << endl;
//		cout << "mac_header_offset: " << p->mac_header_offset() << ", sizeof(click_ether): " << sizeof(click_ether) << endl;
//		cout << "network_header_offset: " << p->network_header_offset() << ", sizeof(click_ether): " << sizeof(click_ip) << endl;
////		headersLen =  p->network_header_offset() + sizeof(click_ip) + transport_header_len;
//	}

	WritablePacket *q = (WritablePacket *)p;

	click_ether_vlan *vlan = reinterpret_cast<click_ether_vlan *>(q->data()+4);
	if (vlan->ether_vlan_proto == htons(ETHERTYPE_8021Q)) {
		uint16_t tci = ntohs(vlan->ether_vlan_tci);
		vlan->ether_vlan_tci = htons(tci+1);
		cout << "[IncreaseVersionNumber] increaseOuterVlan succeed." << endl;
	} else {
		cout << "[IncreaseVersionNumber] Error: invalid ethertype" << ntohs(vlan->ether_vlan_proto) << endl;
	}


	// we need to set the annotation in for supporting more than one entity in the middlebox that change the packet
	uint64_t packId = PACKID_ANNO(q);
	SET_PACKID_ANNO(q, packId+1);

	if (_isMasterMode) {
		cout << "start running test..." << endl;
		WrappedPacketData* wpd = ((PacketLoggerIVClient*)client)->createWPD(q, _onlyHeader);	//prepareTest1();
		sendToLogger(wpd);

		if (wpd != NULL) {
			delete wpd->data;
			delete wpd;
		}
		cout << "done testing.." << endl;
	} else {
		cout << "Doesn't need to send packet to logger - slave mode" << endl;
	}

	// TODO temp! this code is temporary (testing pals manager)
	/*
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
	*/

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

//void
//IncreaseVersionNumber::add_handlers()
//{
//	add_data_handlers("producer_id", Handler::h_read | Handler::h_write, &_producerId);
//}

CLICK_ENDDECLS
EXPORT_ELEMENT(IncreaseVersionNumber IncreaseVersionNumber-IncreaseVersionNumber)




