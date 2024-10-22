#include <iostream> 
#include <cstring> // String functions ... memset
#include <arpa/inet.h> // Networking Functions (TCP) - IF_INET .. SOCK_STREAM .. INADDR_ANY, mand more
#include <unistd.h> // For close() .. close socket

#define PORT 8080 //  Server --> Port

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

    // Creating socket with TCP-and-IP & checking for error
    if((server_fd = socket(AF_INET,SOCKSTREAM,0)) == 0) {
        perror("Issue creating socket! Line 16");
        exit(EXIT_FAILURE);
    }

    // Filling server address structure -> Setup
    addr.sin_family = AF_INET; // IPv4 Addr
    addr.sin_addr.s_addr = INADDR_ANY; // Accepting connections from ANY IP addr
    address.sin_port = htons(PORT); // Set port # -> htons basically protects network byte order.

    // Bind socket to IP and Port from .conf file & checking for error
    if(bind(server_fd, (struct sockaddress*)&addr, sizeof(address)) < 0){
        perror("Binding socket issue! Line 36");
    }

    // Listen for connections -> Max = 3 (clients) , for now.
    if(listen(server_fd, 3) < 0){
        perror("Listen failed");
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