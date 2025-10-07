/*
 * Server.cpp - versión comentada y explicada
 * Estándar: C++98 (gnuc++98)
 * Propósito: implementación básica de un servidor IRC (solo servidor)
 * Comentarios detallados añadidos para cada bloque de funcionalidad.
 *
 * Notas rápidas:
 *  - Esta versión asume sockets en modo no bloqueante (O_NONBLOCK).
 *  - Usa select() para monitorizar lecturas. Para robustez en envío, convendría
 *    añadir también monitorización de escrituras (writefds) para vaciar buffer_out.
 *  - El parsing de comandos es deliberadamente simple; para cumplir RFC/ft_irc
 *    necesitarás aumentar la validación y respuestas numéricas.
 */

#include "Server.hpp"
#include <cstdio>    // perror
#include <cstdlib>   // exit, EXIT_FAILURE
#include <cerrno>    // errno
#include <cstring>   // memset, strerror
#include <unistd.h>  // close, read, write
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fcntl.h>   // fcntl

// Constructor: guarda puerto y password, inicializa listen socket a -1
Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _listenSocket(-1) {}

// Destructor: cierra socket de escucha si existe y libera memoria de clientes
Server::~Server() {
    if (_listenSocket != -1)
        close(_listenSocket);

    // Liberar objetos Client almacenados en el mapa
    for (std::map<int, Client*>::iterator it = _clients_fd.begin(); it != _clients_fd.end(); ++it)
        delete it->second;
}

// setNonBlocking: activa O_NONBLOCK en el file descriptor dado
// - Utilizamos fcntl para no bloquear llamadas a recv/send/accept
// - En MacOS el enunciado permite fcntl(fd, F_SETFL, O_NONBLOCK)
void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) flags = 0; // si falla, asumimos 0
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// init: crea el socket de escucha, lo deja en non-blocking, bind y listen
// - setsockopt(SO_REUSEADDR) para poder reutilizar el puerto rápidamente
// - se sale con EXIT_FAILURE en caso de error (puedes cambiar por excepciones)
void Server::init() {
    _listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenSocket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(_listenSocket);
        exit(EXIT_FAILURE);
    }

    // Poner el socket de escucha en modo no bloqueante
    setNonBlocking(_listenSocket);

    sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY; // escuchar en todas las interfaces
    servAddr.sin_port = htons(_port);

    if (bind(_listenSocket, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        perror("bind");
        close(_listenSocket);
        exit(EXIT_FAILURE);
    }

    if (listen(_listenSocket, 10) < 0) {
        perror("listen");
        close(_listenSocket);
        exit(EXIT_FAILURE);
    }

    std::cout << "Servidor escuchando en puerto " << _port
              << " con password: " << _password << std::endl;
}

// addClient: aceptar un nuevo cliente (ya obtenido por accept en run())
// - marca el socket como non-blocking
// - crea un objeto Client y lo añade al mapa _clients_fd
void Server::addClient(int fd) {
    setNonBlocking(fd); // cliente también en non-blocking
    Client* client = new Client(fd);
    _clients_fd[fd] = client;
    std::cout << "Cliente agregado con fd: " << fd << std::endl;
}

// removeClient: limpiar todo lo asociado a un cliente
// Pasos:
//  1) borrar entrada de nick en _clients_nick si la tenía
//  2) remover el cliente de todos los canales en _channels
//  3) cerrar el fd y borrar el objeto Client
//  4) borrar la entrada en _clients_fd
// Nota: esta función puede ser llamada desde el loop de select, por lo que
//       hacemos una búsqueda por fd y borramos con cuidado para no invalidar iteradores externos.
void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients_fd.find(fd);
    if (it != _clients_fd.end()) {
        Client* client = it->second;

        // eliminar de mapa por nick si existía
        if (!client->nick.empty()) {
            _clients_nick.erase(client->nick);
        }

        // quitar de todos los canales (recorremos todos los canales)
        for (std::map<std::string, std::vector<Client*> >::iterator cit = _channels.begin();
             cit != _channels.end(); ++cit) {
            std::vector<Client*>& v = cit->second;
            for (std::vector<Client*>::iterator vit = v.begin(); vit != v.end(); ++vit) {
                if ((*vit) == client) {
                    v.erase(vit);
                    break; // salir del vector de este canal
                }
            }
        }

        if (client->fd != -1) {
            close(client->fd);
        }
        delete client; // liberar memoria
        _clients_fd.erase(it);

        std::cout << "Cliente con fd " << fd << " eliminado." << std::endl;
    }
}

