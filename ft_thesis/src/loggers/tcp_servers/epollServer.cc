/*
 * epollServer.cc
 *
 *  Created on: Feb 19, 2017
 *      Author: Tal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <map>
#include <set>
#include <pthread.h>
#include <iostream>

using namespace std;

#define MAXEVENTS 64

class EpollServer;

typedef struct ThreadInfo {
	char srcHostName[NI_MAXHOST];
	int id;
	set<int> *sockfds;
	int efd;
	struct epoll_event event;
	struct epoll_event *events;
	pthread_spinlock_t lock;
} ThreadInfo;

struct ThreadArgs {
	EpollServer* server;
	ThreadInfo* info;
};

class EpollServer {
private:
	int _threadIdSequencer;
	int yes;
	int port;
	struct sockaddr_in serveraddr;	// server address
	int listener;
	int efd;
	struct epoll_event event;
	struct epoll_event *events;
	map<char*, ThreadInfo*> threadsInfo;
	map<int, pthread_t*> activeThreads;

	pthread_spinlock_t threadsToDeleteLock;
	set<pthread_t*> threadsToDelete;


	int make_socket_non_blocking(int sfd);
	void create_and_bind();
	void initThreadInfo (ThreadInfo* info, char* hostName);
	void addNewClient();
	void removeClient(int sockfdToRemove, int numOfReceivedBytes);
	void doRemoveClient(int sockfdToRemove, ThreadInfo* info);
	void handleClientRequest(int sockfd);
	ThreadInfo* getOrCreateThreadInfo (char* hostName, bool* shouldCreateNewThread);
	ThreadInfo* getThreadInfoBySockfd(int sockfd);
	void addSockfdToThreadInfo(ThreadInfo* info, int infd);
	void removeOrphanThreads();
	void runThread(ThreadInfo* info);
	void addThreadToDeleteSet(pthread_t* th);
	void initSpinLock(pthread_spinlock_t* lock);
	void lockSpinLock(pthread_spinlock_t* lock);
	void unlockSpinLock(pthread_spinlock_t* lock);
	void deleteThreadsFromSet();


	static void* threadRunnerHelper(void* voidArgs) {
		ThreadArgs* args = (ThreadArgs*)voidArgs;
		EpollServer* server = args->server;
		ThreadInfo* info = args->info;
		server->runThread(info);

		return NULL;
	}

public:
	EpollServer(int port);
	void init();
	void run();
};

EpollServer::EpollServer(int serverPort) {
	yes = 1;
	port = serverPort;
	_threadIdSequencer = 0;
}

int EpollServer::make_socket_non_blocking (int sfd) {
	int flags, s;

	flags = fcntl (sfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl (sfd, F_SETFL, flags);
	if (s == -1) {
		perror ("fcntl");
		return -1;
	}

	return 0;
}


void EpollServer::create_and_bind () {

	if((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		cout << "ERROR: Server-socket() error lol!" << endl;
//	  DEBUG_STDOUT(cout << "ERROR: Server-socket() error lol!" << endl);
		exit(1);
	}

  	// prevent "address already in use" error
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		cout << "ERROR: Server-setsockopt() error lol!" << endl;
//  		DEBUG_STDOUT(cout << "ERROR: Server-setsockopt() error lol!" << endl);
  		exit(1);
  	}

  	// bind
  	serveraddr.sin_family = AF_INET;
  	serveraddr.sin_addr.s_addr = INADDR_ANY;
  	serveraddr.sin_port = htons(port);
  	memset(&(serveraddr.sin_zero), '\0', 8);

  	if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
  		cout << "ERROR: Server-bind() error lol!" << endl;
//  		DEBUG_STDOUT(cout << "ERROR: Server-bind() error lol!" << endl);
  		exit(1);
  	}

  	// listen
  	if(listen(listener, 1000) == -1) {
  		cout << "ERROR: Server-listen() error lol!" << endl;
//  		 DEBUG_STDOUT(cout << "ERROR: Server-listen() error lol!" << endl);
  		 exit(1);
  	}

  	cout << "server init completed successfully" << endl;

}


void EpollServer::init() {
	create_and_bind();

	if (make_socket_non_blocking(listener) == -1)
		exit(1);

	if (listen (listener, SOMAXCONN) == -1) {
		cout << "ERROR: Server-listen() error lol!" << endl;
//		DEBUG_STDOUT(cout << "ERROR: Server-listen() error lol!" << endl);
		exit(1);
	}

	efd = epoll_create1(0);
	if (efd == -1) {
		cout << "ERROR: epoll_create1() error lol!" << endl;
		exit(1);
	}

	event.data.fd = listener;
	event.events = EPOLLIN;		//event.events = EPOLLIN | EPOLLET;

	if (epoll_ctl(efd, EPOLL_CTL_ADD, listener, &event) == -1) {
		cout << "ERROR: epoll_ctl() error lol!" << endl;
		exit(1);
	}

	/* Buffer where events are returned */
	events = (epoll_event *)calloc (MAXEVENTS, sizeof(struct epoll_event));

	initSpinLock(&threadsToDeleteLock);
}

