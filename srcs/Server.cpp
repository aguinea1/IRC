#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
#include <stdio.h>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>

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

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
    _channels.clear();

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
    if (!c->getNick().empty()) {
        _nicks.erase(c->getNick());
        removeClientFromAllChannels(c);
    }

    std::cout << "[CLIENT] Disconnected fd: " << fd
              << " nick: " << (c->getNick().empty() ? "(none)" : c->getNick()) << std::endl;

    delete c;
    _clients.erase(it);
}

// -------------------- helpers --------------------

static std::string ensureCRLF(const std::string &s) {
    if (s.size() >= 2 && s[s.size()-2] == '\r' && s[s.size()-1] == '\n')
        return s;
    return s + "\r\n";
}

std::string Server::serverPrefix() const {
    return ":irc.local";
}

void Server::sendToClient(Client* c, const std::string& msg) {
    if (!c) return;

    c->appendOutput(ensureCRLF(msg));
}

void Server::sendNumeric(Client* c, const std::string& code, const std::string& args) {
    std::string nick = c->getNick().empty() ? "*" : c->getNick();
    sendToClient(c, serverPrefix() + " " + code + " " + nick + " " + args);
}

// Overload declarado en el .hpp: formatea "NNN" y delega
void Server::sendNumeric(Client &client, int code, const std::string &message) {
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << code; // "001", "002", ...
    sendNumeric(&client, oss.str(), message);
}

void Server::sendWelcome(Client &client) {
    const std::string hostStr = serverPrefix().substr(1); // "irc.local"
    // nick!user@host
    std::string fullId = client.getNick();
    fullId += "!";
    fullId += client.getUsername().empty() ? "user" : client.getUsername();
    fullId += "@";
    fullId += client.getHost().empty() ? hostStr : client.getHost();

    time_t now = time(NULL);
    std::string date = ctime(&now) ? ctime(&now) : "";
    if (!date.empty() && date[date.size()-1] == '\n') date.erase(date.size()-1);

    sendNumeric(client, 1,  std::string(":Welcome to the Internet Relay Network ") + fullId);
    sendNumeric(client, 2,  std::string(":Your host is ") + hostStr + ", running version 1.0");
    sendNumeric(client, 3,  std::string(":This server was created ") + date);
    sendNumeric(client, 4,  hostStr + " 1.0 o o");
    sendNumeric(client, 375, ":- " + hostStr + " Message of the day -");
    sendNumeric(client, 372, ":- Welcome to " + hostStr + " — keep it simple, keep it working.");
    sendNumeric(client, 376, ":End of /MOTD command.");
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
            // IMPORTANTE: no escribimos antes de select (subject)
            // Activamos writefds para todos (o si tu Client tiene hasPendingOutput(), úsalo aquí)
            FD_SET(fd, &writefds);
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
    if (p.cmd == "PRIVMSG") { cmdPRIVMSG(c, p.params, line); return; }
    if (p.cmd == "QUIT") { cmdQUIT(c, p.params); return; }
    if (p.cmd == "JOIN") { cmdJOIN(c, p.params); return; }
    if (p.cmd == "PART") { cmdPART(c, p.params); return; }
    if (p.cmd == "TOPIC") { cmdTOPIC(c, p.params); return; }
    if (p.cmd == "NAMES") { cmdNAMES(c, p.params); return; }
    if (p.cmd == "LIST") { cmdLIST(c, p.params); return; }
    if (p.cmd == "MODE") { cmdMODE(c, p.params); return; }
    if (p.cmd == "KICK") { cmdKICK(c, p.params); return; }
    if (p.cmd == "INVITE") { cmdINVITE(c, p.params); return; }

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
        // Antes enviabas solo 001; ahora Welcome completo para irssi
        sendWelcome(*c);
        // Bot saluda al nuevo usuario
        botWelcomeUser(c);
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

    std::string realname = params[3];
    if (!realname.empty() && realname[0] == ':')
        realname.erase(0, 1);
    c->setRealname(realname);

    c->setHasUser(true);

    if (c->passOk() && c->hasNick() && c->hasUser() && !c->isRegistered()) {
        c->markRegistered();
        sendWelcome(*c);
        // Bot saluda al nuevo usuario
        botWelcomeUser(c);
    }
}

