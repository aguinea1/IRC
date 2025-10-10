#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
#include <stdio.h>

// -------------------- constructor / destructor --------------------

Server::Server(int port, const std::string& password)
: _listenFd(-1), _port(port), _password(password) {
    std::cout << "[SERVER] Starting server..." << std::endl;
    setupListen();
}

Server::~Server() {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
    _clients.clear();

    if (_listenFd >= 0) {
        ::close(_listenFd);
        _listenFd = -1;
    }
    std::cout << "[SERVER] Shutdown complete." << std::endl;
}

// -------------------- configuración --------------------

void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Server::setupListen() {
    _listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(_port));

    if (::bind(_listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(_listenFd);
        throw std::runtime_error("bind() failed");
    }
    if (::listen(_listenFd, 128) < 0) {
        ::close(_listenFd);
        throw std::runtime_error("listen() failed");
    }

    setNonBlocking(_listenFd);
    std::cout << "[SERVER] Listening on port " << _port << std::endl;
}

// -------------------- clientes --------------------

void Server::acceptNewClient() {
    sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    int cfd = ::accept(_listenFd, (sockaddr*)&cliaddr, &len);
    if (cfd < 0) return;

    setNonBlocking(cfd);
    char buf[64];
    std::memset(buf, 0, sizeof(buf));
    const char* ip = inet_ntop(AF_INET, &cliaddr.sin_addr, buf, sizeof(buf) - 1);
    std::string host = ip ? std::string(ip) : "unknown";

    Client* c = new Client(cfd, host);
    _clients[cfd] = c;

    std::cout << "[CLIENT] Connected from " << host << " (fd " << cfd << ")" << std::endl;
    std::cout << "[USAGE]\n   1. PASS <password>\n   2. NICK <nick>\n   3. USER <user> <h> <s> :<name>\n" << std::endl;
}

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;

    Client* c = it->second;
    if (!c->getNick().empty())
        _nicks.erase(c->getNick());

    std::cout << "[CLIENT] Disconnected fd: " << fd
              << " nick: " << (c->getNick().empty() ? "(none)" : c->getNick()) << std::endl;

    delete c;
    _clients.erase(it);
}

// -------------------- helpers --------------------

std::string Server::serverPrefix() const {
    return ":irc.local";
}

void Server::sendToClient(Client* c, const std::string& msg) {
    if (!c) return;
    c->appendOutput(msg);
}

void Server::sendNumeric(Client* c, const std::string& code, const std::string& args) {
    std::string nick = c->getNick().empty() ? "*" : c->getNick();
    sendToClient(c, serverPrefix() + " " + code + " " + nick + " " + args);
}

// -------------------- parsing --------------------

Server::Parsed Server::parseLine(const std::string& lineIn) {
    Parsed p;
    std::string s = lineIn;
    if (!s.empty() && *--s.end() == '\n') s.erase(s.size() - 1);
    if (!s.empty() && *--s.end() == '\r') s.erase(s.size() - 1);

    std::string::size_type pos = 0;
    while (pos < s.size() && s[pos] == ' ') ++pos;
    std::string::size_type start = pos;
    while (pos < s.size() && s[pos] != ' ') ++pos;
    p.cmd = s.substr(start, pos - start);
    for (size_t i = 0; i < p.cmd.size(); ++i)
        p.cmd[i] = static_cast<char>(std::toupper(p.cmd[i]));

    while (pos < s.size()) {
        while (pos < s.size() && s[pos] == ' ') ++pos;
        if (pos >= s.size()) break;
        if (s[pos] == ':') {
            p.params.push_back(s.substr(pos + 1));
            break;
        }
        start = pos;
        while (pos < s.size() && s[pos] != ' ') ++pos;
        p.params.push_back(s.substr(start, pos - start));
    }
    return p;
}

// -------------------- bucle principal --------------------

