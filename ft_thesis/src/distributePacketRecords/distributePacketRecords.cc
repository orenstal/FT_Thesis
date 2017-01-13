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


#include <iostream>
using namespace std;
CLICK_DECLS


class DetLoggerClient : public Client {

protected:
	void serializeObject(void* obj, char* serialized, int* len);

public:
	DetLoggerClient(int port, char* address) : Client(port, address) {
		// do nothing
	}
};

void DetLoggerClient::serializeObject(void* obj, char* serialized, int* len) {
	cout << "DetLoggerClient::serializeObject" << endl;
	PALSManager* pm = (PALSManager*)obj;

	cout << "start serializing det_logger client" << endl;
	PALSManager::serialize(pm, serialized, len);
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
		client = new DetLoggerClient(9095, "10.0.0.5");
		client->connectToServer();
	}

	// todo this code works and designed for listening to master/slave changes (not for the regular behavior)
//	pthread_t t1;
//	pthread_create(&t1, NULL, &DistributePacketRecords::print_message, NULL);

	cout << "continue.." << endl;



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

	cout << "Socket created" << endl;

	// activate keep-alive mechanism
//	int val = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);

	//Connect to remote server
	if (connect(sockfd , (struct sockaddr *)&sock_addr_server , sizeof(sock_addr_server)) < 0) {
		cout << "ERROR: Connect failed." << endl;
		return -1;
	}

	cout<<"Connected\n";

	return sockfd;
}


void DistributePacketRecords::sendToLogger(void* pm) {
	cout << "DistributePacketRecords::runTest" << endl;

	if (pm == NULL) {
		cout << "ERROR: pm is null" << endl;
		return;
	}

	char serialized[SERVER_BUFFER_SIZE];
	int len;

	client->prepareToSend((void*)pm, serialized, &len);

	bool isSucceed = client->sendMsgAndWait(serialized, len);

	if (isSucceed) {
		cout << "succeed to send" << endl;
	} else {
		cout << "failed to send" << endl;
	}

}


Packet *
DistributePacketRecords::smaction(Packet *p)	// main logic - should be changed
{
	if (_isMasterMode) {
		cout << "start distributing packet records..." << endl;
		PALSManager* pm =(PALSManager *)PALS_MANAGER_REFERENCE_ANNO(p);
		pm->setMBId(_mbId);
		pm->setPacketId(PACKID_ANNO(p));


		gpal* newGPalsList = pm->getGPalList();
		int test = newGPalsList[0].var_id;
		cout << "gpal var id: " << test << "." << endl;
		char* text = newGPalsList[0].val;
		cout << "gpal val is: " << text << endl;


		sendToLogger(pm);
		// todo: notify slave !!
		delete pm;
		cout << "done distribution.." << endl;
	} else {
		cout << "Doesn't need to send packet records to logger - slave mode" << endl;
	}

	return p;
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


void
DistributePacketRecords::add_handlers()
{
	add_data_handlers("mb_id", Handler::h_read | Handler::h_write, &_mbId);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(TCPClient)
EXPORT_ELEMENT(DistributePacketRecords DistributePacketRecords-DistributePacketRecords)


