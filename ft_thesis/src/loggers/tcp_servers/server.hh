/*
 * server.hh
 *
 *  Created on: Dec 24, 2016
 *      Author: Tal
 */

#ifndef TCP_SERVERS_SERVER_HH_
#define TCP_SERVERS_SERVER_HH_

//#define SYNC_SERVER true

#include <pthread.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include <set>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>


#ifndef SYNC_SERVER
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <map>
#include <set>
#endif



#define DEBUG false
#define DEBUG_STDOUT(x) //x

//#ifdef DEBUG
//	#define DEBUG_STDOUT(x) x
//#else
//	#define DEBUG_STDOUT(x)
//#endif


#define SERVER_BUFFER_SIZE_WITHOUT_PREFIX 300000
#define NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX 7
#define NUM_OF_DIGITS_FOR_COMMAND_PREFIX 1
#define NUM_OF_DIGITS_FOR_RET_VAL_STATUS 1
#define SERVER_BUFFER_SIZE SERVER_BUFFER_SIZE_WITHOUT_PREFIX+NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX+NUM_OF_DIGITS_FOR_COMMAND_PREFIX
#define MAX_RET_VAL_LENGTH 300000
#define RESPONSE_STATE_SUCCESS "1"
#define RESPONSE_STATE_FAILURE "0"
using namespace std;

#ifndef SYNC_SERVER
class Server;
#define MAXEVENTS 64

typedef struct ThreadInfo {
	string srcHostName;
	int id;
	set<int> *sockfds;
	int efd;
	struct epoll_event event;
	struct epoll_event *events;
	pthread_spinlock_t lock;
} ThreadInfo;

struct ThreadArgs {
	Server* server;
	ThreadInfo* info;
};
#endif


class Server {
	private:
		int port;
		struct sockaddr_in serveraddr;	// server address
		int listener;					// server listening socket descriptor
		int yes;						// for setsockopt() SO_REUSEADDR, below

#ifdef SYNC_SERVER
			pthread_mutex_t master_set_mtx;	// mutex for master fd_set protection
			fd_set master;					// master file descriptor list
			fd_set read_fds;				// temp file descriptor list for select()
			struct sockaddr_in clientaddr;	// client address
			int fdmax;						// maximum file descriptor number
			int newfd;						// newly accept()ed socket descriptor
			set<int> connectedClients;
#else
			int _threadIdSequencer;
			int efd;
			struct epoll_event event;
			struct epoll_event *events;
			map<string, ThreadInfo*> threadsInfo;
			map<int, pthread_t*> activeThreads;

			pthread_spinlock_t threadsToDeleteLock;
			set<pthread_t*> threadsToDelete;
#endif

		void addNewClient();
		void removeClient(int sockfdToRemove, int numOfReceivedBytes);
		void handleClientRequest(int sockfd);



		void readCommonClientRequest(int sockfd, char* msg, int* msgLen, int* command);
		int receiveMsgFromClient (int clientSockfd, int totalReceivedBytes, char* msg, int maximalReceivedBytes);
		bool sendMsg(int sockfd, char* retVal, int length);
		void writeResponseToClient(int sockfd, bool succeed, char* retVal, int retValLen);
		int checkLength (char* num, int length, int minimalExpectedValue);
		void printMsg(char* msg, int msgLen);

		//

#ifndef SYNC_SERVER
		int make_socket_non_blocking(int sfd);
		void create_and_bind();
		void initThreadInfo (ThreadInfo* info, string hostName);
		void doRemoveClient(int sockfdToRemove, ThreadInfo* info);
		ThreadInfo* getOrCreateThreadInfo (string hostName, bool* shouldCreateNewThread);
		ThreadInfo* getThreadInfoBySockfd(int sockfd);
		void addSockfdToThreadInfo(ThreadInfo* info, int infd);
		void removeOrphanThreads();
		void runThread(ThreadInfo* info);
		void addThreadToDeleteSet(pthread_t* th);
		void deleteThreadsFromSet();


		static void* threadRunnerHelper(void* voidArgs) {
			cout << "[threadRunnerHelper] current thread id: " << pthread_self() << endl;
			ThreadArgs* args = (ThreadArgs*)voidArgs;
			Server* server = args->server;
			ThreadInfo* info = args->info;
			server->runThread(info);

			delete args;
			return NULL;
		}
#endif

	protected:
		void initSpinLock(pthread_spinlock_t* lock);
		void lockSpinLock(pthread_spinlock_t* lock);
		void unlockSpinLock(pthread_spinlock_t* lock);
		void initMutex(pthread_mutex_t* lock);
		void lockMutex(pthread_mutex_t* lock);
		void unlockMutex(pthread_mutex_t* lock);

		virtual void* deserializeClientRequest(int command, char* msg, int msgLen);
		virtual bool processRequest(void* obj, int command, char* retVal, int* retValLen);
		virtual void freeDeserializedObject(void* obj, int command);

	public:
		Server(int port);
		void init();
		bool run();
		void intToStringDigits (int number, uint8_t numOfDigits, char* numAsStr);
};

#endif /* TCP_SERVERS_SERVER_HH_ */
