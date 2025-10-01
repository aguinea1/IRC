// Client.hpp
#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>
#include "Server.hpp"

class Client {
public:
    int fd;
    std::string nick;
    std::string username;
    std::string realname;
    std::string buffer_in;
    std::string buffer_out;
    bool registered;
    std::string modes;
    time_t last_activity;

    Client(int socket);
    ~Client();

    void resetBuffers();
    //Client(const Client&) = delete;
    //Client& operator=(const Client&) = delete;
};

#endif
