#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <ctime>
#include "Client.hpp"

class Client;

class Server {
private:
    int _port;
    std::string _password;
    int _listenSocket;

    std::map<int, Client*> _clients_fd;          // fd -> Client*
    std::map<std::string, Client*> _clients_nick; // nick -> Client*
    std::map<std::string, std::vector<Client*> > _channels; // channel -> clients

public:
    Server(int port, const std::string &password);
    ~Server();

    void init();
    void run();
    void addClient(int fd);
    void removeClient(int fd);

private:
    void setNonBlocking(int fd);
    void handleClientReadable(int fd);
    void processLine(Client *client, const std::string &line);
    void sendToClient(Client *client, const std::string &msg);
    void broadcastToChannel(const std::string &channel, const std::string &msg, Client *except = NULL);
    void joinChannel(const std::string &channel, Client *client);
    void partChannel(const std::string &channel, Client *client);
};

#endif
