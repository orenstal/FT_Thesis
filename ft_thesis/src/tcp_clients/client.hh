/*
 * client.hh
 *
 *  Created on: Dec 28, 2016
 *      Author: Tal
 */

#ifndef TCP_SERVER_CLIENT_HH_
#define TCP_SERVER_CLIENT_HH_

#include <netinet/in.h>
#include<sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>


#define SERVER_BUFFER_SIZE_WITHOUT_PREFIX 5120	// 5k
#define NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX 7
#define NUM_OF_DIGITS_FOR_COMMAND_PREFIX 1
#define SERVER_BUFFER_SIZE SERVER_BUFFER_SIZE_WITHOUT_PREFIX+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX

using namespace std;

class Client {
protected:
	struct sockaddr_in sock_addr_server;
	int sockfd;

	virtual void serializeObject(void* obj, char* serialized, int* len);

public:
	Client(int port, char* address);
	void connectToServer();
	void prepareToSend(void* obj, char* serialized, int* len, int command);
	bool sendMsg(char* serialized, int len);
	bool sendMsgAndWait(char* serialized, int length);
};


#endif /* TCP_SERVER_CLIENT_HH_ */