void Server::cmdPRIVMSG(Client* c, const std::vector<std::string>& params, const std::string& originalLine) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "411", ":No recipient given (PRIVMSG)");
        return;
    }
    if (params.size() < 2) {
        sendNumeric(c, "412", ":No text to send");
        return;
    }
    
    std::string target = params[0];
    
    // VALIDACIÓN ULTRA ESTRICTA: Analizar la línea original para canales
    if (target[0] == '#' || target[0] == '&') {
        // Buscar el patrón: PRIVMSG #canal :mensaje
        // Debe haber EXACTAMENTE un espacio entre el canal y los ":"
        std::string cleanLine = originalLine;
        // Limpiar \r\n
        if (!cleanLine.empty() && cleanLine[cleanLine.size()-1] == '\n') cleanLine.erase(cleanLine.size() - 1);
        if (!cleanLine.empty() && cleanLine[cleanLine.size()-1] == '\r') cleanLine.erase(cleanLine.size() - 1);
        
        // Buscar "PRIVMSG" (insensible a mayúsculas)
        std::string upperLine = cleanLine;
        for (size_t i = 0; i < upperLine.size(); ++i) {
            upperLine[i] = static_cast<char>(std::toupper(upperLine[i]));
        }
        
        size_t privmsgPos = upperLine.find("PRIVMSG");
        if (privmsgPos == std::string::npos) {
            sendNumeric(c, "412", ":No text to send");
            return;
        }
        
        // Buscar el inicio del canal después de PRIVMSG
        size_t pos = privmsgPos + 7; // "PRIVMSG" = 7 caracteres
        while (pos < cleanLine.size() && cleanLine[pos] == ' ') ++pos;
        
        // Ahora pos debe apuntar al inicio del canal
        if (pos >= cleanLine.size() || (cleanLine[pos] != '#' && cleanLine[pos] != '&')) {
            sendNumeric(c, "412", ":No text to send");
            return;
        }
        
        // Buscar el final del canal (primer espacio después del canal)
        while (pos < cleanLine.size() && cleanLine[pos] != ' ') ++pos;
        
        // Saltar espacios después del canal
        while (pos < cleanLine.size() && cleanLine[pos] == ' ') ++pos;
        
        // El siguiente carácter DEBE ser ':'
        if (pos >= cleanLine.size() || cleanLine[pos] != ':') {
            sendNumeric(c, "412", ":No text to send");
            return;
        }
    }
    
    // VALIDACIÓN ADICIONAL: Si hay más de 2 parámetros, formato incorrecto
    if (params.size() > 2) {
        sendNumeric(c, "412", ":No text to send");
        return;
    }
    
    std::string text = params[1];

    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = getChannel(target);
        if (!channel) {
            sendNumeric(c, "401", target + " :No such nick/channel");
            return;
        }
        if (!channel->hasUser(c->getNick())) {
            sendNumeric(c, "404", target + " :Cannot send to channel");
            return;
        }
        
        // PROTECCIÓN QUIT: Verificar que el cliente aún existe antes de enviar el mensaje
        if (_clients.find(c->getFd()) == _clients.end()) {
            // El cliente ha hecho QUIT, no enviar el mensaje
            return;
        }
        
        std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
        std::string msg = prefix + " PRIVMSG " + target + " :" + text;
        broadcastToChannel(channel, msg, c->getNick());
    } else {
        // Verificar si el mensaje es para el bot
        if (target == "ServerBot") {
            // PROTECCIÓN QUIT: Verificar que el cliente aún existe
            if (_clients.find(c->getFd()) == _clients.end()) {
                return;
            }
            
            // Procesar comando del bot
            botProcessCommand(c, text, "");
            return;
        }
        
        std::map<std::string, int>::iterator nt = _nicks.find(target);
        if (nt == _nicks.end()) {
            sendNumeric(c, "401", target + " :No such nick/channel");
            return;
        }

        // PROTECCIÓN QUIT: Verificar que el cliente aún existe antes de enviar el mensaje
        if (_clients.find(c->getFd()) == _clients.end()) {
            // El cliente ha hecho QUIT, no enviar el mensaje
            return;
        }

        std::map<int, Client*>::iterator dest = _clients.find(nt->second);
        if (dest != _clients.end()) {
            std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
            dest->second->appendOutput(ensureCRLF(prefix + " PRIVMSG " + target + " :" + text));
        }
    }
}

