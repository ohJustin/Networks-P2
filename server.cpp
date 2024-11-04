#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <vector>
#include <unistd.h>
#include <signal.h>
#include <cerrno>
#include <system_error>
#include <sys/wait.h>
#include <sstream>
#include <algorithm>

using namespace std;

// Reply codes for (IRC) messages
#define RPL_WELCOME "001"           // Welcome message
#define RPL_YOURHOST "002"          // Your host information
#define RPL_MYINFO "004"            // Server information
#define ERR_NOSUCHNICK "401"        // No such nick/channel
#define ERR_NICKNAMEINUSE "433"     // Nickname already in use
#define ERR_NEEDMOREPARAMS "461"    // Not enough parameters
#define ERR_NOTONCHANNEL "442"      // User is not on that channel
#define RPL_LIST "322"              // channel list
#define RPL_LISTEND "323"           // end of channel list
#define ERR_USERSDONTMATCH "502" // cannot set mode for other users

#define BACKLOG 10 // For pending connections
#define DBFILE "users.db" // For storing users

// https://www.geeksforgeeks.org/map-associative-containers-the-c-standard-template-library-stl/
// Global variables for users
map<int, string> user_sockets;   // Map from client socket to username
map<string, string> userdb; // Map for storing username and realname
map<string, vector<string>> channels; // Map for channels with users per channel
map<int, bool> nickname_set_map;

// Mode flags for users
map<string, string> user_modes; // -> "a" (away), "i" (invisible), "w" (wallops)
// Mode flags for channels
map<string, string> channel_modes; // -> "a" (anonymous), "s" (secret), "p" (private)
map<string, string> channel_topics; // Store channel topics


void send_reply(int client_socket, const string& code, const string& message) {
    string reply = "Server Response [" + code + "]: " + message + "\\n";
    send(client_socket, reply.c_str(), reply.size(), 0);
}


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

    ss >> token; // "NICK"
    ss >> nickName; // Extract nickname

    // Check if nickname is provided
    if (nickName.empty()) {
        send_reply(client_socket, ERR_NEEDMOREPARAMS, "NICK :Not enough parameters");
        return;
    }

    // Allow NICK to be set even if the client is not registered
    if (user_sockets.find(client_socket) == user_sockets.end()) {
        cout << "Unregistered client set their nickname to -> " << nickName << endl;
        user_sockets[client_socket] = nickName; // Temporarily set nickname for unregistered user
        nickname_set_map[client_socket] = true;
        send_reply(client_socket, RPL_MYINFO, "004 Nickname set to " + nickName);
        return;
    }

    // If the client is already registered, update the nickname
    string userName = user_sockets[client_socket];
    string realName = userdb[userName].substr(userdb[userName].find(':') + 1);

    // Update nickname in the user database
    userdb[userName] = nickName + ":" + realName;
    save_users(); // Save changes to the database

    cout << "Client updated their nickname to -> " << nickName << endl;
    nickname_set_map[client_socket] = true;
    send_reply(client_socket, RPL_MYINFO, "004 Nickname updated to " + nickName);
}


// helper USER function for 'USER'... 
void handle_user_command(const string& command, int clientSocket) {
    stringstream ss(command);
    string userName, token, mode, unused, realName;

    ss >> token;     // USER token
    ss >> userName;  // Extract username
    ss >> mode;      // Extract mode (should be numeric)
    ss >> unused;    // Extract unused parameter
    getline(ss, realName);  // Extract the realname, which may contain spaces

    // Remove the leading ':' from realname
    if (!realName.empty() && realName[0] == ':') {
        realName.erase(0, 1);
    }

    // Check if the username already exists
    if (userdb.find(userName) != userdb.end()) {
        send_reply(clientSocket, "462", "You are already registered"); // ERR_ALREADYREGISTRED
        return;
    }

    // Add the user to the user database with a default nickname (empty) and save to .db
    userdb[userName] = ":guest:" + realName;
    user_sockets[clientSocket] = userName;
    save_users(); // Save to .db

    // Send welcome message
    send_reply(clientSocket, RPL_WELCOME, "Welcome to the IRC server, " + userName);
    cout << "User registered: " << userName << " with realname: " << realName << endl;
}

