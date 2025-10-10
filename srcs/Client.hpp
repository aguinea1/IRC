#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>

class Client {
private:
    int _fd;
    std::string _host;
    std::string _nick;
    std::string _username;
    std::string _realname;
    std::string _bufferIn;
    std::string _bufferOut;
    bool _passOk;
    bool _hasNick;
    bool _hasUser;
    bool _registered;

public:
    Client(int fd, const std::string& host);
    ~Client();

    int getFd() const;
    const std::string& getNick() const;
    const std::string& getUsername() const;
    const std::string& getHost() const;
    bool isRegistered() const;

    void setNick(const std::string& n);
    void setUsername(const std::string& u);
    void setRealname(const std::string& r);
    void markPassOk();
    void markRegistered();

    bool readFromSocket();
    bool writeToSocket();

    bool hasCompleteLine() const;
    std::string extractLine();

    void appendOutput(const std::string& msg);
    void clearBuffers();

    // Estado de registro
    bool passOk() const { return _passOk; }
    bool hasNick() const { return _hasNick; }
    bool hasUser() const { return _hasUser; }
    void setHasNick(bool v) { _hasNick = v; }
    void setHasUser(bool v) { _hasUser = v; }
};

#endif
