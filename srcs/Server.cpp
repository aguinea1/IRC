#include "Server.hpp"
#include <iostream>

//protocolo IRC:
//Conexion TCP
// cliente--servidor: el cliente tiene q abrir un socket, se conecta al servidor(puerto e IP), y el servidor lo guarda en lista de clientes(_clients)
// a partir de ahi el cliente ya puede usar comandos del protocolo IRC(JOIN, PART etc...)
// cada cliente cuenta con su socket y dos buffers(datos, del cliente, datos q el server le quiere enviar)
//formato irc:[:prefix] COMMAND [param1] [param2] ... [:último parámetro con espacios], (:juan!j@localhost PRIVMSG #chat :Hola a todos)
// -------------------- utilidades de socket --------------------

Server::Server(int port, const std::string& password)
: _listenFd(-1), _port(port), _password(password) {
    std::cout <<"Abriendo server"<< std::endl;
    setupListen();
}

Server::~Server() {
    // cierra clientes
    std::map<int, Client*>::iterator it = _clients.begin();
    for (; it != _clients.end(); ++it) {
        delete it->second;
    }
    _clients.clear();
    if (_listenFd >= 0) {
        ::close(_listenFd);
        _listenFd = -1;
    }
}
//Non-blcking = para que no se quede esperando el socket del primer cliente y atienda a los de despues(sino se queda bloqueando esperando al primero)
void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    std::cout<<"[SERVER] Non-Blocking done"<< std::endl;
}

void Server::setupListen() {
    _listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0) throw std::runtime_error("socket() failed");

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
    //std::cout <<"setup Listen hecho "<< std::endl;
    setNonBlocking(_listenFd);
}

void Server::acceptNewClient() {//implementar connect()
    sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);
    int cfd = ::accept(_listenFd, (sockaddr*)&cliaddr, &len);//creacion de socket para el nuevo cliente(cfd = canal de comunicacion)
    if (cfd < 0) return;
    setNonBlocking(cfd);

    char buf[64];
    std::memset(buf, 0, sizeof(buf));
    const char* ip = inet_ntop(AF_INET, &cliaddr.sin_addr, buf, sizeof(buf)-1);
    std::string host = ip ? std::string(ip) : std::string("localhost");//ip binaria en legible (1010101001 a 127.0.0.1 o localhost)

    Client* c = new Client(cfd, host);
    _clients[cfd] = c;
    _cstate[cfd] = ClientState();
    // setear booleanos de clientstate(all false)
    std::cout <<"[CLIENT] client accepted"<<std::endl; 
    std::cout <<" [USAGE for CLIENTS]\n 1. PASS <password>\n 2. NICK <nickname>\n 3. USER <username> <hostname> <servername> :<realname>"<< std::endl;
}

// -------------------- bucle principal --------------------

void Server::run(bool &running) {
    std::cout << "[SERVER] Listening on port... " << _port << std::endl;
    while (running) {
        fd_set readfds;
        fd_set writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        int maxfd = _listenFd;
        FD_SET(_listenFd, &readfds);

        // prepara sets para clientes
        std::map<int, Client*>::iterator it = _clients.begin();
        for (; it != _clients.end(); ++it) {
            std::cout<<"[SERVER] Looking clients data"<<std::endl;
            int fd = it->first;
            FD_SET(fd, &readfds); //para saber si el cliente tiene nuevos datos que leer(en el fd se ha escrito algo)
            if (!it->second->buffer_out.empty()) {//si esta vacio no hay que enviar nada mas al cliente
                FD_SET(fd, &writefds);
            }
            if (fd > maxfd) maxfd = fd;//setea el mas grande para luego
        }

        int ret = ::select(maxfd + 1, &readfds, &writefds, NULL, NULL);
        //ret >0 = fd listo
        if (ret < 0) {
            std::cout<<"[SERVER] invalid fd"<<std::endl;
            if (errno == EINTR) continue;
            std::perror("select");
            break;
        }

        if (FD_ISSET(_listenFd, &readfds)) {
            std::cout <<"[SERVER] new client"<<std::endl;
            acceptNewClient();
        }

        // gestionar clientes: cuidado con borrados durante el loop
        it = _clients.begin();
        while (it != _clients.end()) {
            std::map<int, Client*>::iterator cur = it++;
            int fd = cur->first;
            std::cout <<"[CLIENT] client fd: "<<fd<< std::endl;
            if (FD_ISSET(fd, &readfds)) {
                std::cout <<"[SERVER] data to be readen on socket"<<std::endl;
                handleClientReadable(fd);
            }
            // para escribir output del cliente
            if (_clients.find(fd) != _clients.end() && FD_ISSET(fd, &writefds)) {
                flushClientOutput(fd);
            }
        }
    }

    std::cout << "\n[SERVER] Turning off all conections..." << std::endl;
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
         _clients.clear();
    close(_listenFd);
    std::cout << "[SERVER] Turned off." << std::endl;
    }
}

