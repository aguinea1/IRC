#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <vector>
#include <string>
#include "Client.hpp"
#include "Channel.hpp"

class Server {
private:
    int _listenFd;
    int _port;
    std::string _password;
    std::map<int, Client*> _clients;
    std::map<std::string, int> _nicks;
    std::map<std::string, Channel*> _channels;

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
    void cmdPRIVMSG(Client* c, const std::vector<std::string>& params, const std::string& originalLine);
    void cmdQUIT(Client* c, const std::vector<std::string>& params);
    void cmdJOIN(Client* c, const std::vector<std::string>& params);
    void cmdPART(Client* c, const std::vector<std::string>& params);
    void cmdTOPIC(Client* c, const std::vector<std::string>& params);
    void cmdNAMES(Client* c, const std::vector<std::string>& params);
    void cmdLIST(Client* c, const std::vector<std::string>& params);
    void cmdMODE(Client* c, const std::vector<std::string>& params);
    void cmdKICK(Client* c, const std::vector<std::string>& params);
    void cmdINVITE(Client* c, const std::vector<std::string>& params);
    void cmdDCC(Client* c, const std::vector<std::string>& params);

    // validaciones
    bool isValidNick(const std::string& nick);

    // helpers
    void sendToClient(Client* c, const std::string& msg);
    void sendNumeric(Client* c, const std::string& code, const std::string& args);
    std::string serverPrefix() const;
    void sendToClient(int fd, const std::string &msg);
    void sendNumeric(Client &client, int code, const std::string &message);
    void sendWelcome(Client &client);
    // gestión de canales
    Channel* getChannel(const std::string& name);
    Channel* createChannel(const std::string& name);
    void removeChannel(const std::string& name);
    void removeClientFromAllChannels(Client* c);
    void broadcastToChannel(Channel* channel, const std::string& message, const std::string& excludeNick = "");
    bool isValidChannelName(const std::string& name) const;
    bool processChannelModes(Channel* channel, Client* c, const std::string& modes, const std::string& params);
    
    // funciones del bot
    void botSendMessage(Client* target, const std::string& message);
    void botWelcomeUser(Client* newUser);
    void botProcessCommand(Client* sender, const std::string& command, const std::string& args);

public:
    Server(int port, const std::string& password);
    ~Server();
    void run(bool &running);
};

#endif
