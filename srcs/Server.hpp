#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <ctime>
#include "Client.hpp"


class Client;

class Server {
private:
    int _port;
    std::string _password;
    int _listenSocket;

    std::map<int, Client*> _clients_fd;        // fd -> Client*
    std::map<std::string, Client*> _clients_nick; // nick -> Client*

public:
    Server(int port, const std::string &password);
    ~Server();

    void init();
    void run();
    void addClient(int fd);
    void removeClient(int fd);
};

#endif
