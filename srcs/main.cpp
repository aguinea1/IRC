#include <iostream>
#include <string>
#include <cstdlib>
#include <cctype>
#include "Server.hpp"
#include "Client.hpp"
//Probando si password esta vacio
static bool parse_pass(const std::string &pass) {
    if (!pass.empty())
        return true;
    std::cout << "Error:\nWrong password\n";
    return false;
}
//probando si port son 4 numeros y si se encuentra entre 1 y 65535(protocolo TCP)el mayor valor que cabe en 16 bits)
static bool parse_port(const std::string &port) {
    for (size_t i = 0; i < port.size(); i++) {
        if (!std::isdigit(port[i])) {
            std::cout << "Error:\nWrong port" << std::endl;
            return false;
        }
    }
    long p = std::strtol(port.c_str(), NULL, 10);//el numero en base 10
    if (p < 1 || p > 65535) {
        std::cout << "Error:\nWrong port" << std::endl;
        return false;
    }
    return true;
}

static bool parse(char **av) {
    std::string port = av[1];
    std::string pass = av[2];
    return parse_port(port) && parse_pass(pass);
}

static void message(const std::string &port, const std::string &password) {
    std::cout << "Listening on port " << port
              << " With password: " << password << std::endl;
}

int main(int ac, char **av) {
    if (ac != 3) {
        std::cout << "Error:\nInvalid number of Arguments.\n"
                  << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    } 

    if (parse(av)) {
        message(av[1], av[2]);
		int port = std::atoi(av[1]);
		std::string pass = av[2];
		Server server(port, pass);
		server.init();
		server.run();

    }
    return 0;
}



