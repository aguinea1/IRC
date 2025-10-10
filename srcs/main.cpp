#include "Server.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>

static bool running = true;

static bool validPort(long p) { return p > 0 && p <= 65535; }

static void handleSigint(int) {
    std::cout << "\n[SERVER] SIGINT received, shutting down..." << std::endl;
    running = false;          
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./ircserv <port> <password>\n";
        return 1;
    }

    char* end = 0;
    long p = std::strtol(argv[1], &end, 10);
    if (!end || *end || !validPort(p)) {
        std::cerr << "Invalid port\n";
        return 1;
    }

    std::string pass = argv[2];
    if (pass.empty()) {
        std::cerr << "Password must not be empty\n";
        return 1;
    }

    std::signal(SIGINT, handleSigint);

    try {
        Server s(static_cast<int>(p), pass);
        s.run(running);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