// -------------------- lectura y escritura --------------------

void Server::handleClientReadable(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;

    char buf[4096];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);//recv: lee lo q haya en el socket
        if (n > 0) {//si hay datos
            c->buffer_in.append(buf, static_cast<size_t>(n));//getnexline
            if (c->buffer_in.size() > 65536) {//intento de overflow
                removeClient(fd);
                return;
            }
        } else if (n == 0) {//QUIT
            // peer closed
            removeClient(fd);
            return;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {//no hay mas datos ahora
                break;
            } else {
                removeClient(fd);
                return;
            }
        }
    }

    // procesar líneas completas por \r\n o \n
    while (true) {//getnextline (lyendo por lineas)
        std::string::size_type pos = c->buffer_in.find('\n');
        if (pos == std::string::npos) break;

        // extrae línea
        std::string line = c->buffer_in.substr(0, pos + 1);
        c->buffer_in.erase(0, pos + 1);

        processLine(c, line);
        if (_clients.find(fd) == _clients.end()) return; // pudo desconectar
    }
}

void Server::flushClientOutput(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;

    while (!c->buffer_out.empty()) {
        ssize_t n = ::send(fd, c->buffer_out.data(), c->buffer_out.size(), 0);
        if (n > 0) {
            c->buffer_out.erase(0, static_cast<size_t>(n));
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return; // ya enviaremos en el siguiente ciclo
            } else {
                removeClient(fd);
                return;
            }
        }
    }
}

// -------------------- alta/baja cliente --------------------

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end()) return;
    Client* c = it->second;

    // salir de canales
    std::map<std::string, Channel>::iterator ch = _channels.begin();
    for (; ch != _channels.end(); ++ch) {
        if (ch->second.members.count(fd)) {
            ch->second.members.erase(fd);
            ch->second.ops.erase(fd);
            ch->second.invited.erase(fd);
            // avisar salida (opcional)
            // no enviamos aquí; socket puede estar muriendo
        }
    }

    if (!c->nick.empty()) _nicks.erase(c->nick);
    _cstate.erase(fd);
   std::cout << "[CLIENT] client: " << (!c->nick.empty() ? c->nick : "unknown") << " is out." << std::endl;
    delete c;
    _clients.erase(it);
}

// -------------------- helpers de protocolo --------------------

