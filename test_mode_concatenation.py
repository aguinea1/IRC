#!/usr/bin/env python3
"""
Script de prueba para verificar la concatenación de modos en el servidor IRC
"""

import socket
import time

def send_command(sock, command):
    """Envía un comando al servidor IRC"""
    print(f">>> {command}")
    sock.send((command + "\r\n").encode())
    time.sleep(0.1)

def receive_response(sock):
    """Recibe respuesta del servidor"""
    try:
        response = sock.recv(4096).decode()
        if response:
            print(f"<<< {response.strip()}")
        return response
    except:
        return ""

def test_mode_concatenation():
    """Prueba la concatenación de modos"""
    
    # Conectar al servidor
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        sock.connect(('127.0.0.1', 6667))  # Ajusta el puerto si es necesario
        print("Conectado al servidor IRC")
        
        # Registro inicial
        send_command(sock, "PASS password")  # Ajusta la contraseña
        receive_response(sock)
        
        send_command(sock, "NICK testuser")
        receive_response(sock)
        
        send_command(sock, "USER testuser 0 * :Test User")
        receive_response(sock)
        
        time.sleep(1)
        
        # Crear/unirse a un canal
        send_command(sock, "JOIN #testchan")
        receive_response(sock)
        
        time.sleep(1)
        
        print("\n=== PRUEBAS DE CONCATENACIÓN DE MODOS ===\n")
        
        # Prueba 1: Concatenar múltiples modos simples
        print("1. Probando concatenación de modos simples: +it")
        send_command(sock, "MODE #testchan +it")
        receive_response(sock)
        
        # Verificar estado
        send_command(sock, "MODE #testchan")
        receive_response(sock)
        
        time.sleep(1)
        
        # Prueba 2: Mezclar + y - en la misma línea
        print("\n2. Probando mezcla de + y -: +t-i")
        send_command(sock, "MODE #testchan +t-i")
        receive_response(sock)
        
        send_command(sock, "MODE #testchan")
        receive_response(sock)
        
        time.sleep(1)
        
        # Prueba 3: Modos con parámetros concatenados
        print("\n3. Probando modos con parámetros: +kl clave123 50")
        send_command(sock, "MODE #testchan +kl clave123 50")
        receive_response(sock)
        
        send_command(sock, "MODE #testchan")
        receive_response(sock)
        
        time.sleep(1)
        
        # Prueba 4: Concatenación compleja
        print("\n4. Prueba compleja: +itk-l clave456")
        send_command(sock, "MODE #testchan +itk-l clave456")
        receive_response(sock)
        
        send_command(sock, "MODE #testchan")
        receive_response(sock)
        
        time.sleep(1)
        
        # Limpieza
        send_command(sock, "PART #testchan")
        receive_response(sock)
        
        send_command(sock, "QUIT :Test completed")
        receive_response(sock)
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("\nDesconectado del servidor")

if __name__ == "__main__":
    print("Iniciando pruebas de concatenación de modos...")
    print("Asegúrate de que el servidor IRC esté ejecutándose en localhost:6667")
    print("Presiona Ctrl+C para cancelar\n")
    
    try:
        test_mode_concatenation()
    except KeyboardInterrupt:
        print("\nPrueba cancelada por el usuario")
    except Exception as e:
        print(f"Error en la prueba: {e}")
