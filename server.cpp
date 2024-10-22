#include <iostream> 
#include <cstring> // String functions ... memset
#include <arpa/inet.h> // Networking Functions (TCP) - IF_INET .. SOCK_STREAM .. INADDR_ANY, mand more
#include <unistd.h> // For close() .. close socket
#include <fstream> // File input/output -> .conf files

//#define PORT 8080 //  Server --> Port

//          Beej's Code Notes
// socket(): Creates a socket for communication.
// bind(): Binds the socket to an IP and port.
// listen(): Puts the server socket in a passive state to listen for client connections.
// accept(): Accepts a connection from a client.
// connect(): Initiates a connection from the client to the server.
// read() and send(): Used to read data from the socket and send data over the socket.
// close(): Closes the socket connection.

using namespace std;

int main(){
    int server_fd, newsocket;

    struct sockaddr_in addr; // Server address info structure
    int addrlen = sizeof(addr);
    char buffer[1024] = {0}; // Buffer for storing client.cpp data
    int port = 8080; // a default port

    // Read from server.conf file
    ifstream conf("server.conf");
    if(conf.is_open()){
        string line;
        while(getline(conf, line)){
            if(line.find("TCP_PORT") != -1){
                port = stoi(line.substr(line.find("=") + 1));
            }
        }
        conf.close();
    }else{
        cout << "Issues opening server.conf, Line 39" << endl;
    }

    // Creating socket with TCP-and-IP & checking for error
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) == 0) {
        perror("Issue creating socket! Line 16");
        exit(EXIT_FAILURE);
    }

    // Filling server address structure -> Setup
    addr.sin_family = AF_INET; // IPv4 Addr
    addr.sin_addr.s_addr = INADDR_ANY; // Accepting connections from ANY IP addr
    addr.sin_port = htons(port); // Set port # -> htons basically protects network byte order.

    // Bind socket to IP and Port from .conf file & checking for error
    if(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("Binding socket issue! Line 36");
    }

    // Listen for connections -> Max = 3 (clients) , for now.
    if(listen(server_fd, 3) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept connection here
    if((newsocket = accept(server_fd, (struct sockaddr*)&addr, (socklen_t*)&addrlen)) < 0){
        perror("Issue with accepting connection! Line 66");
        exit(EXIT_FAILURE);
    }

    // Reading client's message and printing it
    read(newsocket, buffer, 1024); // 1024 bytes read into buffer from client
    cout << "Message received from client!: " << buffer << endl;

    // Send message back to client to show server's perspective
    send(newsocket, buffer, strlen(buffer), 0); 

    close(newsocket);

    return 0;
}