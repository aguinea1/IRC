#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include "Client.hpp"

class Server {
private:
    int _listenFd;
    int _port;
    std::string _password;
    std::map<int, Client*> _clients;
    std::map<std::string, int> _nicks;

    void setupListen();
    void setNonBlocking(int fd);
    void acceptNewClient();
    void removeClient(int fd);

    struct Parsed {
        std::string cmd;
        std::vector<std::string> params;
    };
    Parsed parseLine(const std::string& line);
    void processLine(Client* c, const std::string& line);

    // comandos
    void cmdPASS(Client* c, const std::vector<std::string>& params);
    void cmdNICK(Client* c, const std::vector<std::string>& params);
    void cmdUSER(Client* c, const std::vector<std::string>& params);
    void cmdPRIVMSG(Client* c, const std::vector<std::string>& params);
    void cmdQUIT(Client* c, const std::vector<std::string>& params);

    // helpers
    void sendToClient(Client* c, const std::string& msg);
    void sendNumeric(Client* c, const std::string& code, const std::string& args);
    std::string serverPrefix() const;

public:
    Server(int port, const std::string& password);
    ~Server();
    void run(bool &running);
};

#endif
