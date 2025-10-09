#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <set>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>

#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "Client.hpp"

// Estructura de canal con estado IRC básico
struct Channel {
    std::string     name;
    std::string     topic;
    bool            inviteOnly;
    int             userLimit;    // -1 => sin límite
    std::string     key;          // "" => sin clave
    std::set<int>   ops;          // fds de operadores
    std::set<int>   members;      // fds de miembros
    std::set<int>   invited;      // fds invitados (si +i)

    Channel() : inviteOnly(false), userLimit(-1) {}
};

// Estado de registro por cliente
struct ClientState {
    bool pass_ok;
    bool has_nick;
    bool has_user;
    bool registered;
    ClientState() : pass_ok(false), has_nick(false), has_user(false), registered(false) {}
};

class Server {
public:
    Server(int port, const std::string& password);
    ~Server();

    void run(bool &runing);

private:
    int                          _listenFd;
    int                          _port;
    std::string                  _password;

    std::map<int, Client*>       _clients;     // fd -> Client*
    std::map<std::string, int>   _nicks;       // nick -> fd
    std::map<std::string, Channel> _channels;  // nombre -> Channel
    std::map<int, ClientState>   _cstate;      // fd -> estado registro

    // helpers de socket
    void setupListen();
    void setNonBlocking(int fd);
    void acceptNewClient();
    void removeClient(int fd);

    // I/O
    void handleClientReadable(int fd);
    void flushClientOutput(int fd);

    // protocolo
    struct Parsed {
        std::string cmd;
        std::vector<std::string> params;
    };
    Parsed      parseLine(const std::string& line);
    void        processLine(Client* c, const std::string& line);

    // comandos
    void cmdPASS(Client* c, const std::vector<std::string>& params);
    void cmdNICK(Client* c, const std::vector<std::string>& params);
    void cmdUSER(Client* c, const std::vector<std::string>& params);
    void cmdJOIN(Client* c, const std::vector<std::string>& params);
    void cmdPRIVMSG(Client* c, const std::vector<std::string>& params);
    void cmdQUIT(Client* c, const std::vector<std::string>& params);
    void cmdMODE(Client* c, const std::vector<std::string>& params);

    // auxiliares de protocolo
    bool        tryFinishRegistration(Client* c);
    void        sendToClient(Client* c, const std::string& msg);
    void        sendNumeric(Client* c, const std::string& code, const std::string& args);
    std::string serverPrefix() const;
    bool        isChannel(const std::string& s) const;

    // canal
    void joinChannel(const std::string& name, Client* c);
    void broadcastToChannel(const std::string& chan, const std::string& msg, Client* except);

    // gating
    bool requireRegistered(Client* c);
};

#endif // SERVER_HPP

