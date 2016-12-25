/*
 * based on: http://www.tenouk.com/Module41.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.hh"
#include <pthread.h>


Server::Server() {
//	master = NULL;
//	read_fds = NULL;
//	serveraddr = NULL;
//	clientaddr = NULL;
	fdmax = -1;
	listener = -1;
	newfd = -1;
	yes = 1;
}


void Server::init() {
	// clear the master and temp sets
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	// get the listener
	if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "ERROR: Server-socket() error lol!" << endl;
		//just exit lol!
		exit(1);
	}

	cout << "Server-socket() is OK..." << endl;
	//"address already in use" error message
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		cout << "ERROR: Server-setsockopt() error lol!" << endl;
		exit(1);
	}

	cout << "Server-setsockopt() is OK..." << endl;

	// bind
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(PORT);
	memset(&(serveraddr.sin_zero), '\0', 8);

	if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		cout << "ERROR: Server-bind() error lol!" << endl;
		exit(1);
	}
	cout << "Server-bind() is OK..." << endl;

	// listen
	if(listen(listener, 10) == -1) {
		 cout << "ERROR: Server-listen() error lol!" << endl;
		 exit(1);
	}

	cout << "Server-listen() is OK..." << endl;

	// add the listener to the master set
	FD_SET(listener, &master);
	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

}

bool Server::run() {
	while(1) {
		read_fds = master;

		if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			cout << "ERROR: Server-select() error lol!" << endl;
			exit(1);
		}

		cout << "Server-select() is OK..." << endl;

		//run through the existing connections looking for data to be read
		for (set<int>::iterator it=connectedClients.begin(); it!=connectedClients.end();) {
			set<int>::iterator currIter = it++;

			if(FD_ISSET(*currIter, &read_fds)) {
				if(*currIter == listener) {
					addNewClient();	// handle new connections
				} else {
					int nbytes;
					char buf[1024];

					// handle data from a client
					if((nbytes = recv(*currIter, buf, sizeof(buf), 0)) <= 0) {
						removeClient(*currIter, nbytes);
					} else {
						ThreadArgs threadArgs;
						threadArgs.server = this;
						threadArgs.sockfd = *currIter;
						pthread_t t1;
						pthread_create(&t1, NULL, &Server::handleClientRequestThreadHelper, (void*)&threadArgs);
					}
				}
			}
		}
	}
}

void Server::addNewClient() {
	int addrlen = sizeof(clientaddr);
	if((newfd = accept(listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1) {
		cout << "ERROR: Server-accept() error lol!" << endl;
	} else {
		cout << "Server-accept() is OK..." << endl;

		connectedClients.insert(newfd);
		FD_SET(newfd, &master); // add to master set

		if(newfd > fdmax) {
			fdmax = newfd;
		}

		cout << "New connection from " << inet_ntoa(clientaddr.sin_addr) << " on socket " << newfd << endl;
	}
}

void Server::removeClient(int sockfdToRemove, int numOfReceivedBytes) {
	// got error or connection closed by client
	if(numOfReceivedBytes == 0) {
		// connection closed
		cout << "socket " << sockfdToRemove << " hung up" << endl;
	} else {
		cout << "ERROR: recv() error lol!" << endl;
	}

	// close it...
	close(sockfdToRemove);

	// remove from master set
	FD_CLR(sockfdToRemove, &master);
	connectedClients.erase(sockfdToRemove);
}

void Server::handleClientRequestThread(int sockfd) {
	char* msg = new char[5120 + 7];
	int msgLen;

	readCommonClientRequest(sockfd, msg, &msgLen);

	// now we have msg and msgLen.
	void* obj = deserializeClientRequest(msg, msgLen);
	processRequest(obj);

	delete obj;
	delete msg;
}


void Server::readCommonClientRequest(int sockfd, char* msg, int* msgLen) {

	// receive message in the following format: [7 digits representing the client name's length][client name]
	int totalReceivedBytes = receiveMsgFromClient(sockfd, 0, msg, 7);

	// client socket was closed and removed
	if (totalReceivedBytes == -1) {
		return;
	}


	char *ptr = msg + totalReceivedBytes;

	// convert the received length of the client name to int
	char len[7 + 1];
	memset(len, '\0', 7 + 1);
	strncpy(len, msg, 7);
	*msgLen = checkLength(len, 7, 1);

	// receive the client name
	totalReceivedBytes = receiveMsgFromClient(sockfd, totalReceivedBytes, ptr, *msgLen+7);

	// client socket was closed and removed
	if (totalReceivedBytes == -1) {
		return;
	}

}

/*
 * This function checks that the inserted num is actually a number, and that is at least of minimalExpectedValue.
 * length is num size. If num passed these checks - returns it as integer. Otherwise returns -1.
 */
int Server::checkLength (char* num, int length, int minimalExpectedValue) {
	for (int i=0; i<length; i++) {
		if (isdigit(num[i]) == false) {
			return -1;
		}
	}

	int convertedNum = atoi(num);

	if (convertedNum < minimalExpectedValue) {
		return -1;
	}

	return convertedNum;
}

/*
 * This function handles the receiving of a message from the client clientName (with socket fd clientSockfd)
 * of length (maximalReceivedBytes - totalReceivedBytes) from totalReceivedBytes to maximalReceivedBytes.
 * It returns how many it received in here, and inserts this message to msg.
 * In case that the client disconnected abruptly - it turns the terminateItself flag to true.
 */
int Server::receiveMsgFromClient (int clientSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes)
{
	char * ptr = msg;

	while (totalReceivedBytes < maximalReceivedBytes) {
		int ret = recv(clientSockfd, ptr, maximalReceivedBytes - totalReceivedBytes, 0);

		if (ret == 0) {
			removeClient(clientSockfd, ret);
			return -1;
		}

		if (ret < 0) {
			// trying to receive one more time after failing the first time
			ret = recv(clientSockfd, ptr, maximalReceivedBytes - totalReceivedBytes, 0);

			if (ret <= 0) {
				removeClient(clientSockfd, ret);
				return -1;
			}
		}

		totalReceivedBytes += ret;
		ptr += ret;
	}

	return totalReceivedBytes;
}

void* Server::deserializeClientRequest(char* msg, int msgLen) {
	// temp implementation
	char* obj = new char[100];

	return (void*)obj;
}

void Server::processRequest(void* obj) {

}

//void *print_message(void* context){
//	int *val = (int*)context;
//    cout << "Threading: " << *val << "\n";
//}


int main(int argc, char *argv[])
{
	cout << "start" << endl;

//	pthread_t t2;
//	int a = 4;
//	pthread_create(&t2, NULL, &print_message, &a);
//	cout << "Hello";

	Server *server = new Server();
	server->init();
	server->run();
	return 0;
}