// HANDLE -> MODE COMMAND
void handle_mode_command(const string& command, int client_socket, const string& username){
    stringstream ss(command);
    string token, target, mode;
    ss >> token >> target >> mode;

    if (target.empty() || mode.empty()) {
        send_reply(client_socket, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }

    // HANDLE THE -> user mode
    if (user_sockets.find(client_socket) != user_sockets.end() && target == username) {
        user_modes[username] = mode; // Set mode for the user
        send_reply(client_socket, RPL_MYINFO, username + " :" + mode); // Confirmation message
    }
    // HANDLE THE -> channel mode
    else if (channels.find(target) != channels.end()) {
        channel_modes[target] = mode; // Set mode for the channel
        send_reply(client_socket, RPL_MYINFO, "Channel " + target + " mode set to: " + mode); // Confirmation message
    }
    else {
        send_reply(client_socket, ERR_USERSDONTMATCH, "MODE :Cannot set mode for " + target);
    }
}

// HANDLE QUIT COMMAND
void handle_quit_command(int client_socket, const string& username){
    // Inform users of disconnection
    send_reply(client_socket, RPL_MYINFO, "Goodbye " + username);
    close(client_socket);
}

// HANDLE JOIN COMMAND
void handle_join_command(const string& command, int client_socket, const string& username){
    stringstream ss(command);
    string token, channel; // hold values
    ss >> token >> channel; // Extract JOIN / Channel Name

    if(channel.empty()){
        send_reply(client_socket, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }

    // Add user to channel
    channels[channel].push_back(username);
    send_reply(client_socket, RPL_LIST, channel + " :Users in channel"); // Sample Success
    send_reply(client_socket, "323", channel + " :End of ./JOIN list!");
}


// HANDLE PART COMMAND (Leaving Channel)
void handle_part_command(const string& command, int client_socket, const string& username){
    stringstream ss(command);
    string token, channel;
    ss >> token >> channel;

    if (channel.empty() || channels[channel].empty()) {
        send_reply(client_socket, ERR_NOTONCHANNEL, channel + " :You're not on that channel");
        return;
    }

    auto& users = channels[channel];
    users.erase(std::remove(users.begin(), users.end(), username), users.end());
    send_reply(client_socket, RPL_LISTEND, channel + " :Left the channel");
}

// Handle privmsg COMMAND
void handle_privmsg_command(const string& command, int client_socket, const string& username) {
    stringstream ss(command);
    string token, recipient, message;
    ss >> token >> recipient;
    getline(ss, message);

    if (message.empty() || recipient.empty()) {
        send_reply(client_socket, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
        return;
    }

    // Trim leading space from the message if necessary
    if (!message.empty() && message[0] == ':') {
        message.erase(0, 1);
    }

    // Check if recipient is a channel
    if (channels.find(recipient) != channels.end()) {
        // Broadcast the message to all users in the channel
        for (const string& user : channels[recipient]) {
            int recipient_socket = -1;
            for (const auto& entry : user_sockets) {
                if (entry.second == user) {
                    recipient_socket = entry.first;
                    break;
                }
            }

            if (recipient_socket != -1 && recipient_socket != client_socket) {
                send_reply(recipient_socket, "PRIVMSG", username + " says to " + recipient + ": " + message);
            }
        }
        return;
    }


    // Check if the recipient exists
    bool recipient_found = false;
    int recipient_socket;
    for (const auto& entry : user_sockets) {
        if (entry.second == recipient) {
            recipient_found = true;
            recipient_socket = entry.first;
            break;
        }
    }

    if (recipient_found) {
        // Relay the message to the recipient
        send_reply(recipient_socket, "PRIVMSG", username + " says: " + message);
    } else {
        // Send error back to sender if recipient not found
        send_reply(client_socket, ERR_NOSUCHNICK, recipient + " :No such user");
    }
}

void handle_topic_command(const string& command, int client_socket, const string& username){
    stringstream ss(command);
    string token, channel, topic;
    ss >> token >> channel;
    getline(ss, topic);

    if (channel.empty()) {
        send_reply(client_socket, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }

    if (!topic.empty() && topic[0] == ':') {
        topic.erase(0, 1); // Remove the leading ':' from the topic
    }

    if (topic.empty()) {
        send_reply(client_socket, "RPL_LIST", channel + " :" + channel_topics[channel]);
    } else {
        channel_topics[channel] = topic;
        send_reply(client_socket, "322", channel + " :Topic set to " + topic);
    }
}

void handle_names_command(const string& command, int client_socket, const string& username) {
    stringstream ss(command);
    string token, channel;
    ss >> token >> channel;

    if (channels.find(channel) == channels.end()) {
        send_reply(client_socket, ERR_NOSUCHNICK, "NAMES :No such channel");
        return;
    }

    // List all users in the specified channel
    string user_list;
    for (const auto& user : channels[channel]) {
        user_list += user + " ";
    }
    send_reply(client_socket, RPL_LIST, channel + " :" + user_list);
    send_reply(client_socket, RPL_LISTEND, channel + " :End of /NAMES list");
}

void handle_client_command(const string& command, int client_socket) {
    bool is_registered = user_sockets.find(client_socket) != user_sockets.end();

    // Allow NICK command for unregistered clients to initiate registration
    if (command.find("NICK") == 0) {
        handle_nick_command(command, client_socket);
        return;
    }

    // Ensure USER command can be processed if a client is not registered
    // Allow `USER` command for unregistered clients after `NICK`
    if (command.find("USER") == 0) {
        if (nickname_set_map[client_socket]) {  // Check if the NICK was set
            handle_user_command(command, client_socket);
        } else {
            send_reply(client_socket, "451", "You have not registered");  // ERR_NOTREGISTERED
        }
        return;
    }

    if (!is_registered) {
        cout << "Unregistered client tried to issue a command. Ignoring." << endl;
        send_reply(client_socket, "451", "You have not registered");  // ERR_NOTREGISTERED
        return;
    }

    const string& username = user_sockets[client_socket]; // Retrieve the username

    // Process other commands for registered clients
    if (command.find("MODE") == 0) {
        handle_mode_command(command, client_socket, username);
    } else if (command.find("JOIN") == 0) {
        handle_join_command(command, client_socket, username);
    } else if (command.find("PART") == 0) {
        handle_part_command(command, client_socket, username);
    } else if (command.find("PRIVMSG") == 0) {
        handle_privmsg_command(command, client_socket, username);
    } else if (command.find("TOPIC") == 0) {
        handle_topic_command(command, client_socket, username);
    } else if (command.find("NAMES") == 0) {
        handle_names_command(command, client_socket, username);
    } else if (command.find("QUIT") == 0) {
        handle_quit_command(client_socket, username);
    } else {
        send_reply(client_socket, "421", "Unknown command");
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
    ifstream conf(argv[1]);
    if (!conf.is_open()) {
        cerr << "Issue opening server.conf!" << endl;
        return 1;
    }

    string line;

    while (getline(conf, line)) {
        if (line.find("SERVER_PORT=") == 0) {
            port = line.substr(12);
        }
    }
    conf.close();

    if (port.empty()) {
        cerr << "Invalid server.conf!" << endl;
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed!");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed!");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(stoi(port));

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed!");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    cout << "Server -> Waiting for connections..." << endl;

    struct sigaction sa;
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    fd_set master_fds, read_fds;
int fdmax = server_fd;
FD_ZERO(&master_fds);
FD_ZERO(&read_fds);
FD_SET(server_fd, &master_fds);
set<int> active_clients; // Tracking active client sockets

struct sockaddr_in client_addr;
socklen_t addr_len = sizeof(client_addr);

while (true) {
    read_fds = master_fds;
    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
        perror("Select failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i <= fdmax; i++) {
        if (FD_ISSET(i, &read_fds)) {
            if (i == server_fd) {
                // Accept new client connections
                int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
                FD_SET(client_socket, &master_fds);
                if (client_socket > fdmax) fdmax = client_socket;
                active_clients.insert(client_socket);
            } else {
                // Handle client communication
                char buffer[1024] = {0};
                int valread = read(i, buffer, 1024);
                if (valread <= 0) {
                    close(i);
                    FD_CLR(i, &master_fds);
                    active_clients.erase(i);
                } else {
                    buffer[valread] = '\0';
                    handle_client_command(buffer, i);
                }
            }
        }
    }
}


    close(server_fd);
    return 0;
}
