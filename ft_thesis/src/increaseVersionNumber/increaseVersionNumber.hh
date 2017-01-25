/*
 * increaseVersionId.h
 *
 *  Created on: Nov 29, 2016
 *      Author: Tal
 */

#ifndef INCREASEVERSIONNUMBER_INCREASEVERSIONNUMBER_HH_
#define INCREASEVERSIONNUMBER_INCREASEVERSIONNUMBER_HH_

#include <click/element.hh>
#include <click/packet_anno.hh>

#include <stdio.h>
#include <string.h>
#include "../common/wrappedPacketData/wrapped_packet_data.hh"
#include "client.hh"

CLICK_DECLS


class IncreaseVersionNumber : public Element {
private:
	bool _isMasterMode;
	bool _onlyHeader;
	Client *client;

	void sendToLogger(WrappedPacketData* wpd);

public:
	IncreaseVersionNumber() CLICK_COLD;
	~IncreaseVersionNumber() CLICK_COLD;

	const char *class_name() const	{ return "increaseVersionNumber"; }
	const char *port_count() const	{ return PORTS_1_1; }

	int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
	bool can_live_reconfigure() const	{ return true; }
	void add_handlers() CLICK_COLD;

	Packet *smaction(Packet *p);
	void push(int port, Packet *p);
	Packet *pull(int port);

	void changeModeToMaster();
	void changeModeToSlave();
	bool isMaster();

	static String read_handler(Element *e, void *user_data) CLICK_COLD;
	static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;
};




#endif /* INCREASEVERSIONNUMBER_INCREASEVERSIONNUMBER_HH_ */