void Server::cmdQUIT(Client* c, const std::vector<std::string>&) {
    removeClient(c->getFd());
}

// -------------------- gestión de canales --------------------

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    return (it != _channels.end()) ? it->second : NULL;
}

Channel* Server::createChannel(const std::string& name) {
    Channel* channel = new Channel(name);
    _channels[name] = channel;
    return channel;
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        delete it->second;
        _channels.erase(it);
    }
}

void Server::removeClientFromAllChannels(Client* c) {
    std::string nickname = c->getNick();
    if (nickname.empty()) return;

    std::vector<std::string> channelsToRemove;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel* channel = it->second;
        if (channel->hasUser(nickname)) {
            channel->removeUser(nickname);
            std::string prefix = ":" + nickname + "!" + c->getUsername() + "@" + c->getHost();
            broadcastToChannel(channel, prefix + " QUIT :Client disconnected");
            
            if (channel->isEmpty()) {
                channelsToRemove.push_back(it->first);
            }
        }
    }
    
    for (size_t i = 0; i < channelsToRemove.size(); ++i) {
        removeChannel(channelsToRemove[i]);
    }
}

void Server::broadcastToChannel(Channel* channel, const std::string& message, const std::string& excludeNick) {
    if (!channel) return;
    
    const std::set<std::string>& users = channel->getUsers();
    for (std::set<std::string>::const_iterator it = users.begin(); it != users.end(); ++it) {
        if (*it != excludeNick) {
            std::map<std::string, int>::iterator nickIt = _nicks.find(*it);
            if (nickIt != _nicks.end()) {
                std::map<int, Client*>::iterator clientIt = _clients.find(nickIt->second);
                if (clientIt != _clients.end()) {
                    sendToClient(clientIt->second, message);
                }
            }
        }
    }
}

bool Server::isValidChannelName(const std::string& name) const {
    if (name.empty() || name.length() > 50) return false;
    if (name[0] != '#' && name[0] != '&') return false;
    
    for (size_t i = 1; i < name.length(); ++i) {
        char c = name[i];
        if (c == ' ' || c == '\a' || c == '\r' || c == '\n' || c == ',') {
            return false;
        }
    }
    return true;
}

// -------------------- comandos de canales --------------------

void Server::cmdJOIN(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "461", "JOIN :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string key = (params.size() > 1) ? params[1] : "";
    
    if (!isValidChannelName(channelName)) {
        sendNumeric(c, "403", channelName + " :No such channel");
        return;
    }
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        channel = createChannel(channelName);
    }
    
    if (!channel->canJoin(c->getNick(), key)) {
        if (channel->isInviteOnly() && !channel->isInvited(c->getNick())) {
            sendNumeric(c, "473", channelName + " :Cannot join channel (+i)");
        } else if (channel->isKeyProtected() && key != channel->getKey()) {
            sendNumeric(c, "475", channelName + " :Cannot join channel (+k)");
        } else if (channel->isUserLimitSet() && channel->getUserCount() >= static_cast<size_t>(channel->getUserLimit())) {
            sendNumeric(c, "471", channelName + " :Cannot join channel (+l)");
        } else {
            sendNumeric(c, "403", channelName + " :No such channel");
        }
        return;
    }
    
    channel->addUser(c->getNick());
    if (channel->getUserCount() == 1) {
        channel->addOperator(c->getNick());
    }
    
    std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
    std::string joinMsg = prefix + " JOIN :" + channelName;
    
    sendToClient(c, joinMsg);
    broadcastToChannel(channel, joinMsg, c->getNick());
    
    if (!channel->getTopic().empty()) {
        sendNumeric(c, "332", channelName + " :" + channel->getTopic());
    }
    
    sendNumeric(c, "353", "= " + channelName + " :" + channel->getUserList());
    sendNumeric(c, "366", channelName + " :End of /NAMES list");
}

