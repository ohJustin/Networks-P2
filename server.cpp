#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <system_error>
#include <sys/wait.h>
#include <sstream>

using namespace std;
#define BACKLOG 10 // For pending connections
#define DBFILE "users.db" // For storing users


// https://www.geeksforgeeks.org/map-associative-containers-the-c-standard-template-library-stl/
// Global variables for users
map<int, string> user_sockets;   // Map from client socket to username
map<string, string> userdb; // Map for storing username and realname

// Load users from .db file into memory
void load_users(){
    ifstream db(DBFILE);
    string line;
    while(getline(db, line)){
        stringstream ss(line); // Parse each piece of the command line for username, nickname and realname
        string username, nickname, realname;
        getline(ss, username, ':');
        getline(ss, nickname, ':');
        getline(ss, realname);
        userdb[username] = nickname + ":" + realname; // Store nickname and realname in user @username in .db
    }
    db.close(); // We're done with the DB.
}

// Helper funct. Save users to db and stay updated...
void save_users(){
    ofstream db(DBFILE);
    for(const auto& user : userdb){
        db << user.first << ":" << user.second << endl; // Save as (username:nickname:realname)
    }
    db.close();
}


// Approach for handling zombie processes that exhaust resources.
void sigchild_handler(int x){
    while(waitpid(-1, NULL, WNOHANG) >0);
}
 
// helper NICK function 'handle_nick_command'
void handle_nick_command(const string& command, int client_socket) {
    stringstream ss(command);
    string nickName, token;

    ss >> token; // NICK token (not needed?)
    ss >> nickName; // Extract the nick-name from command

    // Check for if current client socket is registered with a user in client_socket map.
    if(user_sockets.find(client_socket) == user_sockets.end()){
        cout << "Looks like no user was found for this socket. Use 'USER' command first!" << endl;
        return;
    }

    // --> User/Socket was found if reached here <--
    string userName = user_sockets[client_socket];
    string realName = userdb[userName].substr(userdb[userName].find(':') + 1);

    // Update nickname into user database / then use helper function to save in .db
    userdb[userName] = nickName + ":" + realName;
    save_users(); // Save updated info onto our userdb file

    cout << "Client set their nickname to -> " << nickName << endl;
}

// helper USER function for 'USER'... 
void handle_user_command(const string& command, int clientSocket){
    stringstream ss(command);
    string userName, token, mode, unused, realName;

    ss >> token; // Token
    ss >> userName; // Username
    ss >> mode; // Extract numeric mode
    ss >> unused; // Extract unused parameter here .. Ask Gamage what it could be
    getline(ss, realName); // Get realName, containing no spaces

    // Remove ':' from the realname for .db purposes
    if(userdb.find(userName) != userdb.end()){
        cout << "ERR_ALREADYREGISTERED: User " << userName << " is already registered." << endl;
        return;
    }

    // Add user to .db(database) with default NICK(empty) -> save to .db
    userdb[userName] = ":guest:" + realName;
    user_sockets[clientSocket] = userName;
    save_users();
    cout << "User registered: " << userName << " with realname: " << realName << endl;
}

// Implement more later
void handle_clientmsg(const string& command, int client_socket) {

    // Different commands
    if (command.find("USER") == 0) {
        handle_user_command(command, client_socket);
    } else if (command.find("NICK") == 0) {
        handle_nick_command(command, client_socket);
    } else if (command.find("PRIVMSG") == 0) {
        cout << "Private message received: " << command.substr(8) << endl;
    } else if (command.find("JOIN") == 0) {
        cout << "Client wants to join channel: " << command.substr(5) << endl;
    } else if (command.find("PART") == 0) {
        cout << "Client wants to leave channel: " << command.substr(5) << endl;
    } else if (command.find("exit") == 0) {
        cout << "Client sent exit message. Closing connection." << endl;
        return;
    } else {
        cout << "Unknown command: " << command << endl;
    }
}




//          Beej's Code Notes
// socket(): Creates a socket for communication.
// bind(): Binds the socket to an IP and port.
// listen(): Puts the server socket in a passive state to listen for client connections.
// accept(): Accepts a connection from a client.
// connect(): Initiates a connection from the client to the server.
// read() and send(): Used to read data from the socket and send data over the socket.
// close(): Closes the socket connection.


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
               handle_clientmsg(buffer, client_socket);

                // Check for "exit" message
                if (strncmp(buffer, "exit", 4) == 0) {
                    cout << "Client sent exit message. Closing connection." << endl;
                    break;
                }
                

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