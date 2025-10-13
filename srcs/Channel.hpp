#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>
#include <vector>
#include "Client.hpp"

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::set<std::string> _users;           // nicknames de usuarios en el canal
    std::set<std::string> _operators;       // operadores del canal
    std::set<std::string> _invited;         // usuarios invitados
    std::string _key;                       // clave del canal (modo +k)
    int _userLimit;                         // límite de usuarios (modo +l)
    bool _inviteOnly;                       // modo +i
    bool _topicProtected;                   // modo +t
    bool _keyProtected;                     // modo +k
    bool _userLimitSet;                     // modo +l
    bool _moderated;                        // modo +m
    bool _secret;                           // modo +s
    bool _private;                          // modo +p

public:
    Channel(const std::string& name);
    ~Channel();

    // Getters
    const std::string& getName() const;
    const std::string& getTopic() const;
    const std::set<std::string>& getUsers() const;
    const std::set<std::string>& getOperators() const;
    bool isInviteOnly() const;
    bool isTopicProtected() const;
    bool isKeyProtected() const;
    bool isUserLimitSet() const;
    bool isModerated() const;
    bool isSecret() const;
    bool isPrivate() const;
    const std::string& getKey() const;
    int getUserLimit() const;
    size_t getUserCount() const;

    // Setters
    void setTopic(const std::string& topic);
    void setKey(const std::string& key);
    void setUserLimit(int limit);
    void setInviteOnly(bool value);
    void setTopicProtected(bool value);
    void setKeyProtected(bool value);
    void setUserLimitSet(bool value);
    void setModerated(bool value);
    void setSecret(bool value);
    void setPrivate(bool value);

    // Gestión de usuarios
    void addUser(const std::string& nickname);
    void removeUser(const std::string& nickname);
    bool hasUser(const std::string& nickname) const;
    void addOperator(const std::string& nickname);
    void removeOperator(const std::string& nickname);
    bool isOperator(const std::string& nickname) const;
    void addInvited(const std::string& nickname);
    void removeInvited(const std::string& nickname);
    bool isInvited(const std::string& nickname) const;

    // Utilidades
    std::string getModeString() const;
    std::string getUserList() const;
    std::string getVisibleUserList() const; // Para LIST, sin usuarios en canales secretos
    bool canJoin(const std::string& nickname, const std::string& key = "") const;
    void broadcast(const std::string& message, const std::string& excludeNick = "");
    void broadcastToOperators(const std::string& message);
    bool isEmpty() const;
    void clear();
};

#endif
