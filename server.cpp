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
// void* get_in_addr(struct sockaddr* sa){
//     if(sa->sa_family == AF_INET){
//         return &(((struct sockaddr_in*)sa)->sin_addr); // IPv4
//     }
//     return &(((struct sockaddr_in6*)sa)->sin6_addr); // IPv6
// }


//          Beej's Code Notes
// socket(): Creates a socket for communication.
// bind(): Binds the socket to an IP and port.
// listen(): Puts the server socket in a passive state to listen for client connections.
// accept(): Accepts a connection from a client.
// connect(): Initiates a connection from the client to the server.
// read() and send(): Used to read data from the socket and send data over the socket.
// close(): Closes the socket connection.

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: Server <config_file>" << endl;
        return 1;
    }

    string port;
    cout << "Opening .conf file for server" << endl;
    ifstream conf(argv[1]); // Command line input for .conf file
    if (!conf.is_open()) { // Check if conf file opened.
        cerr << "Issue opening server.conf! Line 12" << endl;
        return 1;
    }

    // Read port from config file
    string line;
    while (getline(conf, line)) {
        if (line.find("SERVER_PORT=") == 0) {
            port = line.substr(12);
        }
    }
    conf.close();

    if (port.empty()) {
        cerr << "Invalid server.conf! Line 20" << endl;
        return 1;
    }

    // Create a socket for listening
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed! Line 72");
        exit(EXIT_FAILURE);
    }

    // Allow reuse of the port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed! Line 79");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Define the server address structure
    struct sockaddr_in address;
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Accept any IP (could be localhost or another IP)
    address.sin_port = htons(stoi(port)); // Use the port from server.conf

    // Binding the socket to the IP and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed! Line 92");
        close(server_fd);
        exit(EXIT_FAILURE);
    } else {
        cout << "Server successfully bound to port !" << port << endl;
    }

    // Start listening for incoming connections
    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    cout << "Server -> Waiting for connections..." << endl;

    // Reap dead processes
    struct sigaction sa;
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    // Accept client connections
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_socket;
    
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        cout << "Connection established with client!" << endl;

        // Communication with client (Receive and send back a message)
        char buffer[1024] = {0};
        while (true) {
            int valread = read(client_socket, buffer, 1024);
            if (valread > 0) {
                cout << "Message received from client: " << buffer << endl;


                
                // Handle IRC commands ! IMPLEMENT LATER
                if (strncmp(buffer, "NICK", 4) == 0) {
                    cout << "Nick command received: " << buffer + 5 << endl; 
                    // Assign nickname or send error if invalid
                } else if (strncmp(buffer, "JOIN", 4) == 0) {
                    cout << "Join command received: " << buffer + 5 << endl; 
                    // Add user to a channel
                } else if (strncmp(buffer, "PRIVMSG", 7) == 0) {
                    cout << "Private message: " << buffer + 8 << endl;
                    // Send message to the channel or user
                } else if (strncmp(buffer, "PART", 4) == 0) {
                    cout << "Part command received: " << buffer + 5 << endl;
                    // Remove user from the channel
                } else if (strncmp(buffer, "exit", 4) == 0) {
                    cout << "Client sent exit message. Closing connection." << endl;
                    break;
        }

                // Check for "exit" message
                // if (strncmp(buffer, "exit", 4) == 0) {
                //     cout << "Client sent exit message. Closing connection." << endl;
                //     break;
                // }
                

                // Send our rev. message back to our client.
                send(client_socket, buffer, strlen(buffer), 0);
               // cout << "Message sent back to client!" << endl;
                memset(buffer, 0, 1024); // Clear buffer after each message
            } else if (valread == 0) {
                cout << "Client disconnected." << endl;
                break;
            } else {
                perror("Read error");
                break;
            }
        }

        close(client_socket); // Close the client socket after handling communication
    }

    close(server_fd); // Close the server socket
    return 0;
}