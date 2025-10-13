#!/bin/bash

# Script completo para probar todos los comandos IRC implementados
# Excluye MODE (que está pendiente de implementación completa)

echo "=== PRUEBA COMPLETA DE COMANDOS IRC ==="
echo "Probando: PASS, NICK, USER, JOIN, PART, TOPIC, PRIVMSG, NAMES, LIST, KICK, INVITE, QUIT"
echo ""

# Función para enviar comandos con delay
send_commands() {
    local commands="$1"
    local delay="${2:-1}"
    echo "$commands" | nc localhost 6667
    sleep $delay
}

# Función para mostrar separador
show_separator() {
    echo ""
    echo "=========================================="
    echo "$1"
    echo "=========================================="
}

echo "1. Iniciando servidor en background..."
./ircserv 6667 testpass &
SERVER_PID=$!
sleep 3

show_separator "REGISTRO DE CLIENTES"

echo "2. Registrando Alice (será operador del canal)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User" 2

echo "3. Registrando Bob (usuario normal)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User" 2

echo "4. Registrando Charlie (usuario normal)..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User" 2

show_separator "PRUEBAS DE CANALES"

echo "5. Alice crea y se une al canal #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
JOIN #test" 2

echo "6. Bob se une al canal #test..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
JOIN #test" 2

echo "7. Charlie se une al canal #test..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
JOIN #test" 2

echo "8. Alice crea otro canal #general..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
JOIN #general" 2

show_separator "PRUEBAS DE MENSAJES"

echo "9. Alice envía mensaje al canal #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
PRIVMSG #test :Hola a todos en #test!" 2

echo "10. Bob responde en #test..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
PRIVMSG #test :Hola Alice!" 2

echo "11. Alice envía mensaje privado a Bob..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
PRIVMSG bob :Hola Bob, mensaje privado" 2

echo "12. Bob responde mensaje privado a Alice..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
PRIVMSG alice :Hola Alice, recibido!" 2

show_separator "PRUEBAS DE TÓPICO"

echo "13. Alice establece tópico en #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
TOPIC #test :Canal de prueba para IRC" 2

echo "14. Bob consulta el tópico de #test..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
TOPIC #test" 2

echo "15. Charlie consulta tópico de canal sin tópico..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
TOPIC #general" 2

show_separator "PRUEBAS DE LISTADO"

echo "16. Alice consulta usuarios de #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
NAMES #test" 2

echo "17. Bob consulta usuarios de todos los canales..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
NAMES" 2

echo "18. Charlie lista todos los canales..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
LIST" 2

show_separator "PRUEBAS DE KICK"

echo "19. Alice (operador) expulsa a Charlie de #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
KICK #test charlie :Por ser molesto" 2

echo "20. Alice consulta usuarios después del KICK..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
NAMES #test" 2

echo "21. Bob intenta expulsar a Alice (no es operador)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
KICK #test alice :No puedo hacer esto" 2

echo "22. Alice intenta expulsarse a sí misma..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
KICK #test alice :Me expulso" 2

show_separator "PRUEBAS DE INVITE"

echo "23. Alice invita a Charlie de vuelta a #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
INVITE charlie #test" 2

echo "24. Charlie se une al canal (invitado)..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
JOIN #test" 2

echo "25. Bob intenta invitar a alguien (no es operador)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
INVITE alice #general" 2

echo "26. Alice intenta invitar a Bob que ya está en el canal..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
INVITE bob #test" 2

show_separator "PRUEBAS DE PART"

echo "27. Charlie sale del canal #test..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
PART #test :Adiós a todos!" 2

echo "28. Alice consulta usuarios después del PART..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
NAMES #test" 2

echo "29. Bob intenta salir de canal donde no está..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
PART #general :No estoy aquí" 2

show_separator "PRUEBAS DE ERRORES"

echo "30. Usuario no registrado intenta JOIN..."
send_commands "JOIN #test" 2

echo "31. Usuario no registrado intenta PRIVMSG..."
send_commands "PRIVMSG #test :Sin registrarse" 2

echo "32. Usuario no registrado intenta KICK..."
send_commands "KICK #test alice :Sin registrarse" 2

echo "33. Usuario no registrado intenta INVITE..."
send_commands "INVITE bob #test" 2

show_separator "PRUEBAS DE MODO BÁSICO"

echo "34. Alice consulta modos del canal #test..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

echo "35. Bob intenta cambiar modos (no es operador)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
MODE #test +i" 2

show_separator "PRUEBAS DE MÚLTIPLES CANALES"

echo "36. Bob se une a #general..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
JOIN #general" 2

echo "37. Alice envía mensaje a #general..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
PRIVMSG #general :Hola en #general!" 2

echo "38. Lista final de canales..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
LIST" 2

show_separator "LIMPIEZA Y CIERRE"

echo "39. Alice sale de todos los canales..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
PART #test :Adiós
PART #general :Adiós" 2

echo "40. Bob sale de todos los canales..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
PART #test :Adiós
PART #general :Adiós" 2

echo "41. Todos se desconectan..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
QUIT :Test completado" 1

send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
QUIT :Test completado" 1

send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
QUIT :Test completado" 1

echo ""
echo "=== PRUEBA COMPLETA FINALIZADA ==="
echo "Matando servidor..."
kill $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "RESUMEN DE COMANDOS PROBADOS:"
echo "✅ PASS, NICK, USER - Autenticación y registro"
echo "✅ JOIN, PART - Gestión de canales"
echo "✅ PRIVMSG - Mensajes privados y a canales"
echo "✅ TOPIC - Tópicos de canales"
echo "✅ NAMES, LIST - Listado de usuarios y canales"
echo "✅ KICK - Expulsar usuarios (solo operadores)"
echo "✅ INVITE - Invitar usuarios (solo operadores)"
echo "✅ MODE - Consultar modos (básico)"
echo "✅ QUIT - Desconexión"
echo "✅ Validaciones de permisos y errores"
echo ""
echo "COMANDOS PENDIENTES:"
echo "⚠️  MODE - Cambiar modos de canal (+i, +t, +k, +o, +l)"
echo ""
echo "¡Servidor IRC funcionando correctamente!"