void Server::run(bool &running) {
    while (running) {
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        int maxfd = _listenFd;
        FD_SET(_listenFd, &readfds);

        for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            int fd = it->first;
            FD_SET(fd, &readfds);
            if (!it->second->writeToSocket()) removeClient(fd);
            if (fd > maxfd) maxfd = fd;
        }

        int ret = ::select(maxfd + 1, &readfds, &writefds, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(_listenFd, &readfds))
            acceptNewClient();

        std::map<int, Client*>::iterator it = _clients.begin();
        while (it != _clients.end()) {
            std::map<int, Client*>::iterator cur = it++;
            Client* c = cur->second;
            int fd = c->getFd();

            if (FD_ISSET(fd, &readfds)) {
                if (!c->readFromSocket()) {
                    removeClient(fd);
                    continue;
                }
                while (c->hasCompleteLine()) {
                    std::string line = c->extractLine();
                    processLine(c, line);
                    if (_clients.find(fd) == _clients.end()) break;
                }
            }

            if (_clients.find(fd) != _clients.end() && FD_ISSET(fd, &writefds)) {
                if (!c->writeToSocket())
                    removeClient(fd);
            }
        }
    }

    std::cout << "[SERVER] Shutting down..." << std::endl;
    std::vector<int> fds;
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        fds.push_back(it->first);
    for (size_t i = 0; i < fds.size(); ++i)
        removeClient(fds[i]);
}

// -------------------- comandos --------------------

void Server::processLine(Client* c, const std::string& line) {
    Parsed p = parseLine(line);
    if (p.cmd.empty()) return;

    if (p.cmd == "PASS") { cmdPASS(c, p.params); return; }
    if (p.cmd == "NICK") { cmdNICK(c, p.params); return; }
    if (p.cmd == "USER") { cmdUSER(c, p.params); return; }
    if (p.cmd == "PRIVMSG") { cmdPRIVMSG(c, p.params); return; }
    if (p.cmd == "QUIT") { cmdQUIT(c, p.params); return; }

    sendNumeric(c, "421", p.cmd + " :Unknown command");
}

// -------------------- implementación de comandos --------------------

void Server::cmdPASS(Client* c, const std::vector<std::string>& params) {
    if (c->isRegistered()) {
        sendNumeric(c, "462", ":You may not reregister");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "461", "PASS :Not enough parameters");
        return;
    }
    if (params[0] == _password) {
        c->markPassOk();
    } else {
        sendNumeric(c, "464", ":Password incorrect");
    }
}

void Server::cmdNICK(Client* c, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendNumeric(c, "431", ":No nickname given");
        return;
    }
    std::string newnick = params[0];
    if (_nicks.count(newnick) && _nicks[newnick] != c->getFd()) {
        sendNumeric(c, "433", newnick + " :Nickname is already in use");
        return;
    }
    if (!c->getNick().empty())
        _nicks.erase(c->getNick());

    c->setNick(newnick);
    _nicks[newnick] = c->getFd();

    if (c->passOk() && c->hasNick() && c->hasUser() && !c->isRegistered()) {
        c->markRegistered();
        sendNumeric(c, "001", ":Welcome to the IRC server " + c->getNick());
    }
}

void Server::cmdUSER(Client* c, const std::vector<std::string>& params) {
    if (params.size() < 4) {
        sendNumeric(c, "461", "USER :Not enough parameters");
        return;
    }
    if (c->hasUser()) {
        sendNumeric(c, "462", ":You may not reregister");
        return;
    }

    c->setUsername(params[0]);
    c->setRealname(params[3]);

    if (c->passOk() && c->hasNick() && c->hasUser() && !c->isRegistered()) {
        c->markRegistered();
        sendNumeric(c, "001", ":Welcome to the IRC server " + c->getNick());
    }
}

void Server::cmdPRIVMSG(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.size() < 2) {
        sendNumeric(c, "411", ":No recipient given (PRIVMSG)");
        return;
    }
    std::string target = params[0];
    std::string text   = params[1];

    std::map<std::string, int>::iterator nt = _nicks.find(target);
    if (nt == _nicks.end()) {
        sendNumeric(c, "401", target + " :No such nick");
        return;
    }

    std::map<int, Client*>::iterator dest = _clients.find(nt->second);
    if (dest != _clients.end()) {
        std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
        dest->second->appendOutput(prefix + " PRIVMSG " + target + " :" + text);
    }
}

void Server::cmdQUIT(Client* c, const std::vector<std::string>&) {
    removeClient(c->getFd());
}