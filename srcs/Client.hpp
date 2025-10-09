#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <cerrno>
#include <unistd.h>

class Client {
public:
    int         fd;
    std::string nick;
    std::string username;
    std::string realname;
    std::string host;       // opcional: "localhost" o lo que resuelvas
    std::string buffer_in;  // datos acumulados de lectura
    std::string buffer_out; // cola de salida (se envía solo cuando writefds lo permite)

    Client(int cfd, const std::string& h = "localhost")
    : fd(cfd), host(h) {}

    ~Client() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    // deshabilita copia
private:
    Client(const Client&);
    Client& operator=(const Client&);
};

#endif // CLIENT_HPP

