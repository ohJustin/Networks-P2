#include <iostream>
#include <arpa/inet.h> // Similar to server .. networking functions (AF_INET, SOCK_STREAM, inet_pton)
#include <unistd.h> // close() -> close socket
#include <cstring> // String functions (strlen)...

#define PORT 8080 // Port for server

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
    int sock = 0;

    struct sockaddr_in servaddr; // Holds server address INFO
    char buffer[1024] = {0}; // Holds data received from server

    // Creats socket (TCP->IP[ SOCK_STREAM ])
    if((sock == socket(AF_INET, SOCK_STREAM, 0)) < 0){
        cout << "Socket creation failed" << endl;
        return -1; // Exit upon sock creation failure.
    }

    // Build server address structure to utilize
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_port = htons(PORT); // Setting port number, htons() protects network byte order

    // Place IP address back as binary form (Converted) -> Network operations needs binary form
    // 127.0.0.1 is a loopback address
    if(inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0){
        cout << "Problem with connecting! Line 38" << endl;
        return 1;
    }

    // Connect to server with IP & Port
    if (connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        cout << "Problem with connecting! Line 44" << endl;
        return -1;
    }

    // Send message to Server

    const char* msg = "Hello from your friendly-neighbourhood client!";
    send(sock, msg, strlen(msg), 0);
    cout << "Message sent to the server" << endl;

    // Receive echoed message here
    read(sock, buffer, 1024);
    cout << "Message received from current server -> " << buffer << endl;

    close(sock);
    return 0;

}