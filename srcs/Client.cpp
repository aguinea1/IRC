#include "Client.hpp"
#include <sys/socket.h>
#include <cerrno>

Client::Client(int fd, const std::string& host)
    : _fd(fd), _host(host),
      _passOk(false), _hasNick(false), _hasUser(false), _registered(false) {}

Client::~Client() {
    ::close(_fd);
}

int Client::getFd() const { return _fd; }
const std::string& Client::getNick() const { return _nick; }
const std::string& Client::getUsername() const { return _username; }
const std::string& Client::getHost() const { return _host; }
bool Client::isRegistered() const { return _registered; }

void Client::setNick(const std::string& n) { _nick = n; _hasNick = true; }
void Client::setUsername(const std::string& u) { _username = u; _hasUser = true; }
void Client::setRealname(const std::string& r) { _realname = r; }
void Client::markPassOk() { _passOk = true; }
void Client::markRegistered() { _registered = true; }

bool Client::readFromSocket() {
    char buf[4096];
    while (true) {
        ssize_t n = ::recv(_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            _bufferIn.append(buf, static_cast<size_t>(n));
            if (_bufferIn.size() > 65536) return false;
        } else if (n == 0) {
            return false;
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) break;
            return false;
        }
    }
    return true;
}

bool Client::writeToSocket() {
    while (!_bufferOut.empty()) {
        ssize_t n = ::send(_fd, _bufferOut.data(), _bufferOut.size(), 0);
        if (n > 0) {
            _bufferOut.erase(0, static_cast<size_t>(n));
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return true;
            return false;
        }
    }
    return true;
}

bool Client::hasCompleteLine() const {
    return _bufferIn.find('\n') != std::string::npos;
}

std::string Client::extractLine() {
    std::string::size_type pos = _bufferIn.find('\n');
    if (pos == std::string::npos) return "";
    std::string line = _bufferIn.substr(0, pos + 1);
    _bufferIn.erase(0, pos + 1);
    return line;
}

void Client::appendOutput(const std::string& msg) {
    if (msg.size() >= 2 && msg.substr(msg.size()-2) == "\r\n")
        _bufferOut += msg;
    else
        _bufferOut += msg + "\r\n";
}

void Client::clearBuffers() {
    _bufferIn.clear();
    _bufferOut.clear();
}