Server::Parsed Server::parseLine(const std::string& lineIn) {
    Parsed p;
    std::string s = lineIn;

    // quitar fin de línea
    if (!s.empty() && s[s.size()-1] == '\n') s.erase(s.size()-1);
    if (!s.empty() && s[s.size()-1] == '\r') s.erase(s.size()-1);

    // saltar espacios iniciales
    std::string::size_type pos = 0;
    while (pos < s.size() && s[pos] == ' ') ++pos;

    // comando
    std::string::size_type start = pos;
    while (pos < s.size() && s[pos] != ' ') ++pos;
    p.cmd = s.substr(start, pos - start);
    for (size_t i = 0; i < p.cmd.size(); ++i)
        p.cmd[i] = static_cast<char>(std::toupper(p.cmd[i]));

    // parámetros
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

std::string Server::serverPrefix() const {
    // Puedes poner hostname real si lo resuelves
    return ":" + std::string("irc.local");
}

void Server::sendToClient(Client* c, const std::string& msg) {
    if (!c) return;
    // asegura CRLF
    if (msg.size() >= 2 && msg[msg.size()-2] == '\r' && msg[msg.size()-1] == '\n')
        c->buffer_out += msg;
    else
        c->buffer_out += msg + "\r\n";
}

void Server::sendNumeric(Client* c, const std::string& code, const std::string& args) {
    std::string nick = c->nick.empty() ? "*" : c->nick;
    sendToClient(c, serverPrefix() + " " + code + " " + nick + " " + args);
}

bool Server::isChannel(const std::string& s) const {
    return !s.empty() && s[0] == '#';
}

bool Server::requireRegistered(Client* c) {
    ClientState &st = _cstate[c->fd];
    if (!st.registered) {
        sendNumeric(c, "451", ":You have not registered");
        return false;
    }
    return true;
}

bool Server::tryFinishRegistration(Client* c) {
    ClientState &st = _cstate[c->fd];
    if (!st.registered && st.pass_ok && st.has_nick && st.has_user) {
        st.registered = true;
        sendNumeric(c, "001", ":Welcome to the IRC server " + c->nick);
        return true;
    }
    return false;
}

// -------------------- dispatcher de comandos --------------------

void Server::processLine(Client* c, const std::string& line) {
    Parsed p = parseLine(line);
    std::cout << p.cmd<< std::endl;
    if (p.cmd.empty()) return;

    if (p.cmd == "PASS") { std::cout<<p.cmd<<std::endl; cmdPASS(c, p.params); return; }
    if (p.cmd == "NICK") { std::cout<<p.cmd<<std::endl; cmdNICK(c, p.params); return; }
    if (p.cmd == "USER") { std::cout<<p.cmd<<std::endl; cmdUSER(c, p.params); return; }
    if (p.cmd == "JOIN") { std::cout<<p.cmd<<std::endl; cmdJOIN(c, p.params); return; }
    if (p.cmd == "PRIVMSG") { std::cout<<p.cmd<<std::endl; cmdPRIVMSG(c, p.params); return; }
    if (p.cmd == "QUIT") { std::cout<<p.cmd<<std::endl; cmdQUIT(c, p.params); return; }
    if (p.cmd == "MODE") { std::cout<<p.cmd<<std::endl; cmdMODE(c, p.params); return; } 

    // Comando desconocido
    sendNumeric(c, "421", p.cmd + " :Unknown command");
}

// -------------------- implementación de comandos --------------------

void Server::cmdPASS(Client* c, const std::vector<std::string>& params) {
    ClientState &st = _cstate[c->fd];
    if (st.registered) {
        sendNumeric(c, "462", ":You may not reregister");
        return;
    }
    if (params.empty()) {
        sendNumeric(c, "461", "PASS :Not enough parameters");
        return;
    }
    if (params[0] == _password) {
        std::cout<<"correct pass"<<std::endl;
        st.pass_ok = true;
    } else {
        sendNumeric(c, "464", ":Password incorrect");
        // removeClient(c->fd);
    }
    tryFinishRegistration(c);
}

void Server::cmdNICK(Client* c, const std::vector<std::string>& params) {
    if (params.empty() || params[0].empty()) {
        sendNumeric(c, "431", ":No nickname given");
        return;
    }
    std::string newnick = params[0];
    if (_nicks.count(newnick) && _nicks[newnick] != c->fd) {
        sendNumeric(c, "433", newnick + " :Nickname is already in use");
        return;
    }

    // actualizar mapa de nicks
    if (!c->nick.empty()) _nicks.erase(c->nick);
    c->nick = newnick;
    _nicks[newnick] = c->fd;

    _cstate[c->fd].has_nick = true;
    tryFinishRegistration(c);
}

void Server::cmdUSER(Client* c, const std::vector<std::string>& params) {
    ClientState &st = _cstate[c->fd];
    if (st.has_user) {
        sendNumeric(c, "462", ":You may not reregister");
        return;
    }
    if (params.size() < 4) {
        sendNumeric(c, "461", "USER :Not enough parameters");
        return;
    }
    c->username = params[0];
    c->realname = params[3]; // trailing
    st.has_user = true;
    tryFinishRegistration(c);
}

void Server::joinChannel(const std::string& name, Client* c) {
    std::string chan = name;
    if (!isChannel(chan)) chan = "#" + chan;

    Channel &ch = _channels[chan];
    ch.name = chan;

    // +i: sólo invitados u operators
    if (ch.inviteOnly && ch.invited.count(c->fd) == 0 && ch.ops.count(c->fd) == 0) {
        sendNumeric(c, "473", chan + " :Cannot join channel (+i)");
        return;
    }

    // límite (si implementas +l)
    if (ch.userLimit >= 0 && static_cast<int>(ch.members.size()) >= ch.userLimit) {
        sendNumeric(c, "471", chan + " :Cannot join channel (+l)");
        return;
    }

    // clave (si implementas +k): comprobar aquí

    bool first = ch.members.empty();
    ch.members.insert(c->fd);
    if (first) ch.ops.insert(c->fd); // primer usuario operador

    // notifica JOIN
    std::string prefix = ":" + (c->nick.empty() ? "*" : c->nick) + "!" +
                         (c->username.empty() ? "user" : c->username) + "@" + c->host;
    broadcastToChannel(chan, prefix + " JOIN " + chan, NULL);

    // topic si hay
    if (!ch.topic.empty())
        sendNumeric(c, "332", c->nick + " " + chan + " :" + ch.topic);

    // NAMES
    std::string names;
    std::set<int>::iterator it = ch.members.begin();
    for (; it != ch.members.end(); ++it) {
        Client* m = _clients[*it];
        bool isop = ch.ops.count(*it) != 0;
        if (m) {
            if (!names.empty()) names += " ";
            names += (isop ? "@" : "") + (m->nick.empty() ? "*" : m->nick);
        }
    }
    sendNumeric(c, "353", "= " + chan + " :" + names);
    sendNumeric(c, "366", chan + " :End of /NAMES list");
}

void Server::cmdJOIN(Client* c, const std::vector<std::string>& params) {
    if (!requireRegistered(c)) return;
    if (params.empty()) {
        sendNumeric(c, "461", "JOIN :Not enough parameters");
        return;
    }
    // soporta JOIN #a,#b
    std::string list = params[0];
    std::string cur;
    for (size_t i = 0; i <= list.size(); ++i) {
        if (i == list.size() || list[i] == ',') {
            if (!cur.empty()) joinChannel(cur, c);
            cur.clear();
        } else {
            cur += list[i];
        }
    }
}

void Server::broadcastToChannel(const std::string& chan, const std::string& msg, Client* except) {
    std::map<std::string, Channel>::iterator it = _channels.find(chan);
    if (it == _channels.end()) return;
    Channel& ch = it->second;

    std::set<int>::iterator m = ch.members.begin();
    for (; m != ch.members.end(); ++m) {
        if (except && *m == except->fd) continue;
        std::map<int, Client*>::iterator cit = _clients.find(*m);
        if (cit != _clients.end()) {
            sendToClient(cit->second, msg);
        }
    }
}

void Server::cmdPRIVMSG(Client* c, const std::vector<std::string>& params) {
    if (!requireRegistered(c)) return;
    if (params.size() < 2) {
        sendNumeric(c, "411", ":No recipient given (PRIVMSG)");
        return;
    }
    std::string target = params[0];
    std::string text   = params[1]; // ya soporta trailing con espacios

    std::string prefix = ":" + (c->nick.empty() ? "*" : c->nick) + "!" +
                         (c->username.empty() ? "user" : c->username) + "@" + c->host;

    if (isChannel(target)) {
        std::map<std::string, Channel>::iterator it = _channels.find(target);
        if (it == _channels.end()) {
            sendNumeric(c, "403", target + " :No such channel");
            return;
        }
        // envía a todos menos al emisor
        broadcastToChannel(target, prefix + " PRIVMSG " + target + " :" + text, c);
    } else {
        // a usuario
        std::map<std::string, int>::iterator nt = _nicks.find(target);
        if (nt == _nicks.end()) {
            sendNumeric(c, "401", target + " :No such nick");
            return;
        }
        std::map<int, Client*>::iterator dest = _clients.find(nt->second);
        if (dest != _clients.end()) {
            sendToClient(dest->second, prefix + " PRIVMSG " + target + " :" + text);
        }
    }
}

void Server::cmdQUIT(Client* c, const std::vector<std::string>& params) {
    std::string msg = (params.empty() ? "Client Quit" : params[0]);

    // notificar a canales
    std::map<std::string, Channel>::iterator ch = _channels.begin();
    for (; ch != _channels.end(); ++ch) {
        if (ch->second.members.count(c->fd)) {
            std::string prefix = ":" + (c->nick.empty() ? "*" : c->nick) + "!" +
                                 (c->username.empty() ? "user" : c->username) + "@" + c->host;
            broadcastToChannel(ch->first, prefix + " QUIT :" + msg, c);
        }
    }
    removeClient(c->fd);
}

void Server::cmdMODE(Client* c, const std::vector<std::string>& params) {
    if (params.empty()) {
        sendNumeric(c, "461", "MODE :Not enough parameters");
        return;
    }
    std::string target = params[0];

    // sólo modos de canal para el subject
    if (!isChannel(target)) {
        // puedes ignorar o responder mínimo
        sendNumeric(c, "501", ":Unknown MODE flag");
        return;
    }

    Channel &ch = _channels[target];
    ch.name = target;

    if (params.size() == 1) {
        // consulta de modos
        std::string modes = "+";
        if (ch.inviteOnly) modes += "i";
        // aquí agregarías k,l,t si están activos, pero basta como ejemplo
        sendNumeric(c, "324", target + " " + modes);
        return;
    }

    // cambiar modos: requiere operador
    if (ch.ops.count(c->fd) == 0) {
        sendNumeric(c, "482", target + " :You're not channel operator");
        return;
    }

    std::string flags = params[1];
    int sign = 0; // +1 o -1

    for (size_t i = 0; i < flags.size(); ++i) {
        char f = flags[i];
        if (f == '+') { sign = +1; continue; }
        if (f == '-') { sign = -1; continue; }

        if (f == 'i') {
            ch.inviteOnly = (sign > 0);
            std::string prefix = ":" + (c->nick.empty() ? "*" : c->nick) + "!" +
                                 (c->username.empty() ? "user" : c->username) + "@" + c->host;
            broadcastToChannel(target, prefix + " MODE " + target + " " + (ch.inviteOnly?"+i":"-i"), NULL);
        }

        // Aquí extenderías:
        // 't' -> restringir TOPIC
        // 'k' -> requiere param para set (+k <key>) / unset (-k)
        // 'o' -> (+o <nick> / -o <nick>) para operadores
        // 'l' -> (+l <n> / -l)
    }
}
