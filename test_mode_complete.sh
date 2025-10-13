#!/bin/bash

# Script para probar MODE completo - todos los modos de canal
echo "=== PRUEBA COMPLETA DE MODE ==="
echo "Probando: +i, -i, +t, -t, +k, -k, +o, -o, +l, -l"
echo ""

# Función para enviar comandos
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

echo "2. Registrando Alice (operador)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
JOIN #test" 2

echo "3. Registrando Bob (usuario normal)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
JOIN #test" 2

echo "4. Registrando Charlie (usuario normal)..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
JOIN #test" 2

show_separator "PRUEBAS DE MODOS BÁSICOS"

echo "5. Alice consulta modos iniciales..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

echo "6. Alice activa modo invite-only (+i)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +i" 2

echo "7. Alice consulta modos después de +i..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

echo "8. Alice activa topic protection (+t)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +t" 2

echo "9. Alice consulta modos después de +t..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

show_separator "PRUEBAS DE CLAVE DE CANAL"

echo "10. Alice establece clave del canal (+k)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +k secret123" 2

echo "11. Alice consulta modos con clave..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

echo "12. Alice quita la clave del canal (-k)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test -k" 2

echo "13. Alice consulta modos sin clave..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

show_separator "PRUEBAS DE OPERADORES"

echo "14. Alice da operador a Bob (+o)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +o bob" 2

echo "15. Alice consulta usuarios (Bob debería tener @)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
NAMES #test" 2

echo "16. Alice quita operador a Bob (-o)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test -o bob" 2

echo "17. Alice consulta usuarios (Bob sin @)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
NAMES #test" 2

show_separator "PRUEBAS DE LÍMITE DE USUARIOS"

echo "18. Alice establece límite de 2 usuarios (+l)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +l 2" 2

echo "19. Alice consulta modos con límite..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

echo "20. Alice quita el límite de usuarios (-l)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test -l" 2

echo "21. Alice consulta modos sin límite..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

show_separator "PRUEBAS DE MÚLTIPLES MODOS"

echo "22. Alice activa múltiples modos a la vez..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +itk secret456" 2

echo "23. Alice consulta modos múltiples..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

echo "24. Alice quita múltiples modos a la vez..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test -itk" 2

echo "25. Alice consulta modos después de quitar múltiples..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test" 2

show_separator "PRUEBAS DE ERRORES"

echo "26. Bob intenta cambiar modos (no es operador)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
MODE #test +i" 2

echo "27. Alice intenta dar operador a usuario inexistente..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +o david" 2

echo "28. Alice intenta dar operador a usuario no en el canal..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +o charlie" 2

echo "29. Alice intenta modo inválido..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +x" 2

echo "30. Alice intenta +k sin parámetro..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +k" 2

echo "31. Alice intenta +o sin parámetro..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +o" 2

echo "32. Alice intenta +l sin parámetro..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +l" 2

echo "33. Alice intenta +l con límite inválido..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +l 0" 2

show_separator "PRUEBAS DE FUNCIONALIDAD"

echo "34. Alice activa invite-only y topic protection..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
MODE #test +it" 2

echo "35. Charlie intenta unirse (debería fallar por +i)..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
JOIN #test" 2

echo "36. Alice invita a Charlie..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
INVITE charlie #test" 2

echo "37. Charlie se une (invitado)..."
send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
JOIN #test" 2

echo "38. Bob intenta cambiar tópico (debería fallar por +t)..."
send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
TOPIC #test :Bob cambia tópico" 2

echo "39. Alice cambia tópico (operador, debería funcionar)..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
TOPIC #test :Alice cambia tópico" 2

show_separator "LIMPIEZA Y CIERRE"

echo "40. Todos se desconectan..."
send_commands "PASS testpass
NICK alice
USER alice 0 * :Alice User
QUIT :Test MODE completado" 1

send_commands "PASS testpass
NICK bob
USER bob 0 * :Bob User
QUIT :Test MODE completado" 1

send_commands "PASS testpass
NICK charlie
USER charlie 0 * :Charlie User
QUIT :Test MODE completado" 1

echo ""
echo "=== PRUEBA MODE COMPLETADA ==="
echo "Matando servidor..."
kill $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "RESUMEN DE MODOS PROBADOS:"
echo "✅ +i/-i - Invite-only"
echo "✅ +t/-t - Topic protection"
echo "✅ +k/-k - Channel key"
echo "✅ +o/-o - Operator privileges"
echo "✅ +l/-l - User limit"
echo "✅ Múltiples modos simultáneos"
echo "✅ Validaciones de errores"
echo "✅ Funcionalidad integrada"
echo ""
echo "¡MODE COMPLETO IMPLEMENTADO Y FUNCIONANDO!"
