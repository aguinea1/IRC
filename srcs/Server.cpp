#include "Server.hpp"
#include <cstdio>    // perror
#include <cstdlib>   // exit, EXIT_FAILURE
#include <cerrno>    // errno
#include <cstring>   // memset
#include <unistd.h>  // close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>


Server::Server(int port, const std::string &password)
    : _port(port), _password(password), _listenSocket(-1) {}

Server::~Server() {
    if (_listenSocket != -1)
        close(_listenSocket);

    // Limpiar clientes
    for (std::map<int, Client*>::iterator it = _clients_fd.begin(); it != _clients_fd.end(); ++it)
        delete it->second;
}

void Server::init() {
    _listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenSocket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Permitir reutilizar el puerto inmediatamente después de cerrar el servidor
    int opt = 1;
    if (setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(_listenSocket);
        exit(EXIT_FAILURE);
    }

    sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
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

void Server::removeClient(int fd) {
    std::map<int, Client*>::iterator it = _clients_fd.find(fd);
    if (it != _clients_fd.end()) {
        Client* client = it->second;
        // Cerrar el socket del cliente
        if (client->fd != -1) {
            close(client->fd);
        }
        // Liberar memoria
        delete client;

        // Borrar del mapa
        _clients_fd.erase(it);

        std::cout << "Cliente con fd " << fd << " eliminado." << std::endl;
    }
}

void Server::run() {
    fd_set readfds;
    int max_fd;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(_listenSocket, &readfds);
        max_fd = _listenSocket;

        // añadir todos los clientes al set
        std::map<int, Client*>::iterator it;
        for (it = _clients_fd.begin(); it != _clients_fd.end(); ++it) {
            int fd = it->first;
            FD_SET(fd, &readfds);
            if (fd > max_fd)
                max_fd = fd;
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // nuevo cliente
        if (FD_ISSET(_listenSocket, &readfds)) {
            struct sockaddr_in clientAddr;
            socklen_t addrlen = sizeof(clientAddr);
            int new_fd = accept(_listenSocket, (struct sockaddr*)&clientAddr, &addrlen);
            if (new_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                perror("accept");
            } else {
                addClient(new_fd);
                std::cout << "Nueva conexión desde "
                          << inet_ntoa(clientAddr.sin_addr)
                          << ":" << ntohs(clientAddr.sin_port) << std::endl;
            }
        }

        // mensajes de clientes existentes
        for (it = _clients_fd.begin(); it != _clients_fd.end(); ) {
            int fd = it->first;
            Client* client = it->second;
            ++it;

            if (FD_ISSET(fd, &readfds)) {
                char buffer[512];
                int bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                    std::cout << "Cliente desconectado: "
                              << (client->nick.empty() ? "unknown" : client->nick)
                              << std::endl;
                    removeClient(fd);
                } else {
                    buffer[bytes] = '\0';
                    std::string nick = client->nick.empty() ? "unknown" : client->nick;
                    std::cout << nick << ": " << buffer;
                }
            }
        }
    }
}



// Añadir cliente (solo prueba)
void Server::addClient(int fd) {
    Client* client = new Client(fd);
    _clients_fd[fd] = client;
    std::cout << "Cliente agregado con fd: " << fd << std::endl;
}
