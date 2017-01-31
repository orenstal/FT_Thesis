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
#include "distributePacketRecords.hh"
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

#include "../common/pal_api/pals_manager.hh"

#define STORE_COMMAND_TYPE 0

#include <iostream>
using namespace std;
CLICK_DECLS


enum { H_MB_STATE_CALL, H_TO_MASTER_CALL, H_TO_SLAVE_CALL };


class DetLoggerClient : public Client {

private:
	void serializePalsManagerObject(int command, void* obj, char* serialized, int* len);

protected:
	void serializeObject(int command, void* obj, char* serialized, int* len);
	void handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj);

public:
	DetLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
};

void DetLoggerClient::serializePalsManagerObject(int command, void* obj, char* serialized, int* len) {
	DEBUG_STDOUT(cout << "[DetLoggerClient::serializePalsManagerObject] Start" << endl);
	PALSManager* pm = (PALSManager*)obj;

#ifdef DEBUG
	cout << "start serializing det_logger client" << endl;
	cout << "serialized: " << serialized << ", len: " << *len << ", mbId: " << pm->getMBId() << endl;
	cout << ", packid: " << pm->getPacketId() << endl;
	cout << "pm->getGPalSize()" << pm->getGPalSize() << endl;
	cout << "pm->getSPalSize()" << pm->getSPalSize() << endl;
#endif

	PALSManager::serialize(pm, serialized, len);
	DEBUG_STDOUT(cout << "[DetLoggerClient::serializePalsManagerObject] End" << endl);
}

void DetLoggerClient::serializeObject(int command, void* obj, char* serialized, int* len) {
	DEBUG_STDOUT(cout << "DetLoggerClient::serializeObject" << endl);

	if (command == STORE_COMMAND_TYPE) {
		serializePalsManagerObject(command, obj, serialized, len);
	}
}

void DetLoggerClient::handleReturnValue(int status, char* retVal, int len, int command, void* retValAsObj) {
	DEBUG_STDOUT(cout << "DetLoggerClient::handleReturnValue" << endl);

	if (status == 0 || command == STORE_COMMAND_TYPE || len <= 0) {
		DEBUG_STDOUT(cout << "nothing to handle." << endl);
	}
}





DistributePacketRecords::DistributePacketRecords()
{
}

DistributePacketRecords::~DistributePacketRecords()
{
}

int
DistributePacketRecords::configure(Vector<String> &conf, ErrorHandler *errh)
{
	int mbId = 0;
	bool isMasterMode = true;

	if (Args(conf, this, errh)
	.read_p("MB_ID", BoundedIntArg(1, 65535), mbId)
	.read_p("MASTER", isMasterMode)
	.complete() < 0)
		return -1;


	_mbId = mbId;
	_isMasterMode = isMasterMode;
	cout << "isMaster mode? " << _isMasterMode << endl;
	cout << "_mbId: " << _mbId << endl;

	if (_isMasterMode) {
		detLoggerClient = new DetLoggerClient(9095, "10.0.0.5");
		detLoggerClient->connectToServer();
	} else {
		detLoggerClient = new DetLoggerClient(9095, "10.0.0.8");
		detLoggerClient->connectToServer();
	}

	// todo this code works and designed for listening to master/slave changes (not for the regular behavior)
//	pthread_t t1;
//	pthread_create(&t1, NULL, &DistributePacketRecords::print_message, NULL);

	cout << "Done configuration.." << endl;



    return 0;
}


void* DistributePacketRecords::print_message(void* args) {
	cout << "Hi from thread.." << endl;
	connectToServer(9095, "10.0.0.5");
	cout << "connected ??" << endl;
	while (1) {

	}

	return NULL;
}

