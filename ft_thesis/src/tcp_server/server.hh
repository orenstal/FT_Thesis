/*
 * server.hh
 *
 *  Created on: Dec 24, 2016
 *      Author: Tal
 */

#ifndef TCP_SERVER_SERVER_HH_
#define TCP_SERVER_SERVER_HH_

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include <set>
#include <sys/select.h>


#define PORT 9095	// port to listening on
#define GPAL_VAL_SIZE 50
using namespace std;

class Server;

struct ThreadArgs {
	Server* server;
	int sockfd;
};

class Server {
	private:
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

		void readCommonClientRequest(int sockfd, char* msg, int* msgLen);
		int receiveMsgFromClient (int clientSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes);
		int checkLength (char* num, int length, int minimalExpectedValue);

		void* deserializeClientRequest(char* msg, int msgLen);
		void processRequest(void*);

	public:
		Server();
		void init();
		bool run();
};

#endif /* TCP_SERVER_SERVER_HH_ */
