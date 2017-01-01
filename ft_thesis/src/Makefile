# based on: http://stackoverflow.com/questions/5191436/creating-two-separate-executables-from-a-makefile-g
SERVER_ROOT = ./tcp_servers
CLIENT_ROOT = ./tcp_clients
CC = g++
CFLAGS = -c -Wall -DDEBUG -g
LDFLAGS = -std=c++11 -lpthread -pthread# -Wl,--no-as-needed

# Det Logger Server
DET_LOGGER_SERVER_SOURCES = $(SERVER_ROOT)/server.cc $(SERVER_ROOT)/det_logger_server.cc #./pal_api/pals_manager_test.cpp #ft_thesis.cpp pushPacketId.cpp
DET_LOGGER_SERVER_OBJECTS = $(DET_LOGGER_SERVER_SOURCES:.cpp=.o)
DET_LOGGER_SERVER_EXECUTABLE = detLoggerServer

# Det Logger Client
DET_LOGGER_CLIENT_SOURCES = $(CLIENT_ROOT)/client.cc $(CLIENT_ROOT)/det_logger_client.cc
DET_LOGGER_CLIENT_OBJECTS = $(DET_LOGGER_CLIENT_SOURCES:.cpp=.o)
DET_LOGGER_CLIENT_EXECUTABLE = detLoggerClient


# Progress Logger Server
PROGRESS_LOGGER_SERVER_SOURCES = $(SERVER_ROOT)/server.cc $(SERVER_ROOT)/progress_logger_server.cc
PROGRESS_LOGGER_SERVER_OBJECTS = $(PROGRESS_LOGGER_SERVER_SOURCES:.cpp=.o)
PROGRESS_LOGGER_SERVER_EXECUTABLE = progressLoggerServer

# Progress Logger Client
PROGRESS_LOGGER_CLIENT_SOURCES = $(CLIENT_ROOT)/client.cc $(CLIENT_ROOT)/progress_logger_client.cc
PROGRESS_LOGGER_CLIENT_OBJECTS = $(PROGRESS_LOGGER_CLIENT_SOURCES:.cpp=.o)
PROGRESS_LOGGER_CLIENT_EXECUTABLE = progressLoggerClient


# Wrapped Packets Logger Server
PACKETS_LOGGER_SERVER_SOURCES = $(SERVER_ROOT)/server.cc $(SERVER_ROOT)/packets_logger_server.cc
PACKETS_LOGGER_SERVER_OBJECTS = $(PACKETS_LOGGER_SERVER_SOURCES:.cpp=.o)
PACKETS_LOGGER_SERVER_EXECUTABLE = packetsLoggerServer

# Wrapped Packets Logger Client
PACKETS_LOGGER_CLIENT_SOURCES = $(CLIENT_ROOT)/client.cc $(CLIENT_ROOT)/packets_logger_client.cc
PACKETS_LOGGER_CLIENT_OBJECTS = $(PACKETS_LOGGER_CLIENT_SOURCES:.cpp=.o)
PACKETS_LOGGER_CLIENT_EXECUTABLE = packetsLoggerClient



.PHONY: all detLoggerServer detLoggerClient progressLoggerServer progressLoggerClient packetsLoggerServer packetsLoggerClient

all: detLoggerServer detLoggerClient progressLoggerServer progressLoggerClient packetsLoggerServer packetsLoggerClient


detLoggerServer: $(DET_LOGGER_SERVER_EXECUTABLE)

$(DET_LOGGER_SERVER_EXECUTABLE): $(DET_LOGGER_SERVER_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@


detLoggerClient: $(DET_LOGGER_CLIENT_EXECUTABLE)

$(DET_LOGGER_CLIENT_EXECUTABLE): $(DET_LOGGER_CLIENT_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@


progressLoggerServer: $(PROGRESS_LOGGER_SERVER_EXECUTABLE)

$(PROGRESS_LOGGER_SERVER_EXECUTABLE): $(PROGRESS_LOGGER_SERVER_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@


progressLoggerClient: $(PROGRESS_LOGGER_CLIENT_EXECUTABLE)

$(PROGRESS_LOGGER_CLIENT_EXECUTABLE): $(PROGRESS_LOGGER_CLIENT_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@


packetsLoggerServer: $(PACKETS_LOGGER_SERVER_EXECUTABLE)

$(PACKETS_LOGGER_SERVER_EXECUTABLE): $(PACKETS_LOGGER_SERVER_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@


packetsLoggerClient: $(PACKETS_LOGGER_CLIENT_EXECUTABLE)

$(PACKETS_LOGGER_CLIENT_EXECUTABLE): $(PACKETS_LOGGER_CLIENT_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@


.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(DET_LOGGER_SERVER_EXECUTABLE) $(DET_LOGGER_CLIENT_EXECUTABLE) 
	rm $(PROGRESS_LOGGER_SERVER_EXECUTABLE) $(PROGRESS_LOGGER_CLIENT_EXECUTABLE) 
	rm $(PACKETS_LOGGER_SERVER_EXECUTABLE) $(PACKETS_LOGGER_CLIENT_EXECUTABLE)
#	rm $(SERVER_ROOT)/*.o $(CLIENT_ROOT)/*.o