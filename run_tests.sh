#!/bin/bash

# Script para compilar el servidor IRC y ejecutar pruebas de concatenación de modos

set -e  # Salir si hay algún error

echo "🔧 Compilando el servidor IRC..."
make clean
make

echo ""
echo "🚀 Iniciando el servidor IRC..."
# Iniciar el servidor en segundo plano
./ircserv 6667 password &
SERVER_PID=$!

# Esperar a que el servidor se inicie
sleep 2

echo "📊 Servidor iniciado con PID: $SERVER_PID"
echo ""

# Función para limpiar al salir
cleanup() {
    echo ""
    echo "🔄 Deteniendo el servidor..."
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    echo "✅ Servidor detenido"
}

# Configurar limpieza al salir
trap cleanup EXIT

echo "🧪 Ejecutando pruebas de concatenación de modos..."
echo "=============================================="

# Ejecutar las pruebas
python3 test_mode_concatenation.py

echo ""
echo "✅ Pruebas completadas!"
echo ""
echo "📋 Resumen de modos soportados:"
echo "   - i: invite-only (solo por invitación)"
echo "   - t: topic protection (protección de tema)"
echo "   - k: channel key (clave del canal)"
echo "   - l: user limit (límite de usuarios)"
echo "   - o: operator privileges (privilegios de operador)"
echo ""
echo "💡 Ejemplos de concatenación válidos:"
echo "   MODE #canal +it          → Activa invite-only y topic protection"
echo "   MODE #canal +kl clave 50 → Pone clave y límite de usuarios"
echo "   MODE #canal +t-i         → Activa topic protection, desactiva invite-only"
echo "   MODE #canal +itk-l clave → Configuración compleja"