void EpollServer::initThreadInfo (ThreadInfo* info, char* hostName) {
	printf("[EpollServer::initThreadInfo] hostName: %s\n", hostName);

	//set host name
	memset(info->srcHostName, '\0', NI_MAXHOST);
	memcpy(info->srcHostName, hostName, NI_MAXHOST);

	// set thread id
	info->id = ++_threadIdSequencer;

	// init sock fds set
	info->sockfds = new set<int>;

	// create new efd
	info->efd = epoll_create1(0);

	if (info->efd == -1) {
		cout << "ERROR: epoll_create1() error lol!" << endl;
		exit(1);
	}

	/* Buffer where events are returned */
	info->events = (epoll_event *)calloc (MAXEVENTS, sizeof(struct epoll_event));

	// init spin lock
	initSpinLock(&(info->lock));

	// add to new thread to threadsInfo map
	printf("1\n");
	threadsInfo.insert(make_pair(hostName, info));

	printf("[EpollServer::initThreadInfo] End\n");

}



void EpollServer::addNewClient() {
	// todo I need to add the host to mapping and maybe create a new thread !!
	int s;

	while (1) {
		struct sockaddr in_addr;
		socklen_t in_len;
		int infd;
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

		in_len = sizeof in_addr;
		infd = accept (listener, &in_addr, &in_len);
		if (infd == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				/* We have processed all incoming
				   connections. */
				break;
			} else {
				perror ("accept");
				break;
			}
		}

		s = getnameinfo (&in_addr, in_len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
				NI_NUMERICHOST | NI_NUMERICSERV);

		if (s == 0) {
			printf("Accepted connection on descriptor %d "
					"(host=%s, port=%s)\n", infd, hbuf, sbuf);
		}

		/* Make the incoming socket non-blocking and add it to the
		   list of fds to monitor. */

		bool shouldCreateNewThread = false;
		ThreadInfo* threadInfo = getOrCreateThreadInfo(hbuf, &shouldCreateNewThread);
		addSockfdToThreadInfo(threadInfo, infd);

		if (shouldCreateNewThread) {
			ThreadArgs threadArgs;
			threadArgs.server = this;
			threadArgs.info = threadInfo;
			pthread_t* thread = new pthread_t;
			activeThreads.insert(make_pair(threadInfo->id, thread));
			pthread_create(thread, NULL, &EpollServer::threadRunnerHelper, (void*)&threadArgs);
		}
	}
}

ThreadInfo* EpollServer::getOrCreateThreadInfo (char* hostName, bool* shouldCreateNewThread) {
	printf("[EpollServer::getOrCreateThreadInfo] hostName: %s\n", hostName);

	if (threadsInfo.find(hostName) != threadsInfo.end()) {
		printf("threadsInfo has %s\n", hostName);
		*shouldCreateNewThread = false;
		return threadsInfo[hostName];
	} else {
		printf("threadsInfo doesn't has %s. creating new one..\n", hostName);
		ThreadInfo *info = new ThreadInfo;
		initThreadInfo(info, hostName);
		*shouldCreateNewThread = true;
		return info;
	}

	printf("[EpollServer::getOrCreateThreadInfo] End\n");
}

void EpollServer::addSockfdToThreadInfo(ThreadInfo* info, int infd) {

	printf("[EpollServer::addSockfdToThreadInfo] infd: %d\n", infd);

	/* Make the incoming socket non-blocking and add it to the
	   list of fds to monitor. */
	int s = make_socket_non_blocking (infd);
	if (s == -1)
		abort ();

	printf("1\n");

	lockSpinLock(&(info->lock));

	info->event.data.fd = infd;
	info->event.events = EPOLLIN;		//EPOLLIN | EPOLLET;
	s = epoll_ctl (info->efd, EPOLL_CTL_ADD, infd, &(info->event));

	printf("4\n");

	if (s == -1) {
		perror ("epoll_ctl");
		abort ();
	}

	printf("5\n");

	unlockSpinLock(&(info->lock));

	info->sockfds->insert(infd);

	printf("num of sockets to listen to: %d\n", (int)info->sockfds->size());

	printf("[EpollServer::addSockfdToThreadInfo] End\n");
}

