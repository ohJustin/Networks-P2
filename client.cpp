#include <iostream>
#include <arpa/inet.h> // Networking functions (AF_INET, SOCK_STREAM, inet_pton)
#include <unistd.h> // close() -> close socket
#include <cstring> // String functions (strlen)...
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MAX_DATASIZE 100 // Buffer size limit

// Extract IP from sockaddr structure, no matter the family(IPv4 etc)
void* get_in_addr(struct sockaddr* sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr); // IPv4
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr); // IPv6
}

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: Client <config_file>" << endl;
        return 1;
    }

    string serverIP, serverPort;
    cout << "Opening .conf file for client" << endl;
    ifstream conf(argv[1]);
    if (!conf.is_open()) {
        cerr << "Issue opening client.conf!" << endl;
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

    if (serverIP.empty() || serverPort.empty()) {
        cerr << "Invalid client.conf!" << endl;
        return 1;
    }

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    cout << "Parsed Server IP: " << serverIP << " Port: " << serverPort << endl;
    int rv = getaddrinfo(serverIP.c_str(), serverPort.c_str(), &hints, &servinfo);
    if (rv != 0) {
        cerr << "getaddrinfo -> " << gai_strerror(rv) << endl;
        return 1;
    }

    int sockfd;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Client issue: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Client issue: connection");
            continue;
        }

        break;
    }

    if (p == NULL) {
        cerr << "Issue connecting client!" << endl;
        return 2;
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
    cout << "Client -> connecting to: " << s << endl;

    
    freeaddrinfo(servinfo);

    fd_set master_fds, read_fds;
    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &master_fds);
    FD_SET(STDIN_FILENO, &master_fds); // Add standard input (user input)

    int fdmax = sockfd;
    char buff[MAX_DATASIZE];
    string msg;

    while (true) {
        read_fds = master_fds;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select failed");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            cout << "> ";
            cout.flush();
            getline(cin, msg);
            if (msg == "exit") {
                break;
            }
            if (send(sockfd, msg.c_str(), msg.size(), 0) == -1) {
                perror("Send failed");
                break;
            }
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            int numbytes = recv(sockfd, buff, MAX_DATASIZE - 1, 0);
            if (numbytes == -1) {
                perror("recv issue");
                break;
            } else if (numbytes == 0) {
                cout << "Server closed the connection!" << endl;
                break;
            }

            buff[numbytes] = '\0';

                        // Check for heartbeat message
            if (strcmp(buff, "..HEARTBEAT..\n") == 0) {
                cout << "\nReceived heartbeat from server" << endl << "> " << flush;
            } else {
                cout << "\nServer: " << buff << endl << "> " << flush; // Prompt updated for immediate display
            }

        // cout << "\nServer: " << buff << endl << "> " << flush; // Prompt updated for immediate display
        }
    }

    close(sockfd);
    return 0;
}
