/*
 * server.hh
 *
 *  Created on: Dec 24, 2016
 *      Author: Tal
 */

#ifndef TCP_SERVERS_SERVER_HH_
#define TCP_SERVERS_SERVER_HH_

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include <set>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>


#define DEBUG true
#define DEBUG_STDOUT(x) x

//#ifdef DEBUG
//	#define DEBUG_STDOUT(x) x
//#else
//	#define DEBUG_STDOUT(x)
//#endif


#define SERVER_BUFFER_SIZE_WITHOUT_PREFIX 5120	// 5k
#define NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX 7
#define NUM_OF_DIGITS_FOR_COMMAND_PREFIX 1
#define NUM_OF_DIGITS_FOR_RET_VAL_STATUS 1
#define SERVER_BUFFER_SIZE SERVER_BUFFER_SIZE_WITHOUT_PREFIX+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX
#define MAX_RET_VAL_LENGTH 5120	// 5K
#define RESPONSE_STATE_SUCCESS "1"
#define RESPONSE_STATE_FAILURE "0"
using namespace std;

class Server;

struct ThreadArgs {
	Server* server;
	int sockfd;
};

class Server {
	private:
		int port;
		pthread_mutex_t master_set_mtx;			// mutex for master fd_set protection
		fd_set master;					// master file descriptor list
		fd_set read_fds;				// temp file descriptor list for select()
		struct sockaddr_in serveraddr;	// server address
		struct sockaddr_in clientaddr;	// client address
		int fdmax;						// maximum file descriptor number
		int listener;					// server listening socket descriptor
		int newfd;						// newly accept()ed socket descriptor
		int yes;						// for setsockopt() SO_REUSEADDR, below
		set<int> connectedClients;

		void addNewClient();
		void removeClient(int sockfdToRemove, int numOfReceivedBytes);

		void handleClientRequestThread(int sockfd);

		static void* handleClientRequestThreadHelper(void* voidArgs) {
			struct ThreadArgs* args = (struct ThreadArgs*)voidArgs;
			Server* server = args->server;
			int sockfd = args->sockfd;
			server->handleClientRequestThread(sockfd);

			return NULL;
		}

		void readCommonClientRequest(int sockfd, char* msg, int* msgLen, int* command);
		int receiveMsgFromClient (int clientSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes);
		bool sendMsg(int sockfd, char* retVal, int length);
		void writeResponseToClient(int sockfd, bool succeed, char* retVal, int retValLen);
		int checkLength (char* num, int length, int minimalExpectedValue);
		void printMsg(char* msg, int msgLen);

	protected:
		virtual void* deserializeClientRequest(int command, char* msg, int msgLen);
		virtual bool processRequest(void* obj, int command, char* retVal, int* retValLen);
		virtual void freeDeserializedObject(void* obj, int command);

	public:
		Server(int port);
		void init();
		bool run();
};

#endif /* TCP_SERVERS_SERVER_HH_ */
