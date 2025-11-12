# 🤖 ServerBot - Asistente Virtual IRC

## ¿Qué es ServerBot?

ServerBot es un bot integrado en el servidor IRC que proporciona funcionalidades básicas para mejorar la experiencia de los usuarios.

## 🚀 Funcionalidades

### ✨ Saludo Automático
- Saluda automáticamente a todos los usuarios cuando se registran en el servidor
- Envía mensajes de bienvenida personalizados con el nick del usuario
- Proporciona información básica sobre cómo interactuar con el bot

### 🎯 Comandos Disponibles

El bot responde a mensajes privados. Usa: `PRIVMSG ServerBot :comando`

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `help` | `ayuda` | Muestra la lista de comandos disponibles |
| `info` | - | Información sobre el servidor |
| `time` | `hora` | Muestra la hora actual del servidor |
| `users` | `usuarios` | Número de usuarios conectados |
| `channels` | `canales` | Número de canales activos |

### 📖 Ejemplos de Uso

```irc
PRIVMSG ServerBot :help
PRIVMSG ServerBot :info  
PRIVMSG ServerBot :time
PRIVMSG ServerBot :users
PRIVMSG ServerBot :canales
```

## 🔧 Implementación Técnica

### Integración en el Servidor
- El bot está integrado directamente en la clase `Server`
- No requiere conexión separada (es un "cliente virtual")
- Se activa automáticamente cuando un usuario completa el registro

### Funciones Principales
- `botSendMessage()` - Envía mensajes desde el bot a un cliente
- `botWelcomeUser()` - Saluda a nuevos usuarios
- `botProcessCommand()` - Procesa comandos enviados al bot

### Flujo de Funcionamiento

1. **Registro de Usuario**:
   ```
   Usuario se registra → sendWelcome() → botWelcomeUser()
   ```

2. **Comandos al Bot**:
   ```
   PRIVMSG ServerBot :comando → botProcessCommand() → respuesta automática
   ```

## 🎨 Características

- ✅ **Emojis**: Utiliza emojis para hacer las respuestas más amigables
- ✅ **Multiidioma**: Soporta comandos en español e inglés
- ✅ **Información en tiempo real**: Estadísticas actualizadas del servidor
- ✅ **Integración transparente**: No interfiere con el funcionamiento normal del IRC
- ✅ **Protección QUIT**: Incluye las mismas protecciones que otros comandos

## 🔮 Posibles Extensiones

El bot puede extenderse fácilmente para incluir:
- Más comandos informativos
- Funciones de moderación básica
- Recordatorios programados
- Estadísticas más detalladas
- Juegos simples

## 🚀 ¡A Probar!

1. Compila e inicia el servidor: `make && ./ircserv 6667 password`
2. Conéctate con tu cliente IRC favorito
3. Una vez registrado, ¡el bot te saludará automáticamente!
4. Envía `PRIVMSG ServerBot :help` para explorar los comandos

¡Disfruta de tu nuevo asistente virtual! 🎉
