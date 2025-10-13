#!/bin/bash

# Script para probar los canales del servidor IRC
# Uso: ./test_channels.sh

echo "=== PRUEBA DE CANALES IRC ==="
echo "Conectando al servidor localhost:6667..."
echo ""

# Función para enviar comandos al servidor
send_command() {
    echo "$1" | nc localhost 6667
    sleep 0.5
}

echo "1. Registrando cliente 1 (Alice)..."
{
    echo "PASS testpass"
    sleep 0.5
    echo "NICK alice"
    sleep 0.5
    echo "USER alice 0 * :Alice User"
    sleep 1
} | nc localhost 6667 &

sleep 2

echo "2. Registrando cliente 2 (Bob)..."
{
    echo "PASS testpass"
    sleep 0.5
    echo "NICK bob"
    sleep 0.5
    echo "USER bob 0 * :Bob User"
    sleep 1
} | nc localhost 6667 &

sleep 2

echo "3. Alice se une al canal #test..."
{
    echo "JOIN #test"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "4. Bob se une al canal #test..."
{
    echo "JOIN #test"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "5. Alice envía un mensaje al canal..."
{
    echo "PRIVMSG #test :Hola a todos!"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "6. Bob responde en el canal..."
{
    echo "PRIVMSG #test :Hola Alice!"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "7. Alice establece el tópico del canal..."
{
    echo "TOPIC #test :Canal de prueba para IRC"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "8. Bob consulta el tópico..."
{
    echo "TOPIC #test"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "9. Alice consulta la lista de usuarios..."
{
    echo "NAMES #test"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "10. Bob consulta la lista de canales..."
{
    echo "LIST"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "11. Alice sale del canal..."
{
    echo "PART #test :Adiós!"
    sleep 1
} | nc localhost 6667 &

sleep 1

echo "12. Bob consulta los modos del canal..."
{
    echo "MODE #test"
    sleep 1
} | nc localhost 6667 &

sleep 2

echo ""
echo "=== PRUEBA COMPLETADA ==="
echo "Revisa los logs del servidor para ver las respuestas"