// sendToClient: añade el mensaje al buffer de salida y trata de enviarlo inmediatamente
// - En sockets non-blocking send() puede devolver < tamaño pedido (envío parcial)
// - Si send devuelve -1 con errno == EWOULDBLOCK/EAGAIN, debemos mantener el resto en buffer_out
// - Mejora recomendada: usar writefds en select() para saber cuándo el socket es escribible
void Server::sendToClient(Client *client, const std::string &msg) {
    if (!client) return;
    // Guardamos en buffer de salida (por si send() no puede enviar todo ahora)
    client->buffer_out += msg;
    if (!client->buffer_out.empty()) {
        // Intentamos enviar todo lo que haya en buffer_out
        ssize_t sent = send(client->fd, client->buffer_out.c_str(), client->buffer_out.size(), 0);
        if (sent > 0) {
            // borrar bytes enviados
            client->buffer_out.erase(0, sent);
        }
        // Si sent < 0 y errno == EWOULDBLOCK/EAGAIN -> lo dejamos en buffer_out para reintentar
        // Si sent < 0 y errno != EWOULDBLOCK/EAGAIN -> deberíamos cerrar la conexión (no tratado aquí)
    }
}

// broadcastToChannel: envía msg a todos los clientes del canal excepto (opcional) el cliente 'except'
// - El método usa sendToClient() por lo que el envío puede quedar en buffer_out
void Server::broadcastToChannel(const std::string &channel, const std::string &msg, Client *except) {
    std::map<std::string, std::vector<Client*> >::iterator it = _channels.find(channel);
    if (it == _channels.end()) return; // canal desconocido
    std::vector<Client*>& clients = it->second;
    for (size_t i = 0; i < clients.size(); ++i) {
        Client *c = clients[i];
        if (except && c->fd == except->fd) continue; // no reenviar al emisor
        sendToClient(c, msg);
    }
}

// joinChannel: añade un cliente a un canal (crea el vector si no existe)
// - Evita duplicados
// - Envía una notificación JOIN al canal (opcional)
void Server::joinChannel(const std::string &channel, Client *client) {
    std::vector<Client*>& vec = _channels[channel];
    // evitar duplicados
    for (size_t i = 0; i < vec.size(); ++i)
        if (vec[i] == client) return;
    vec.push_back(client);

    // notificar a canal que se ha unido (formato simple)
    std::string notice = ":" + client->nick + " JOIN " + channel + "\r\n";
    broadcastToChannel(channel, notice, client);
}

// partChannel: elimina cliente del canal y notifica
void Server::partChannel(const std::string &channel, Client *client) {
    std::map<std::string, std::vector<Client*> >::iterator it = _channels.find(channel);
    if (it == _channels.end()) return;
    std::vector<Client*>& vec = it->second;
    for (std::vector<Client*>::iterator vit = vec.begin(); vit != vec.end(); ++vit) {
        if ((*vit) == client) {
            vec.erase(vit);
            break;
        }
    }
    std::string notice = ":" + client->nick + " PART " + channel + "\r\n";
    broadcastToChannel(channel, notice, client);
}

