#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <system_error>
#include <sys/wait.h>

#define BACKLOG 10 // For pending connections

// Approach for handling zombie processes that exhaust resources.
void sigchild_handler(int x){
    while(waitpid(-1, NULL, WNOHANG) >0);
}

// Retrieve IPv4 or IPv6 addr from struct
void* get_in_addr(struct sockaddr* sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr); // IPv4
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr); // IPv6
}


//          Beej's Code Notes
// socket(): Creates a socket for communication.
// bind(): Binds the socket to an IP and port.
// listen(): Puts the server socket in a passive state to listen for client connections.
// accept(): Accepts a connection from a client.
// connect(): Initiates a connection from the client to the server.
// read() and send(): Used to read data from the socket and send data over the socket.
// close(): Closes the socket connection.

using namespace std;

int main(int argc, char* argv[]){
    int sock_fd, new_fd; // 1. Socket file descriptor, listener.. 2. Same, but for client
    struct addrinfo hints, *servinfo, *p; // hints(IPv4 or...) - servinfo(list of results returned by getaddrinfo() - p(pointer to iterate servinfo) )
    struct sockaddr_storage their_addr; // Address of client
    socklen_t sin_size; // Hold size of their_addr. For knowing size of client addr when connection accepted.
    struct sigaction sa; // SIGCHLD signal for zombie processes. specifies how to handle signals
    char s[INET6_ADDRSTRLEN]; // readable ip address holder
    int rv; // return value for functions.. not super important
    int yes = 1; // For setting socket options,setsockopt() for example.
    string port; // read from .conf file


    // Read from server.conf file
    ifstream conf("server.conf");
    if(conf.is_open()){
        string line;
        while(getline(conf, line)){
            if(line.substr(0,9) == "SERVER_PORT="){
                port = line.substr(9);
                break;
            }
        }
        conf.close();
    }else{
        cerr << "Issues opening server.conf, Line 39" << endl;
        return -1;
    }

    // Prep hints - Retrieve addr information
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // For IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // socket type for TCP
    hints.ai_flags = AI_PASSIVE; // Use own IP addr when binding.

    // Check addrinfo for null return
    if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }

    // Looping through results and binding to first bind-able result.
    // socket(int domain/addrfamily, int sockettype, int protocol)
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket! Line 84");
            continue;
        }
        // socket options allows server to reuse same port after restarting.
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            throw system_error(errno, generic_category(), "setsockopt! Line 89");
        }
        //bind socket to curr addr(p->ai_addr) and the port.
        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            perror("server: bind");
            continue;
        }

        break; // Socket created errorless, options set, bounded to address.. Server is ready to listen for connections
    }

   freeaddrinfo(servinfo); // Done with structure, free it.

    if(p == NULL){
        cerr << "issue with binding server! Line 104";
        return 1;
    }

    if(listen(sock_fd, BACKLOG) == -1){
        throw system_error(errno, generic_category(), "Listen! Line 109");
    }

    // Setup for signal handler - Handle dead child processes
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sa, NULL) == -1){
        throw system_error(errno, generic_category(), "sigaction! Line 117");
    }

    cout << "Server -> Waiting for connections..." << endl;

    // Accept incoming connections
    while(true){
        sin_size = sizeof their_addr;
        new_fd = accept(sock_fd, (struct sockaddr*)&their_addr, &sin_size);
        if(new_fd == -1){
            perror("Accept! Line 127");
            continue; // AFter failure, go back to waiting for new connection
        }
        // Print client IP addr - inet_ntop (function to convert IP addr from binary to human readable)
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof s);
        cout << "server: got connection from " << s << endl;

        if(!fork()){
            // Child Process here.
            close(sock_fd); // Our child doesn't need a listener.
            // Handling client comm here
            char buff[100];
            int numbytes = recv(new_fd, buff, sizeof buff, 0);
            if(numbytes == -1){
                perror("recv! Line 140");
            }else{
                buff[numbytes] = '\0';
                cout << "Server has received message -> " << buff << endl;
                send(new_fd, buff, numbytes, 0); // send  message back to client
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd); // Parent does not need this.
    }

    return 0;
}