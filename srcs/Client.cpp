
#include "Client.hpp"
#include "Server.hpp"
#include <ctime>
#include <unistd.h> 

Client::Client(int socket) : fd(socket), registered(false), last_activity(time(NULL)) {}
Client::~Client() {
    if (fd != -1) {
        close(fd);
    }
}

void Client::resetBuffers() {
    buffer_in.clear();
    buffer_out.clear();
}