// processLine: procesa una línea completa proveniente del cliente
// - El parsing es intencionalmente sencillo. Implementa NICK, USER, JOIN, PRIVMSG, QUIT.
// - La implementación sirve como base funcional.
void Server::processLine(Client *client, const std::string &line) {
    if (!client) return;
    if (line.empty()) return;

    // Tokenizar comando simple: separa primer token (cmd) del resto (rest)
    std::string cmd;
    std::string rest;
    size_t pos = line.find(' ');
    if (pos == std::string::npos) {
        cmd = line;
    } else {
        cmd = line.substr(0, pos);
        rest = line.substr(pos + 1);
    }

    // convertir cmd a mayúsculas para comparación sin case-sensitivity
    for (size_t i = 0; i < cmd.size(); ++i) cmd[i] = toupper(cmd[i]);

    if (cmd == "NICK") {
        // NICK <newnick>
        std::string newnick = rest;
        // recortar espacios a la izquierda
        if (!newnick.empty() && newnick[0] == ' ') newnick.erase(0, newnick.find_first_not_of(' '));
        // si hay parámetros adicionales, nos quedamos con el primero
        size_t sp = newnick.find(' ');
        if (sp != std::string::npos) newnick = newnick.substr(0, sp);

        if (newnick.empty()) {
            sendToClient(client, "431 :No nickname given\r\n");
            return;
        }
        if (_clients_nick.find(newnick) != _clients_nick.end()) {
            sendToClient(client, "433 " + newnick + " :Nickname is already in use\r\n");
            return;
        }
        // borrar antiguo nick si existía
        if (!client->nick.empty()) {
            _clients_nick.erase(client->nick);
        }
        client->nick = newnick;
        _clients_nick[newnick] = client;
        // si ya tenemos USER, marcamos registered y enviamos 001 Welcome (simplificado)
        if (!client->username.empty()) {
            client->registered = true;
            sendToClient(client, ":server 001 " + client->nick + " :Welcome\r\n");
        }
        return;
    } else if (cmd == "USER") {
        // USER <username> <hostname> <servername> :<realname>
        // Implementación simplificada: tomamos el primer token como username
        std::string username = rest;
        if (!username.empty() && username[0] == ' ') username.erase(0, username.find_first_not_of(' '));
        size_t sp = username.find(' ');
        if (sp != std::string::npos) username = username.substr(0, sp);
        client->username = username;
        if (!client->nick.empty()) {
            client->registered = true;
            sendToClient(client, ":server 001 " + client->nick + " :Welcome\r\n");
        }
        return;
    } else if (cmd == "JOIN") {
        // JOIN <channel>
        std::string channel = rest;
        if (!channel.empty() && channel[0] == ' ') channel.erase(0, channel.find_first_not_of(' '));
        // tomamos solo primer token
        size_t p = channel.find(' ');
        if (p != std::string::npos) channel = channel.substr(0, p);
        if (channel.empty()) {
            // 461 = ERR_NEEDMOREPARAMS
            sendToClient(client, "461 JOIN :Not enough parameters\r\n");
            return;
        }
        joinChannel(channel, client);
        // Aquí podríamos enviar RPL_TOPIC / RPL_NAMREPLY etc.
        return;
    } else if (cmd == "PRIVMSG") {
        // PRIVMSG <target> :<message>
        std::string target;
        std::string msgtext;
        size_t s = rest.find(' ');
        if (s == std::string::npos) {
            // 411 = ERR_NORECIPIENT
            sendToClient(client, "411 :No recipient given\r\n");
            return;
        } else {
            target = rest.substr(0, s);
            msgtext = rest.substr(s + 1);
            // recortar espacios iniciales de msgtext
            if (!msgtext.empty() && msgtext[0] == ' ')
                msgtext.erase(0, msgtext.find_first_not_of(' '));
            // si comienza por ':' quitarlo (parámetro final según RFC)
            if (!msgtext.empty() && msgtext[0] == ':')
                msgtext.erase(0, 1);
        }

        // Formato de origen simple: :nick!user@host PRIVMSG target :message\r\n
        std::string origin = client->nick.empty() ? "unknown" : client->nick;
        std::string user = client->username.empty() ? "user" : client->username;
        std::string host = "localhost"; // podríamos obtener el host real usando getpeername/inet_ntoa
        std::string full = ":" + origin + "!" + user + "@" + host + " PRIVMSG " + target + " :" + msgtext + "\r\n";

        // Si target es un canal (aquí consideramos '#' como prefijo de canal)
        if (!target.empty() && target[0] == '#') {
            broadcastToChannel(target, full, client);
        } else {
            // mensaje directo a nick
            std::map<std::string, Client*>::iterator cit = _clients_nick.find(target);
            if (cit == _clients_nick.end()) {
                // 401 = ERR_NOSUCHNICK
                sendToClient(client, "401 " + target + " :No such nick/channel\r\n");
            } else {
                sendToClient(cit->second, full);
            }
        }
        return;
    } else if (cmd == "QUIT") {
        // Cliente desconectándose voluntariamente
        removeClient(client->fd);
        return;
    } else {
        // Comando desconocido -> 421 ERR_UNKNOWNCOMMAND
        sendToClient(client, "421 " + cmd + " :Unknown command\r\n");
        return;
    }
}