void EpollServer::removeClient(int sockfdToRemove, int numOfReceivedBytes) {
	// todo I need to remove from host mapping and maybe terminate the thread !!

	cout << "Start removing client " << sockfdToRemove << endl;

	// got error or connection closed by client
	if(numOfReceivedBytes == 0) {
		// connection closed
		cout << "socket " << sockfdToRemove << " hung up" << endl;
	} else {
		cout << "ERROR: recv() error lol!" << endl;
	}

	doRemoveClient(sockfdToRemove, NULL);

	cout << "Done removing client " << sockfdToRemove << endl;
}

void EpollServer::doRemoveClient(int sockfdToRemove, ThreadInfo* info) {
	cout << "[EpollServer::doRemoveClient] Start" << endl;

	if (info == NULL) {
		info = getThreadInfoBySockfd(sockfdToRemove);

		if (info == NULL) {
			return;
		}
	}

	lockSpinLock(&(info->lock));
	if (info->sockfds->find(sockfdToRemove) != info->sockfds->end()) {
		info->sockfds->erase(sockfdToRemove);
	}
	unlockSpinLock(&(info->lock));

	/* Closing the descriptor will make epoll remove it
	   from the set of descriptors which are monitored. */
	close(sockfdToRemove);
	removeOrphanThreads();
	cout << "[EpollServer::doRemoveClient] Done";
}

ThreadInfo* EpollServer::getThreadInfoBySockfd(int sockfd) {
	cout << "[EpollServer::getThreadInfoBySockfd] Start" << endl;
	ThreadInfo* info = NULL;

	for (map<char*, ThreadInfo*>::iterator iter = threadsInfo.begin(); iter != threadsInfo.end(); ) {
		info = iter->second;
		break;
	}

	cout << "[EpollServer::getThreadInfoBySockfd] End" << endl;
	return info;
}


void EpollServer::handleClientRequest(int sockfd) {
	// todo this function should be replaced with this of server.cc
	int done = 0;

	while (1) {
		ssize_t count;
		char buf[512];

		count = read (sockfd, buf, sizeof buf);
		if (count == -1) {
			/* If errno == EAGAIN, that means we have read all
			   data. So go back to the main loop. */
			if (errno != EAGAIN) {
				cout << "ERROR: failed to read from sockfd: " << sockfd << endl;
				done = 1;
			}

			break;

		} else if (count == 0) {
			/* End of file. The remote has closed the
			   connection. */
			done = 1;
			break;
		}

		/* Write the buffer to standard output */
		if (write (1, buf, count) == -1) {
			cout << "ERROR: failed to write to standard output!" << endl;
		}
	}

	if (done) {
		removeClient(sockfd, 0);	// todo it doesn't suppose to be 0 in the second argument !!
	}
}


void EpollServer::run() {
	int s;

	/* The event loop */
	while (1) {
		int n, i;

		n = epoll_wait (efd, events, MAXEVENTS, -1);

		cout << "Server-epoll_wait() is OK..." << endl;
//		DEBUG_STDOUT(cout << "Server-epoll_wait() is OK..." << endl);

		cout << "About to delete orphan threads" << endl;
		deleteThreadsFromSet();

		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
					(!(events[i].events & EPOLLIN))) {
				/*  An error has occured on this fd, or the socket is not
					ready for reading (why were we notified then?) */
				cout << "ERROR: epoll error. continue to next socket.." << endl;
				doRemoveClient(events[i].data.fd, NULL);
//				close(events[i].data.fd);
				continue;

			} else if (listener == events[i].data.fd) {
				/* We have a notification on the listening socket, which
	               means one or more incoming connections. */
				addNewClient();
				continue;
			} /*else {
				// We have data on the fd waiting to be read. Read and
	            //   display it. We must read whatever data is available
	            //   completely, as we are running in edge-triggered mode
	            //   and won't get a notification again for the same
	            //   data.

				handleClientRequest(events[i].data.fd);
			}*/
		}
	}

	free (events);
	close(listener);
}

