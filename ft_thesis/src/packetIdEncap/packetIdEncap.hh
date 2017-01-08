/*
 * packetIdEncap.hh
 *
 *  Created on: Nov 26, 2016
 *      Author: Tal
 */

#ifndef PACKETIDENCAP_PACKETIDENCAP_HH_
#define PACKETIDENCAP_PACKETIDENCAP_HH_

#include <click/element.hh>

#include <stdio.h>
#include <string.h>
#include "../common/wrappedPacketData/wrapped_packet_data.hh"
#include "client.hh"

CLICK_DECLS


class PacketIdEncap : public Element { public:
	PacketIdEncap() CLICK_COLD;
	~PacketIdEncap() CLICK_COLD;

	const char *class_name() const	{ return "packetIdEncap"; }
	const char *port_count() const	{ return PORTS_1_1; }

	int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
	bool can_live_reconfigure() const	{ return true; }
	void add_handlers() CLICK_COLD;

	Packet *smaction(Packet *p);
	void push(int port, Packet *p);
	Packet *pull(int port);

	bool isValidSeqNum(uint32_t);
	uint32_t getNexSeqNum();
	uint16_t createInnerVlan(uint32_t);
	uint16_t createMiddleVlan(uint32_t);
	uint16_t createOuterVlan(uint32_t);
	uint64_t createId();
	Packet *pushVlanLayer(Packet *p, uint16_t vlan_tci);
	const uint32_t MAX_SEQ_NUM = 268435456; // (2^28 -1) - there are 28 (36-5-3) bits for sequence numbers

	static void* print_message(void* voidArgs);
	static int connectToServer(int port, char* address);

  private:
	uint64_t _seqNum;
	uint8_t _producerId;
	Client *client;

	void runTest(WrappedPacketData* wpd);

	static String read_handler(Element *e, void *user_data) CLICK_COLD;
	static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;
};

//class Test {
//
//};
//class PacketLoggerClient : public Client {
//
//protected:
//	void serializeObject(void* obj, char* serialized, int* len);
//
//public:
//	PacketLoggerClient(int port, char* address) : Client(port, address) {
//		// do nothing
//	}
// };
//
//void PacketLoggerClient::serializeObject(void* obj, char* serialized, int* len) {
////	click_chatter("PacketLoggerClient::serializeObject");
////	cout << "PacketLoggerClient::serializeObject" << endl;
//	WrappedPacketData* wpd = (WrappedPacketData*)obj;
//	uint16_t size = wpd->size;
//
//	uint64_t *q = (uint64_t*)serialized;
//	*q = wpd->packetId;
//	q++;
//
//	uint16_t *p = (uint16_t*)q;
//	*p = wpd->offset;
//	p++;
//
//	*p = size;
//	p++;
//
//	char *r = (char*)p;
//
//	for (int i=0; i< size; i++, r++) {
//		*r = wpd->data[i];
//	}
//
//	*len = sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t) + (sizeof(char) * size);
//
////	cout << "len is: " << *len << endl;
////	click_chatter("len is: %d", *len);
//}

CLICK_ENDDECLS

#endif /* PACKETIDENCAP_PACKETIDENCAP_HH_ */