// handleClientReadable: lee datos del socket del cliente (non-blocking) y los acumula
// en client->buffer_in. Cuando encuentra líneas completas (terminadas en CRLF o LF)
// llama a processLine() por cada línea.
void Server::handleClientReadable(int fd) {
    std::map<int, Client*>::iterator it = _clients_fd.find(fd);
    if (it == _clients_fd.end()) return;
    Client* client = it->second;

    // buffer temporal para leer bytes disponibles
    char buf[512];
    ssize_t bytes = recv(fd, buf, sizeof(buf) - 1, 0);
    if (bytes <= 0) {
        // bytes == 0 -> cierre por parte del cliente
        // bytes < 0 y errno != EWOULDBLOCK/EAGAIN -> error real
        if (bytes == 0 || (bytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
            std::cout << "Cliente desconectado: "
                      << (client->nick.empty() ? "unknown" : client->nick)
                      << std::endl;
            removeClient(fd);
        }
        return;
    }
    buf[bytes] = '\0';
    // append al buffer de entrada del cliente (soporta reads parciales)
    client->buffer_in.append(buf, bytes);

    // procesar todas las líneas completas (CRLF o LF)
    size_t pos;
    while ((pos = client->buffer_in.find("\r\n")) != std::string::npos ||
           (pos = client->buffer_in.find("\n")) != std::string::npos) {
        size_t len = pos;
        std::string line = client->buffer_in.substr(0, len);
        // borrar línea + terminador(s)
        if (client->buffer_in.size() > pos + 2 && client->buffer_in[pos] == '\r' && client->buffer_in[pos+1] == '\n')
            client->buffer_in.erase(0, pos + 2);
        else if (client->buffer_in.size() > pos + 1 && client->buffer_in[pos] == '\n')
            client->buffer_in.erase(0, pos + 1);
        else
            client->buffer_in.erase(0, pos + 1);

        // procesar el comando/linea completa
        processLine(client, line);
    }
}

// run: loop principal que usa select() para monitorizar nuevas conexiones y lecturas
// - Sólo se usa un fd_set readfds (requisito del enunciado que permite select() o poll())
// - Recomendado: añadir writefds para enviar buffer_out pendiente
void Server::run() {
    fd_set readfds;
    int max_fd;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(_listenSocket, &readfds);
        max_fd = _listenSocket;

        // Añadir todos los fds de clientes al conjunto
        for (std::map<int, Client*>::iterator it = _clients_fd.begin(); it != _clients_fd.end(); ++it) {
            int fd = it->first;
            FD_SET(fd, &readfds);
            if (fd > max_fd) max_fd = fd;
        }

        // select bloqueante hasta que haya actividad
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue; // señal interrumpió select, reintentar
            perror("select");
            break;
        }

        // nueva conexión entrante
        if (FD_ISSET(_listenSocket, &readfds)) {
            struct sockaddr_in clientAddr;
            socklen_t addrlen = sizeof(clientAddr);
            int new_fd = accept(_listenSocket, (struct sockaddr*)&clientAddr, &addrlen);
            if (new_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // no hay conexiones pendientes
                perror("accept");
            } else {
                addClient(new_fd);
                std::cout << "Nueva conexión desde "
                          << inet_ntoa(clientAddr.sin_addr)
                          << ":" << ntohs(clientAddr.sin_port) << std::endl;
            }
        }

        // mensajes de clientes existentes
        // Para evitar invalidación de iteradores por removeClient(), copiamos los fds a un vector
        std::vector<int> fds;
        for (std::map<int, Client*>::iterator it = _clients_fd.begin(); it != _clients_fd.end(); ++it)
            fds.push_back(it->first);

        for (size_t i = 0; i < fds.size(); ++i) {
            int fd = fds[i];
            if (FD_ISSET(fd, &readfds)) {
                // handle read
                handleClientReadable(fd);
            }
        }
    }
}

/*
 * Sugerencias / mejoras (resumen):
 *  - Añadir manejo de PASS (autenticación con la contraseña _password) antes de permitir USER/NICK/join.
 *  - Implementar writefds en select() para vaciar buffer_out cuando los sockets sean escribibles.
 *  - Robustecer parsing: manejar parámetros que contengan espacios correctamente según RFC (parámetro final ':').
 *  - Validaciones: límites de longitud en nicks, nombres de canal, protección contra líneas demasiado largas.
 *  - Responder con códigos numéricos RFC correctos (RPL_NAMREPLY, RPL_TOPIC, ERR_* más descriptivos).
 *  - Manejar señales y shutdown ordenado (SIGINT) cerrando clientes y liberando recursos.
 *  - Evitar DoS: limitar número de conexiones por IP, rate limiting, timeouts de inactividad.
 *
 * Compilación recomendada (ejemplo):
 *  g++ -Wall -Wextra -Werror -std=gnu++98 Server.cpp Client.cpp main.cpp -o ircserv
 */