void Server::cmdPART(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "461", "PART :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string reason = (params.size() > 1) ? params[1] : "Leaving";
    
    Channel* channel = getChannel(channelName);
    if (!channel || !channel->hasUser(c->getNick())) {
        sendNumeric(c, "442", channelName + " :You're not on that channel");
        return;
    }
    
    std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
    std::string partMsg = prefix + " PART " + channelName + " :" + reason;
    
    sendToClient(c, partMsg);
    broadcastToChannel(channel, partMsg, c->getNick());
    
    channel->removeUser(c->getNick());
    if (channel->isEmpty()) {
        removeChannel(channelName);
    }
}

void Server::cmdTOPIC(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "461", "TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    Channel* channel = getChannel(channelName);
    
    if (!channel) {
        sendNumeric(c, "403", channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasUser(c->getNick())) {
        sendNumeric(c, "442", channelName + " :You're not on that channel");
        return;
    }
    
    if (params.size() == 1) {
        if (channel->getTopic().empty()) {
            sendNumeric(c, "331", channelName + " :No topic is set");
        } else {
            sendNumeric(c, "332", channelName + " :" + channel->getTopic());
        }
    } else {
        if (channel->isTopicProtected() && !channel->isOperator(c->getNick())) {
            sendNumeric(c, "482", channelName + " :You're not channel operator");
            return;
        }
        
        std::string newTopic = params[1];
        channel->setTopic(newTopic);
        
        std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
        std::string topicMsg = prefix + " TOPIC " + channelName + " :" + newTopic;
        broadcastToChannel(channel, topicMsg);
    }
}

void Server::cmdNAMES(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
            Channel* channel = it->second;
            if (!channel->isSecret() || channel->hasUser(c->getNick())) {
                sendNumeric(c, "353", "= " + channel->getName() + " :" + channel->getUserList());
            }
        }
        sendNumeric(c, "366", "* :End of /NAMES list");
    } else {
        std::string channelName = params[0];
        Channel* channel = getChannel(channelName);
        
        if (!channel) {
            sendNumeric(c, "366", channelName + " :End of /NAMES list");
            return;
        }
        
        if (channel->isSecret() && !channel->hasUser(c->getNick())) {
            sendNumeric(c, "366", channelName + " :End of /NAMES list");
            return;
        }
        
        sendNumeric(c, "353", "= " + channelName + " :" + channel->getUserList());
        sendNumeric(c, "366", channelName + " :End of /NAMES list");
    }
}

void Server::cmdLIST(Client* c, const std::vector<std::string>&) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    
    sendNumeric(c, "321", "Channel :Users  Name");
    
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel* channel = it->second;
        if (!channel->isSecret() || channel->hasUser(c->getNick())) {
            std::ostringstream oss;
            oss << channel->getUserCount();
            std::string topic = channel->getTopic();
            if (topic.empty()) topic = "No topic set";
            sendNumeric(c, "322", channel->getName() + " " + oss.str() + " :" + topic);
        }
    }
    
    sendNumeric(c, "323", ":End of /LIST");
}

void Server::cmdMODE(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "461", "MODE :Not enough parameters");
        return;
    }
    
    std::string target = params[0];
    
    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = getChannel(target);
        if (!channel) {
            sendNumeric(c, "403", target + " :No such channel");
            return;
        }
        
        if (params.size() == 1) {
            sendNumeric(c, "324", target + " " + channel->getModeString());
        } else {
            if (!channel->isOperator(c->getNick())) {
                sendNumeric(c, "482", target + " :You're not channel operator");
                return;
            }
            
            std::string modes = params[1];
            std::string modeParams = (params.size() > 2) ? params[2] : "";
            
            if (!processChannelModes(channel, c, modes, modeParams)) {
                return;
            }
            
            std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
            std::string modeMsg = prefix + " MODE " + target + " " + modes;
            if (!modeParams.empty()) {
                modeMsg += " " + modeParams;
            }
            broadcastToChannel(channel, modeMsg);
        }
    } else {
        sendNumeric(c, "502", ":Cannot change mode for other users");
    }
}

// -------------------- comandos de operadores --------------------

