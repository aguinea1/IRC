#include "Channel.hpp"
#include <iostream>
#include <sstream>

// -------------------- constructor / destructor --------------------

Channel::Channel(const std::string& name)
    : _name(name), _topic(""), _userLimit(0),
      _inviteOnly(false), _topicProtected(false), _keyProtected(false),
      _userLimitSet(false), _moderated(false), _secret(false), _private(false) {
}

Channel::~Channel() {
    clear();
}

// -------------------- getters --------------------

const std::string& Channel::getName() const {
    return _name;
}

const std::string& Channel::getTopic() const {
    return _topic;
}

const std::set<std::string>& Channel::getUsers() const {
    return _users;
}

const std::set<std::string>& Channel::getOperators() const {
    return _operators;
}

bool Channel::isInviteOnly() const {
    return _inviteOnly;
}

bool Channel::isTopicProtected() const {
    return _topicProtected;
}

bool Channel::isKeyProtected() const {
    return _keyProtected;
}

bool Channel::isUserLimitSet() const {
    return _userLimitSet;
}

bool Channel::isModerated() const {
    return _moderated;
}

bool Channel::isSecret() const {
    return _secret;
}

bool Channel::isPrivate() const {
    return _private;
}

const std::string& Channel::getKey() const {
    return _key;
}

int Channel::getUserLimit() const {
    return _userLimit;
}

size_t Channel::getUserCount() const {
    return _users.size();
}

// -------------------- setters --------------------

void Channel::setTopic(const std::string& topic) {
    _topic = topic;
}

void Channel::setKey(const std::string& key) {
    _key = key;
}

void Channel::setUserLimit(int limit) {
    _userLimit = limit;
}

void Channel::setInviteOnly(bool value) {
    _inviteOnly = value;
}

void Channel::setTopicProtected(bool value) {
    _topicProtected = value;
}

void Channel::setKeyProtected(bool value) {
    _keyProtected = value;
}

void Channel::setUserLimitSet(bool value) {
    _userLimitSet = value;
}

void Channel::setModerated(bool value) {
    _moderated = value;
}

void Channel::setSecret(bool value) {
    _secret = value;
}

void Channel::setPrivate(bool value) {
    _private = value;
}

// -------------------- gestión de usuarios --------------------

void Channel::addUser(const std::string& nickname) {
    _users.insert(nickname);
}

void Channel::removeUser(const std::string& nickname) {
    _users.erase(nickname);
    _operators.erase(nickname);
    _invited.erase(nickname);
}

bool Channel::hasUser(const std::string& nickname) const {
    return _users.find(nickname) != _users.end();
}

void Channel::addOperator(const std::string& nickname) {
    if (_users.find(nickname) != _users.end()) {
        _operators.insert(nickname);
    }
}

void Channel::removeOperator(const std::string& nickname) {
    _operators.erase(nickname);
}

bool Channel::isOperator(const std::string& nickname) const {
    return _operators.find(nickname) != _operators.end();
}

void Channel::addInvited(const std::string& nickname) {
    _invited.insert(nickname);
}

void Channel::removeInvited(const std::string& nickname) {
    _invited.erase(nickname);
}

bool Channel::isInvited(const std::string& nickname) const {
    return _invited.find(nickname) != _invited.end();
}

// -------------------- utilidades --------------------

std::string Channel::getModeString() const {
    std::string modes = "+";
    std::string params = "";
    
    if (_inviteOnly) modes += "i";
    if (_topicProtected) modes += "t";
    if (_keyProtected) {
        modes += "k";
        params += " " + _key;
    }
    if (_userLimitSet) {
        modes += "l";
        std::ostringstream oss;
        oss << _userLimit;
        params += " " + oss.str();
    }
    if (_moderated) modes += "m";
    if (_secret) modes += "s";
    if (_private) modes += "p";
    
    return modes + params;
}

std::string Channel::getUserList() const {
    std::string userList = "";
    bool first = true;
    
    for (std::set<std::string>::const_iterator it = _users.begin(); it != _users.end(); ++it) {
        if (!first) userList += " ";
        if (_operators.find(*it) != _operators.end()) {
            userList += "@" + *it;
        } else {
            userList += *it;
        }
        first = false;
    }
    
    return userList;
}

std::string Channel::getVisibleUserList() const {
    if (_secret) {
        return ""; // No mostrar usuarios en canales secretos
    }
    return getUserList();
}

bool Channel::canJoin(const std::string& nickname, const std::string& key) const {
    // Verificar si ya está en el canal
    if (hasUser(nickname)) {
        return false;
    }
    
    // Verificar límite de usuarios
    if (_userLimitSet && _users.size() >= static_cast<size_t>(_userLimit)) {
        return false;
    }
    
    // Verificar modo invitación
    if (_inviteOnly && !isInvited(nickname)) {
        return false;
    }
    
    // Verificar clave del canal
    if (_keyProtected && key != _key) {
        return false;
    }
    
    return true;
}

void Channel::broadcast(const std::string& message, const std::string& excludeNick) {
    // Este método será implementado cuando tengamos acceso a los clientes
    // Por ahora solo lo declaramos
    (void)message;
    (void)excludeNick;
}

void Channel::broadcastToOperators(const std::string& message) {
    // Este método será implementado cuando tengamos acceso a los clientes
    (void)message;
}

bool Channel::isEmpty() const {
    return _users.empty();
}

void Channel::clear() {
    _users.clear();
    _operators.clear();
    _invited.clear();
    _topic.clear();
    _key.clear();
    _userLimit = 0;
    _inviteOnly = false;
    _topicProtected = false;
    _keyProtected = false;
    _userLimitSet = false;
    _moderated = false;
    _secret = false;
    _private = false;
}
