# Define compilers
CXX = g++       # Use g++ (server)
CC = gcc        # Use gcc (client)

# FLAGS
CXXFLAGS = -Wall -g
CFLAGS = -Wall -g

# THIS is a rule for compiling ALL
all: server client

# THIS is a rule to compile the server using g++
server: server.o p1_helper.o
	$(CXX) $(CXXFLAGS) -o server server.o p1_helper.o

# THIS is a rule to compile the client using gcc... otherwise it wont work
client: client.o
	$(CC) $(CFLAGS) -o client client.o

# COMPILE server.o from server.c using *G++*
server.o: server.c
	$(CXX) $(CXXFLAGS) -c server.c

# COMPILE client.o from client.c using *GCC*
client.o: client.c
	$(CC) $(CFLAGS) -c client.c

# IMPORTANT -> COMPILE p1_helper.o from p1_helper.cpp using g++
p1_helper.o: p1_helper.cpp
	$(CXX) $(CXXFLAGS) -c p1_helper.cpp

# clean up compiled files
clean:
	rm -f server client *.o
