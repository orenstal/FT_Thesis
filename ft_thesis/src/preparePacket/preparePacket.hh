/*
 * preparePacket.hh
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */

#ifndef PREPAREPACKET_PREPAREPACKET_HH_
#define PREPAREPACKET_PREPAREPACKET_HH_

#include <click/element.hh>
CLICK_DECLS
class PreparePacket : public Element { public:
	PreparePacket() CLICK_COLD;
	~PreparePacket() CLICK_COLD;

	const char *class_name() const	{ return "preparePacket"; }
	const char *port_count() const	{ return PORTS_1_1; }

	Packet *smaction(Packet *p);
	void push(int port, Packet *p);
	Packet *pull(int port);

	uint64_t extractVlanByLevel(Packet *p, uint8_t level);
};

#endif /* PREPAREPACKET_PREPAREPACKET_HH_ */
