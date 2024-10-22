#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "p1_helper.h"

#define PORT "3490"
#define BACKLOG 10
#define SHM_KEY 12345
#define SEM_KEY 54321

// Helper function to convert string to lowercase
std::string to_lowercase(const std::string& str) {
    std::string result = str;
    for (auto &c : result) {
        c = tolower(c);
    }
    return result;
}

// semaphore structures for lock-unlock
struct sembuf sem_lock = {0, -1, 0};  // Lock semaphore
struct sembuf sem_unlock = {0, 1, 0}; // Unlock semaphore

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void client_handler(int client_fd, sockaddr_storage *their_addr, Book *books, int num_books, int sem_id)
{
    char buffer[1024];
    std::string client_msg;
    int numbytes;

    send(client_fd, "200 HELO - OK", 13, 0);

    while (true)
    {
        if ((numbytes = recv(client_fd, buffer, sizeof(buffer)-1, 0)) == -1) {
            perror("recv");
            close(client_fd);
            return;
        }

        buffer[numbytes] = '\0';
        client_msg = std::string(buffer);
        std::stringstream ss(client_msg);
        std::string command;
        ss >> command;

        if (command == "HELO") {
            send(client_fd, "200 HELO - OK", 13, 0);
        } else if (command == "HELP") {
            send(client_fd, "200 Available commands: HELO, SEARCH, MANAGE, CHECKOUT, RETURN, RECOMMEND, BYE !", 75, 0);
        } else if (command == "SEARCH") {
            send(client_fd, "210 Ready for search!!", 21, 0);
        } else if (command == "FIND") {
            std::string search_term;
            ss >> search_term;
            search_term = to_lowercase(search_term);  // FIX FOR CASE SENSITIVE ISSUE
            bool found = false;
            for (int i = 0; i < num_books; i++) {
                if (to_lowercase(books[i].title).find(search_term) != std::string::npos) {  // FIX FOR CASE SENSITIVE ISSUE #2
                    std::string response = "250 " + books[i].title + " by " + books[i].author + 
                        (books[i].available ? " [Available]" : " [Not Available]");
                    send(client_fd, response.c_str(), response.length(), 0);
                    found = true;
                    break;
                }
            }
            if (!found) {
                send(client_fd, "304 No books found matching the search term.", 44, 0);
            }
        } else if (command == "CHECKOUT") {
            std::string book_title;
            ss >> std::ws;
            std::getline(ss, book_title); // READ FOR TITLE
            book_title = to_lowercase(book_title);  // FIX CASE SENSITIVITY

            semop(sem_id, &sem_lock, 1);  // LOCKING SHARED MEMORY
            bool found = false;
            for (int i = 0; i < num_books; i++) {
                if (to_lowercase(books[i].title) == book_title && books[i].available) {
                    books[i].available = false;
                    std::string response = "250 Book checked out: " + books[i].title;
                    send(client_fd, response.c_str(), response.length(), 0);
                    found = true;
                    break;
                }
            }
            semop(sem_id, &sem_unlock, 1);  // UNLOCK SHARED MEMORY

            if (!found) {
                send(client_fd, "404 Book not available or not found.", 37, 0);
            }
        } else if (command == "RETURN") {
            std::string book_title;
            ss >> std::ws;
            std::getline(ss, book_title); 
            book_title = to_lowercase(book_title);  

            semop(sem_id, &sem_lock, 1);  // LOCK SHARED MEMORY
            bool found = false;
            for (int i = 0; i < num_books; i++) {
                if (to_lowercase(books[i].title) == book_title && !books[i].available) {
                    books[i].available = true;
                    std::string response = "250 Book returned: " + books[i].title;
                    send(client_fd, response.c_str(), response.length(), 0);
                    found = true;
                    break;
                }
            }
            semop(sem_id, &sem_unlock, 1);  // UNLOCK SHARED MEMORY

            if (!found) {
                send(client_fd, "404 Book not found or not checked out.", 38, 0);
            }
        } else if (command == "BYE") {
            send(client_fd, "200 Goodbye!", 12, 0);
            close(client_fd);
            return;
        } else {
            send(client_fd, "400 Bad Request", 15, 0);
        }
    }
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo, *p;
    int sockfd, new_fd;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;
    int rv;

    if (argc != 2) {
        fprintf(stderr, "usage: server server.conf\n");
        return 1;
    }

    // Setup shared memory
    int shm_id = shmget(SHM_KEY, sizeof(Book) * 100, IPC_CREAT | 0666);  
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }
    Book *books = (Book *)shmat(shm_id, NULL, 0);
    if (books == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    //  LOAD BOOKS INTO MEMORY
    std::vector<Book> temp_books = loadBooksFromFile("books.db");
    for (size_t i = 0; i < temp_books.size(); i++) {
        books[i] = temp_books[i];
    }
    int num_books = temp_books.size();

    // Setup semaphore
    int sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("semget");
        exit(1);
    }
    semctl(sem_id, 0, SETVAL, 1);  // INITIALIZE SEMAPH to 1

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    //SERVER CONNECTIONS HERE... 9/20/24
    printf("server: waiting for connections...\n");

    while (true) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) {
            close(sockfd);
            client_handler(new_fd, &their_addr, books, num_books, sem_id);
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }

    return 0;
}
