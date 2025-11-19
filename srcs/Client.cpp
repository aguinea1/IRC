#include "Client.hpp"
#include <sys/socket.h>
#include <cerrno>
//-----------------------------CONSTRUCTOR && DESTRUCTOR-----------------------
Client::Client(int fd, const std::string& host)
    : _fd(fd), _host(host),
      _passOk(false), _hasNick(false), _hasUser(false), _registered(false) {}

Client::~Client() {
    ::close(_fd);
}
//----------------------------------GETTERS-----------------------------------
int Client::getFd() const { return _fd; }
const std::string& Client::getNick() const { return _nick; }
const std::string& Client::getUsername() const { return _username; }
const std::string& Client::getHost() const { return _host; }
bool Client::isRegistered() const { return _registered; }

//---------------------------------SETTERS------------------------------------
void Client::setNick(const std::string& n) { _nick = n; _hasNick = true; }
void Client::setUsername(const std::string& u) { _username = u; _hasUser = true; }
void Client::setRealname(const std::string& r) { _realname = r; }
void Client::markPassOk() { _passOk = true; }
void Client::markRegistered() { _registered = true; }

//recv puede acceder las veces que quiera al socket(hasta que ya no hay mas datos por leer), porque el socket  es no bloqueante
//no se detiene en recv -> como si recv fuera abriendo procesos con fork() "varios procesos"
bool Client::readFromSocket() {
    char buf[4096];
    while (true) {
        ssize_t n = ::recv(_fd, buf, sizeof(buf), 0);//leer datos del socket
        if (n > 0) {
            _bufferIn.append(buf, static_cast<size_t>(n));//guardar en socket
            if (_bufferIn.size() > 65536) return false;//puede dar overflow
        } else if (n == 0) {
            // EOF (Ctrl+D) - limpiar buffer de entrada en lugar de desconectar
            _bufferIn.clear();
            return true; // mantener conexión activa
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN) break;//n <0 error
            return false;
        }
    }
    return true;
}

bool Client::writeToSocket() {
    while (!_bufferOut.empty()) {
        ssize_t n = ::send(_fd, _bufferOut.data(), _bufferOut.size(), 0);// si el buffer no esta vacio envia los datos al socket
        if (n > 0) {
            _bufferOut.erase(0, static_cast<size_t>(n));//borrar datos si se han enviado
        } else {
            if (errno == EWOULDBLOCK || errno == EAGAIN)//el buffer de salida del socket esta lleno(no se pueden enviar mas datos)
                return true;//lo volvera a intentar
            return false;//desconexion(quit ...)
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
