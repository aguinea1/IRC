#include "Server.hpp"
#include <iostream>
#include <csignal>  
#include <cstdlib>   

static bool g_running = true;

//ctrl + c signal
void handleSigint(int) {
    std::cout << "\n[SERVER] SIGINT recieved, closing server..." << std::endl;
    g_running = false;
}

static bool validPort(long p) {
    return p > 0 && p <= 65535;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "[USAGE] ./ircserv <port> <password>\n";
        return 1;
    }

    char* end = 0;
    long p = std::strtol(argv[1], &end, 10);
    if (!end || *end || !validPort(p)) {
        std::cerr << "[PORT]Invalid port\n";
        return 1;
    }

    std::string pass = argv[2];
    if (pass.empty()) {
        std::cerr << "[PASS] Password must not be empty\n";
        return 1;
    }

    // --- Capturamos SIGINT antes de lanzar el servidor ---
    signal(SIGINT, handleSigint);

    try {
        Server s(static_cast<int>(p), pass);
        std::cout << "[SERVER] Iniciating server on port " << p << std::endl;
        std::cout << "[SERVER] Press Ctrl+C to stop it.\n";

        s.run(g_running);  // <- pasamos el flag global
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    std::cout << "[SERVER] server closed.\n";
    return 0;
}

