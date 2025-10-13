#!/usr/bin/env python3
"""
Cliente de prueba simple para el servidor IRC
"""

import socket
import time
import threading

class IRCClient:
    def __init__(self, host='localhost', port=6667):
        self.host = host
        self.port = port
        self.socket = None
        self.nick = None
        
    def connect(self):
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            print(f"Conectado a {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Error conectando: {e}")
            return False
    
    def send(self, message):
        if self.socket:
            self.socket.send((message + '\r\n').encode())
            print(f"Enviado: {message}")
    
    def receive(self):
        if self.socket:
            try:
                data = self.socket.recv(1024).decode()
                if data:
                    print(f"Recibido: {data.strip()}")
                return data
            except:
                return None
    
    def register(self, password, nick, username, realname):
        self.nick = nick
        self.send(f"PASS {password}")
        time.sleep(0.1)
        self.send(f"NICK {nick}")
        time.sleep(0.1)
        self.send(f"USER {username} 0 * :{realname}")
        time.sleep(0.5)
    
    def join_channel(self, channel):
        self.send(f"JOIN {channel}")
        time.sleep(0.5)
    
    def send_message(self, target, message):
        self.send(f"PRIVMSG {target} :{message}")
        time.sleep(0.5)
    
    def set_topic(self, channel, topic):
        self.send(f"TOPIC {channel} :{topic}")
        time.sleep(0.5)
    
    def get_topic(self, channel):
        self.send(f"TOPIC {channel}")
        time.sleep(0.5)
    
    def get_names(self, channel=None):
        if channel:
            self.send(f"NAMES {channel}")
        else:
            self.send("NAMES")
        time.sleep(0.5)
    
    def list_channels(self):
        self.send("LIST")
        time.sleep(0.5)
    
    def get_modes(self, channel):
        self.send(f"MODE {channel}")
        time.sleep(0.5)
    
    def part_channel(self, channel, reason="Leaving"):
        self.send(f"PART {channel} :{reason}")
        time.sleep(0.5)
    
    def quit(self, message="Goodbye"):
        self.send(f"QUIT :{message}")
        time.sleep(0.5)
    
    def close(self):
        if self.socket:
            self.socket.close()

def test_channels():
    print("=== PRUEBA DE CANALES IRC ===\n")
    
    # Cliente 1 (Alice)
    print("1. Creando cliente Alice...")
    alice = IRCClient()
    if not alice.connect():
        return
    
    alice.register("testpass", "alice", "alice", "Alice User")
    
    # Cliente 2 (Bob)
    print("\n2. Creando cliente Bob...")
    bob = IRCClient()
    if not bob.connect():
        alice.close()
        return
    
    bob.register("testpass", "bob", "bob", "Bob User")
    
    print("\n3. Alice se une al canal #test...")
    alice.join_channel("#test")
    
    print("\n4. Bob se une al canal #test...")
    bob.join_channel("#test")
    
    print("\n5. Alice envía mensaje al canal...")
    alice.send_message("#test", "Hola a todos!")
    
    print("\n6. Bob responde...")
    bob.send_message("#test", "Hola Alice!")
    
    print("\n7. Alice establece tópico...")
    alice.set_topic("#test", "Canal de prueba para IRC")
    
    print("\n8. Bob consulta tópico...")
    bob.get_topic("#test")
    
    print("\n9. Alice consulta usuarios...")
    alice.get_names("#test")
    
    print("\n10. Bob lista canales...")
    bob.list_channels()
    
    print("\n11. Alice consulta modos...")
    alice.get_modes("#test")
    
    print("\n12. Bob sale del canal...")
    bob.part_channel("#test", "Adiós!")
    
    print("\n13. Cerrando conexiones...")
    alice.quit("Test completado")
    bob.quit("Test completado")
    
    alice.close()
    bob.close()
    
    print("\n=== PRUEBA COMPLETADA ===")

if __name__ == "__main__":
    test_channels()
