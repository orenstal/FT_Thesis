# based on: http://stackoverflow.com/questions/5191436/creating-two-separate-executables-from-a-makefile-g
CC = g++
CFLAGS = -c -Wall -DDEBUG -g
LDFLAGS =
TEST_SOURCES = ft_thesis.cpp
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
TEST_EXECUTABLE = mytestprogram

.PHONY: all tests

all: tests

tests: $(TEST_EXECUTABLE)

$(TEST_EXECUTABLE): $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm mytestprogram *.o