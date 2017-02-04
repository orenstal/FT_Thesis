/*
 * DistributePacketRecords.hh
 *
 *  Created on: Nov 26, 2016
 *      Author: Tal
 */

#ifndef DISTRIBUTEPACKETRECORDS_DISTRIBUTEPACKETRECORDS_HH_
#define DISTRIBUTEPACKETRECORDS_DISTRIBUTEPACKETRECORDS_HH_

#include <click/element.hh>
#include <click/packet_anno.hh>

#include <stdio.h>
#include <string.h>
#include "client.hh"

CLICK_DECLS


class DistributePacketRecords : public Element { public:
	DistributePacketRecords() CLICK_COLD;
	~DistributePacketRecords() CLICK_COLD;

	const char *class_name() const	{ return "distributePacketRecords"; }
	const char *port_count() const	{ return PORTS_1_1; }

	int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
	bool can_live_reconfigure() const	{ return true; }
	void add_handlers() CLICK_COLD;

	Packet *smaction(Packet *p);
	void push(int port, Packet *p);
	Packet *pull(int port);

	static void* print_message(void* voidArgs);
	static int connectToServer(int port, char* address);

	void changeModeToMaster();
	void changeModeToSlave();
	bool isMaster();

  private:
	
	uint16_t _mbId;
	bool _isMasterMode;
	Client *detLoggerClient;
	Client *slaveNotifierClient;
	
	Packet *smactionMaster(Packet *p);
	Packet *smactionSlave(Packet *p);
	void sendToLogger(void* pm);

	static String read_handler(Element *e, void *user_data) CLICK_COLD;
	static int write_handler(const String &str, Element *e, void *user_data, ErrorHandler *errh) CLICK_COLD;
};


CLICK_ENDDECLS

#endif /* DISTRIBUTEPACKETRECORDS_DISTRIBUTEPACKETRECORDS_HH_ */