int DistributePacketRecords::connectToServer(int port, char* address) {
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


void DistributePacketRecords::sendToLogger(void* pm) {
	DEBUG_STDOUT(cout << "DistributePacketRecords::sendToLogger" << endl);

	if (pm == NULL) {
		cout << "ERROR: pm is null" << endl;
		return;
	}

	char serialized[SERVER_BUFFER_SIZE];
	int len;

	detLoggerClient->prepareToSend(pm, serialized, &len, STORE_COMMAND_TYPE);
	bool isSucceed = detLoggerClient->sendMsgAndWait(serialized, len, STORE_COMMAND_TYPE, NULL);

	if (isSucceed) {
		DEBUG_STDOUT(cout << "succeed to send" << endl);
	} else {
		DEBUG_STDOUT(cout << "failed to send" << endl);
	}

}

Packet *
DistributePacketRecords::smactionMaster(Packet *p)
{
	DEBUG_STDOUT(cout << "start distributing packet records..." << endl);

	PALSManager* pm =(PALSManager *)PALS_MANAGER_REFERENCE_ANNO(p);
	pm->setMBId(_mbId);
	pm->setPacketId(PACKID_ANNO(p));


	gpal* newGPalsList = pm->getGPalList();
	int test = newGPalsList[0].var_id;
	DEBUG_STDOUT(cout << "gpal var id: " << test << "." << endl);

	char* text = newGPalsList[0].val;
	DEBUG_STDOUT(cout << "gpal val is: " << text << endl);

	sendToLogger(pm);
	delete pm;
	DEBUG_STDOUT(cout << "done distribution.." << endl);

	return p;
}

Packet *
DistributePacketRecords::smactionSlave(Packet *p)
{
	DEBUG_STDOUT(cout << "Slave mode - sending only data to 'progress logger'" << endl);

	PALSManager* pm = new PALSManager (_mbId, PACKID_ANNO(p));
	pm->setGPalSize(0);
	pm->setSPalSize(0);


	sendToLogger((void*)pm); //static_cast<void*>(&pm)
	delete pm;
	DEBUG_STDOUT(cout << "done distribution.." << endl);

	return NULL; // prevent the packet from be sent again (only master should send packets)
}

Packet *
DistributePacketRecords::smaction(Packet *p)	// main logic - should be changed
{
	if (_isMasterMode) {
		return smactionMaster(p);
	} else {	// slave mode
		return smactionSlave(p);
	}
}


void DistributePacketRecords::changeModeToMaster() {
	_isMasterMode = true;
	printf("mode is changed to master\n");
	fflush(stdout);
}

void DistributePacketRecords::changeModeToSlave() {
	_isMasterMode = false;
	printf("mode is changed to slave\n");
	fflush(stdout);
}

bool DistributePacketRecords::isMaster() {
	return _isMasterMode;
}


void
DistributePacketRecords::push(int, Packet *p)
{
    if (Packet *q = smaction(p))
	output(0).push(q);
}

Packet *
DistributePacketRecords::pull(int)
{
    if (Packet *p = input(0).pull())
	return smaction(p);
    else
	return 0;
}

String
DistributePacketRecords::read_handler(Element *e, void *thunk)
{
	DistributePacketRecords *dpr = (DistributePacketRecords *)e;
	switch ((intptr_t)thunk) {
	case H_MB_STATE_CALL:
		if (dpr->isMaster()) {
			return String("master");
		} else {
			return String("slave");
		}
	default:
		return "<error>";
	}
}

int
DistributePacketRecords::write_handler(const String &in_str, Element *e, void *thunk, ErrorHandler *errh)
{
	DistributePacketRecords *dpr = (DistributePacketRecords *)e;
	String str = in_str;
	switch ((intptr_t)thunk) {
	case H_TO_MASTER_CALL:
		dpr->changeModeToMaster();
		return 0;
	case H_TO_SLAVE_CALL:
		dpr->changeModeToSlave();
		return 0;
	default:
		return errh->error("<internal>");
	}
}

void
DistributePacketRecords::add_handlers()
{
	add_data_handlers("mb_id", Handler::h_read | Handler::h_write, &_mbId);

	add_read_handler("state", read_handler, H_MB_STATE_CALL);
	add_write_handler("toMaster", write_handler, H_TO_MASTER_CALL);
	add_write_handler("toSlave", write_handler, H_TO_SLAVE_CALL);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(TCPClient)
EXPORT_ELEMENT(DistributePacketRecords DistributePacketRecords-DistributePacketRecords)


