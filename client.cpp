#include <iostream>
#include <arpa/inet.h> // Similar to server .. networking functions (AF_INET, SOCK_STREAM, inet_pton)
#include <unistd.h> // close() -> close socket
#include <cstring> // String functions (strlen)...
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


#define MAX_DATASIZE 100 // Buffer size limit

//          Beej's Code Notes
// socket(): Creates a socket for communication.
// bind(): Binds the socket to an IP and port.
// listen(): Puts the server socket in a passive state to listen for client connections.
// accept(): Accepts a connection from a client.
// connect(): Initiates a connection from the client to the server.
// read() and send(): Used to read data from the socket and send data over the socket.
// close(): Closes the socket connection.


// Extract IP from sockaddr structure, no matter the family(IPv4 etc)
void* get_in_addr(struct sockaddr* sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr); //ipv4
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

using namespace std;

int main(int argc, char* argv[]){

    // Did user provide correct amount of arguments?
    if(argc != 2){
        cerr << "Usage: Client <config_file>" << endl;
        return 1;
    }

    string serverIP, serverPort;
    cout << "Opening .conf file for client" << endl;
    ifstream conf(argv[1]); // Should be the position of client.conf file name in command line
    if(!conf.is_open()){ // Make sure file is opened
        cerr << "Issue opening client.conf! Line 43" << endl;
        return 1;
    }

    // Reading IP and Port from config file
    string line;
    while (getline(conf, line)) {
        if (line.find("SERVER_IP=") == 0) {
            serverIP = line.substr(10);
        } else if (line.find("SERVER_PORT=") == 0) {
            serverPort = line.substr(12);
        }
    }

    conf.close();

    // Make sure IP and Port are not empty
    if(serverIP.empty() || serverPort.empty()){
        cerr << "Invalid client.conf! Line 60" << endl;
        return 1;
    }

    // Setup hints(struct addrinfo -> provides hints to getaddrinfo() resolving addresses. Helps know if IPv4... TCP or UDP) for addr resolution
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM; 

    // Retrieve addr info for server.
    cout << "Parsed Server IP: " << serverIP << " Port: " << serverPort << endl;
    int rv = getaddrinfo(serverIP.c_str(),  serverPort.c_str(), &hints, &servinfo);
    if(rv != 0){
        cerr << "getaddrinfo -> " << gai_strerror(rv) << endl;
        return 1;
    }

    int sockfd;
    // Loop thru results then make connection. Creating socket and connecting to server
    for(p = servinfo; p != NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("Client issue: socket! Line 82");
            continue;
        }

        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("Client issue: connection! Line 88");
            continue;
        }

        break;
    }

    if(p == NULL){
        cerr << "Issue connecting client! Line 96" << endl;
        return 2;
    }

    //Convert ip to readable data and print
    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
    cout << "Client -> connecting to: " << s << endl;

    freeaddrinfo(servinfo); // Free info. Done with addr info.

    // Interactive loop for the application communication
    char buff[MAX_DATASIZE];
    string msg;
    while(true){
        cout << "> ";
        getline(cin,msg);
        if(msg == "exit"){
            break;
        }
        if(send(sockfd, msg.c_str(), msg.size(), 0) == -1){
            perror("Send! Line 117");
            break;
        }


        //                      Notes
        // ssize_t recv(int sockfd, void *buf, size_t len, int flags);
        // sockfd (socket file descriptor.. identifies socket data should be received at)
        // buf -> pointer to buffer for data to be stored
        // len -> max num of bytes to be received. make the size of buffer so no extra data beyond buff's memory is printed
        // flags -> flags for recv function, set to 0 by default

        //Receive response from server ...
        int numbytes = recv(sockfd, buff, MAX_DATASIZE -1, 0);
        if(numbytes == -1){
            perror("recv issue! Line 123");
            break;
        }else if (numbytes == 0){
            cout << "Server closed the connection! Line 126" << endl;
            break;
        }

        buff[numbytes] = '\0'; // '\0' represents end of communication for now...
        cout << "Server: " << buff << endl; // Relay to user what server received from recv line 133.
    }

    close(sockfd);
    return 0;




    return 0;
}