void Server::cmdKICK(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.size() < 2) {
        sendNumeric(c, "461", "KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string targetNick = params[1];
    std::string reason = (params.size() > 2) ? params[2] : c->getNick();
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        sendNumeric(c, "403", channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasUser(c->getNick())) {
        sendNumeric(c, "442", channelName + " :You're not on that channel");
        return;
    }
    
    if (!channel->isOperator(c->getNick())) {
        sendNumeric(c, "482", channelName + " :You're not channel operator");
        return;
    }
    
    if (!channel->hasUser(targetNick)) {
        sendNumeric(c, "441", targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }
    
    if (c->getNick() == targetNick) {
        sendNumeric(c, "482", channelName + " :You cannot kick yourself");
        return;
    }
    
    std::map<std::string, int>::iterator nickIt = _nicks.find(targetNick);
    if (nickIt == _nicks.end()) {
        sendNumeric(c, "401", targetNick + " :No such nick");
        return;
    }
    
    std::map<int, Client*>::iterator targetClientIt = _clients.find(nickIt->second);
    if (targetClientIt == _clients.end()) {
        sendNumeric(c, "401", targetNick + " :No such nick");
        return;
    }
    
    std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
    std::string kickMsg = prefix + " KICK " + channelName + " " + targetNick + " :" + reason;
    
    broadcastToChannel(channel, kickMsg);
    channel->removeUser(targetNick);
    if (channel->isEmpty()) {
        removeChannel(channelName);
    }
    
    std::cout << "[KICK] " << c->getNick() << " kicked " << targetNick 
              << " from " << channelName << " (reason: " << reason << ")" << std::endl;
}

void Server::cmdINVITE(Client* c, const std::vector<std::string>& params) {
    if (!c->isRegistered()) {
        sendNumeric(c, "451", ":You have not registered");
        return;
    }
    if (params.size() < 2) {
        sendNumeric(c, "461", "INVITE :Not enough parameters");
        return;
    }
    
    std::string targetNick = params[0];
    std::string channelName = params[1];
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        sendNumeric(c, "403", channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasUser(c->getNick())) {
        sendNumeric(c, "442", channelName + " :You're not on that channel");
        return;
    }
    
    if (!channel->isOperator(c->getNick())) {
        sendNumeric(c, "482", channelName + " :You're not channel operator");
        return;
    }
    
    std::map<std::string, int>::iterator nickIt = _nicks.find(targetNick);
    if (nickIt == _nicks.end()) {
        sendNumeric(c, "401", targetNick + " :No such nick");
        return;
    }
    
    std::map<int, Client*>::iterator targetClientIt = _clients.find(nickIt->second);
    if (targetClientIt == _clients.end()) {
        sendNumeric(c, "401", targetNick + " :No such nick");
        return;
    }
    
    Client* targetClient = targetClientIt->second;
    
    if (channel->hasUser(targetNick)) {
        sendNumeric(c, "443", targetNick + " " + channelName + " :is already on channel");
        return;
    }
    
    channel->addInvited(targetNick);
    
    std::string prefix = ":" + c->getNick() + "!" + c->getUsername() + "@" + c->getHost();
    std::string inviteMsg = prefix + " INVITE " + targetNick + " :" + channelName;
    sendToClient(targetClient, inviteMsg);
    
    sendNumeric(c, "341", targetNick + " " + channelName);
    
    std::cout << "[INVITE] " << c->getNick() << " invited " << targetNick 
              << " to " << channelName << std::endl;
}

// -------------------- procesamiento de modos de canal --------------------

bool Server::processChannelModes(Channel* channel, Client* c, const std::string& modes, const std::string& params) {
    if (modes.empty()) return true;
    
    bool adding = true;
    size_t paramIndex = 0;
    std::vector<std::string> paramList;
    
    if (!params.empty()) {
        std::stringstream ss(params);
        std::string param;
        while (ss >> param) {
            paramList.push_back(param);
        }
    }
    
    for (size_t i = 0; i < modes.length(); ++i) {
        char mode = modes[i];
        
        if (mode == '+') { adding = true; continue; }
        if (mode == '-') { adding = false; continue; }
        
        switch (mode) {
            case 'i':
                channel->setInviteOnly(adding);
                break;
            case 't':
                channel->setTopicProtected(adding);
                break;
            case 'k':
                if (adding) {
                    if (paramIndex >= paramList.size()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return false; }
                    channel->setKey(paramList[paramIndex++]);
                    channel->setKeyProtected(true);
                } else {
                    channel->setKey("");
                    channel->setKeyProtected(false);
                }
                break;
            case 'o': {
                if (paramIndex >= paramList.size()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return false; }
                std::string targetNick = paramList[paramIndex++];
                if (adding) {
                    if (channel->hasUser(targetNick)) channel->addOperator(targetNick);
                    else { sendNumeric(c, "441", targetNick + " " + channel->getName() + " :They aren't on that channel"); return false; }
                } else {
                    channel->removeOperator(targetNick);
                }
                break;
            }
            case 'l':
                if (adding) {
                    if (paramIndex >= paramList.size()) { sendNumeric(c, "461", "MODE :Not enough parameters"); return false; }
                    int limit = atoi(paramList[paramIndex++].c_str());
                    if (limit <= 0) { sendNumeric(c, "461", "MODE :Invalid limit"); return false; }
                    channel->setUserLimit(limit);
                    channel->setUserLimitSet(true);
                } else {
                    channel->setUserLimit(0);
                    channel->setUserLimitSet(false);
                }
                break;
            default:
                sendNumeric(c, "472", std::string(1, mode) + " :is unknown mode char to me");
                return false;
        }
    }
    
    return true;
}

// -------------------- funciones del bot --------------------

void Server::botSendMessage(Client* target, const std::string& message) {
    if (!target) return;
    
    std::string botPrefix = ":ServerBot!bot@" + serverPrefix().substr(1);
    std::string botMsg = botPrefix + " PRIVMSG " + target->getNick() + " :" + message;
    
    target->appendOutput(ensureCRLF(botMsg));
}

void Server::botWelcomeUser(Client* newUser) {
    if (!newUser || !newUser->isRegistered()) return;
    
    // Mensaje de bienvenida del bot
    botSendMessage(newUser, "¡Hola " + newUser->getNick() + "! 👋 Bienvenido al servidor IRC.");
    botSendMessage(newUser, "Soy ServerBot, tu asistente virtual. Escribe 'PRIVMSG ServerBot :help' para ver los comandos disponibles.");
    botSendMessage(newUser, "¡Que disfrutes tu estancia en el servidor! 🚀");
}

void Server::botProcessCommand(Client* sender, const std::string& command, const std::string& args) {
    if (!sender) return;
    
    (void)args; // Evitar warning de parámetro no usado
    
    std::string cmd = command;
    // Convertir a minúsculas para comparación
    for (size_t i = 0; i < cmd.size(); ++i) {
        cmd[i] = static_cast<char>(std::tolower(cmd[i]));
    }
    
    if (cmd == "help" || cmd == "ayuda") {
        botSendMessage(sender, " Comandos disponibles del ServerBot:");
        botSendMessage(sender, "• help/ayuda - Muestra esta ayuda");
        botSendMessage(sender, "• info - Información del servidor");
        botSendMessage(sender, "• time/hora - Hora actual del servidor");
        botSendMessage(sender, "• users/usuarios - Número de usuarios conectados");
        botSendMessage(sender, "• channels/canales - Número de canales activos");
    }
    else if (cmd == "info") {
        botSendMessage(sender, "  Información del servidor:");
        botSendMessage(sender, "• Nombre: " + serverPrefix().substr(1));
        botSendMessage(sender, "• Versión: IRC Server 1.0");
        botSendMessage(sender, "• Funciones: Canales, modos, mensajes privados");
    }
    else if (cmd == "time" || cmd == "hora") {
        time_t now = time(NULL);
        std::string timeStr = ctime(&now) ? ctime(&now) : "Hora no disponible";
        if (!timeStr.empty() && timeStr[timeStr.size()-1] == '\n') timeStr.erase(timeStr.size()-1);
        botSendMessage(sender, " Hora del servidor: " + timeStr);
    }
    else if (cmd == "users" || cmd == "usuarios") {
        std::ostringstream oss;
        oss << _clients.size();
        botSendMessage(sender, " Usuarios conectados: " + oss.str());
    }
    else if (cmd == "channels" || cmd == "canales") {
        std::ostringstream oss;
        oss << _channels.size();
        botSendMessage(sender, " Canales activos: " + oss.str());
    }
    else {
        botSendMessage(sender, " Comando no reconocido. Escribe 'help' para ver los comandos disponibles.");
    }
}
