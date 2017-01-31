/*
 * preparePacket.hh
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */

#ifndef PREPAREPACKET_PREPAREPACKET_HH_
#define PREPAREPACKET_PREPAREPACKET_HH_

#include <click/element.hh>
#include <click/packet_anno.hh>

#include <stdio.h>
#include <string.h>
#include "client.hh"

CLICK_DECLS
class PreparePacket : public Element {

public:
	PreparePacket() CLICK_COLD;
	~PreparePacket() CLICK_COLD;

	const char *class_name() const	{ return "preparePacket"; }
	const char *port_count() const	{ return PORTS_1_1; }

	int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
	bool can_live_reconfigure() const	{ return true; }
	void add_handlers() CLICK_COLD;

	Packet *smaction(Packet *p);
	void push(int port, Packet *p);
	Packet *pull(int port);

	uint64_t extractVlanByLevel(Packet *p, uint8_t level);

	void recover();
	bool isRecover();
	bool changeModeToMaster();
	void doChangeModeToMaster();
	bool isMaster();
	bool isSlave();

	void* getPals(uint64_t packId);

private:
	uint16_t _mbId;
	int _mbState;
	Client *detLoggerClient;

	void* prepareGetPalsByMBIdAndPackId(uint16_t mbId, uint64_t packId);

	static String read_handler(Element *e, void *user_data) CLICK_COLD;
	static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;
};

#endif /* PREPAREPACKET_PREPAREPACKET_HH_ */