void EpollServer::runThread(ThreadInfo* info) {
	int s;

	/* The event loop */
	while (1) {
		int n, i;

		n = epoll_wait (info->efd, info->events, MAXEVENTS, -1);

		cout << "Server-epoll_wait() is OK for " << info->srcHostName << "..." << endl;
//		DEBUG_STDOUT(cout << "Server-epoll_wait() is OK for " << info->srcHostName << "..." << endl);

		for (i = 0; i < n; i++) {
			if ((info->events[i].events & EPOLLERR) || (info->events[i].events & EPOLLHUP) ||
					(!(info->events[i].events & EPOLLIN))) {
				/*  An error has occured on this fd, or the socket is not
					ready for reading (why were we notified then?) */
				cout << "ERROR: epoll error. continue to next socket.." << endl;
				doRemoveClient(events[i].data.fd, info);
//				close(info->events[i].data.fd);
				continue;

			} else {
				/* We have data on the fd waiting to be read. Read and
	               display it. We must read whatever data is available
	               completely, as we are running in edge-triggered mode
	               and won't get a notification again for the same
	               data. */

				handleClientRequest(info->events[i].data.fd);
			}
		}
	}
}


void EpollServer::removeOrphanThreads() {
	cout << "[EpollServer::removeOrphanThreads] Start" << endl;

	for (map<char*, ThreadInfo*>::iterator iter = threadsInfo.begin(); iter != threadsInfo.end(); ) {
		ThreadInfo* info = iter->second;
		lockSpinLock(&(info->lock));

		if (info->sockfds->size() == 0) {
			cout << "erasing thread id: " << info->id << ", hostname: " << info->srcHostName << endl;
			delete info->events;
			delete info;

			addThreadToDeleteSet(activeThreads[info->id]);
			activeThreads.erase(info->id);
			threadsInfo.erase(iter++);
		} else {
			cout << "thread id: " << info->id << ", hostname: " << info->srcHostName << ", sockfdsSize: " << info->sockfds->size() << endl;
			iter++;
		}

		unlockSpinLock(&(info->lock));
	}

	cout << "[EpollServer::removeOrphanThreads] End" << endl;
}

void EpollServer::addThreadToDeleteSet(pthread_t* th) {
	cout << "[EpollServer::addThreadToDeleteSet] Start" << endl;

	lockSpinLock(&threadsToDeleteLock);
	threadsToDelete.insert(th);
	unlockSpinLock(&threadsToDeleteLock);

	cout << "[EpollServer::addThreadToDeleteSet] End" << endl;
}

void EpollServer::initSpinLock(pthread_spinlock_t* lock) {
	cout << "[EpollServer::initSpinLock] Start" << endl;

	int ret = pthread_spin_init(lock, PTHREAD_PROCESS_SHARED);

	if (ret != 0) {
		perror ("pthread_spin_init error");
		abort ();
	}

	cout << "spin lock was initialized" << endl;
}


void EpollServer::lockSpinLock(pthread_spinlock_t* lock) {
	cout << "trying to pass spin lock" << endl;
	int ret = pthread_spin_lock(lock);

	if (ret != 0) {
		perror ("pthread_spin_lock error");
		abort ();
	}

	cout << "spin lock is locked" << endl;
}

void EpollServer::unlockSpinLock(pthread_spinlock_t* lock) {
	int ret = pthread_spin_unlock(lock);

	if (ret != 0) {
		perror ("pthread_spin_lock error");
		abort ();
	}

	cout << "spin lock is free" << endl;
}


void EpollServer::deleteThreadsFromSet() {
	cout << "[EpollServer::deleteThreadsFromSet] Start" << endl;
	lockSpinLock(&threadsToDeleteLock);

	for (set<pthread_t*>::iterator iter = threadsToDelete.begin(); iter != threadsToDelete.end(); iter++) {
		pthread_t* th = *iter;
		cout << "~" << endl;
		delete th;
	}

	threadsToDelete.clear();

	unlockSpinLock(&threadsToDeleteLock);
	cout << "[EpollServer::deleteThreadsFromSet] End" << endl;
}




int main (int argc, char *argv[]) {

	if (argc != 2) {
		fprintf (stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	int port = atoi(argv[1]);
	EpollServer* server = new EpollServer(port);
	server->init();
	server->run();

	delete server;

	return EXIT_SUCCESS;
}


