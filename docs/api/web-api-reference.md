# API Web - Referencia Completa KC868A2

## Descripción
Referencia completa de la API web del sistema KC868A2, incluyendo todas las rutas, parámetros, respuestas y ejemplos de uso.

## Configuración de Acceso

### 🌐 Parámetros de Conexión
- **Protocolo**: HTTP
- **Puerto**: 80
- **Autenticación**: HTTP Basic Authentication
- **Usuario por defecto**: admin
- **Contraseña por defecto**: admin

### 🔐 Autenticación
```bash
# Acceso básico
curl -u admin:admin http://[IP_DISPOSITIVO]/

# Con headers explícitos
curl -H "Authorization: Basic YWRtaW46YWRtaW4=" http://[IP_DISPOSITIVO]/
```

## Rutas de la API

### 📊 Página Principal
**Ruta**: `/`
**Método**: GET
**Descripción**: Página principal con estado del sistema y configuración

**Respuesta**: HTML completo con:
- Estado de seguridad
- Estado de teclados duales
- Formulario de configuración
- Acciones rápidas
- Historial de accesos
- Estado de conexión

**Ejemplo**:
```bash
curl -u admin:admin http://192.168.1.100/
```

### 💾 Guardar Configuración
**Ruta**: `/save`
**Método**: POST
**Descripción**: Guardar configuración del dispositivo

**Parámetros**:
- `deviceName` (string): Nombre del dispositivo
- `releDuration` (float): Duración del relé en segundos
- `useDhcp` (string): "1" para DHCP, "0" para IP estática
- `staticIp` (string): IP estática (si useDhcp="0")
- `staticGateway` (string): Puerta de enlace (si useDhcp="0")
- `staticSubnet` (string): Máscara de subred (si useDhcp="0")
- `staticDns` (string): DNS (si useDhcp="0")

**Ejemplo**:
```bash
curl -u admin:admin -X POST \
  -d "deviceName=MiDispositivo" \
  -d "releDuration=3.0" \
  -d "useDhcp=1" \
  http://192.168.1.100/save
```

**Respuesta**: Redirección a `/` (303)

### ⚡ Activación de Relé
**Ruta**: `/rele`
**Método**: GET
**Descripción**: Activar un relé específico

**Parámetros**:
- `relay` (int): Número del relé (1 o 2)

**Ejemplo**:
```bash
# Activar relé 1
curl -u admin:admin http://192.168.1.100/rele?relay=1

# Activar relé 2
curl -u admin:admin http://192.168.1.100/rele?relay=2
```

**Respuesta**: Redirección a `/` (303)

### 🔄 Reinicio del Sistema
**Ruta**: `/reboot`
**Método**: GET
**Descripción**: Reiniciar el sistema

**Ejemplo**:
```bash
curl -u admin:admin http://192.168.1.100/reboot
```

**Respuesta**: HTML con mensaje de reinicio y redirección automática

### 🔧 Reset a Valores por Defecto
**Ruta**: `/reset`
**Método**: GET
**Descripción**: Resetear configuración a valores por defecto

**Ejemplo**:
```bash
curl -u admin:admin http://192.168.1.100/reset
```

**Respuesta**: HTML con mensaje de reset y redirección automática

### 🔐 Cambio de Contraseña
**Ruta**: `/changepass`
**Método**: POST
**Descripción**: Cambiar contraseña de acceso web

**Parámetros**:
- `newPassword` (string): Nueva contraseña (4-31 caracteres)

**Ejemplo**:
```bash
curl -u admin:admin -X POST \
  -d "newPassword=nuevaContraseña123" \
  http://192.168.1.100/changepass
```

**Respuesta**: HTML con confirmación o error

### 📋 Gestión de Códigos
**Ruta**: `/codes`
**Método**: GET
**Descripción**: Página de gestión de códigos de acceso

**Respuesta**: HTML con:
- Estado de seguridad
- Información del sistema dual
- Modo de validación
- Formulario para añadir códigos
- Lista de códigos almacenados

**Ejemplo**:
```bash
curl -u admin:admin http://192.168.1.100/codes
```

### ➕ Añadir Código
**Ruta**: `/codes/add`
**Método**: POST
**Descripción**: Añadir un nuevo código de acceso

**Parámetros**:
- `type` (string): Tipo de código ("PIN" o "TAG")
- `value` (string): Valor del código
- `relay` (int): Relé a activar (1 o 2)

**Validaciones**:
- **PIN**: 4-6 dígitos numéricos
- **TAG**: 1-16 caracteres
- **Relé**: 1 o 2
- **Duplicados**: No permitidos

**Ejemplo**:
```bash
# Añadir PIN
curl -u admin:admin -X POST \
  -d "type=PIN" \
  -d "value=1234" \
  -d "relay=1" \
  http://192.168.1.100/codes/add

# Añadir TAG
curl -u admin:admin -X POST \
  -d "type=TAG" \
  -d "value=12345678" \
  -d "relay=2" \
  http://192.168.1.100/codes/add
```

**Respuesta**: Redirección a `/codes` (303) o error

### 🗑️ Eliminar Código
**Ruta**: `/codes/delete`
**Método**: GET
**Descripción**: Eliminar un código de acceso

**Parámetros**:
- `type` (string): Tipo de código ("PIN" o "TAG")
- `value` (string): Valor del código

**Ejemplo**:
```bash
curl -u admin:admin \
  "http://192.168.1.100/codes/delete?type=PIN&value=1234"
```

**Respuesta**: Redirección a `/codes` (303)

### ⚙️ Cambiar Modo de Validación
**Ruta**: `/codes/mode`
**Método**: POST
**Descripción**: Cambiar modo de validación (local/remoto primero)

**Parámetros**:
- `mode` (string): "local" o "remote"

**Ejemplo**:
```bash
# Modo local primero
curl -u admin:admin -X POST \
  -d "mode=local" \
  http://192.168.1.100/codes/mode

# Modo remoto primero
curl -u admin:admin -X POST \
  -d "mode=remote" \
  http://192.168.1.100/codes/mode
```

**Respuesta**: Redirección a `/codes` (303)

## Ejemplos de Uso Completos

### 🐍 Cliente Python
```python
import requests
from requests.auth import HTTPBasicAuth
import json

class KC868A2WebClient:
    def __init__(self, ip, username="admin", password="admin"):
        self.base_url = f"http://{ip}"
        self.auth = HTTPBasicAuth(username, password)
        self.session = requests.Session()
        self.session.auth = self.auth
        
    def get_status(self):
        """Obtener estado del sistema"""
        response = self.session.get(f"{self.base_url}/")
        return response.text
        
    def save_configuration(self, device_name, relay_duration, use_dhcp=True, 
                          static_ip=None, static_gateway=None, 
                          static_subnet=None, static_dns=None):
        """Guardar configuración del dispositivo"""
        data = {
            "deviceName": device_name,
            "releDuration": relay_duration,
            "useDhcp": "1" if use_dhcp else "0"
        }
        
        if not use_dhcp:
            data.update({
                "staticIp": static_ip,
                "staticGateway": static_gateway,
                "staticSubnet": static_subnet,
                "staticDns": static_dns
            })
            
        response = self.session.post(f"{self.base_url}/save", data=data)
        return response.status_code == 303
        
    def activate_relay(self, relay):
        """Activar un relé"""
        response = self.session.get(f"{self.base_url}/rele?relay={relay}")
        return response.status_code == 303
        
    def reboot_device(self):
        """Reiniciar el dispositivo"""
        response = self.session.get(f"{self.base_url}/reboot")
        return response.status_code == 200
        
    def reset_device(self):
        """Resetear el dispositivo"""
        response = self.session.get(f"{self.base_url}/reset")
        return response.status_code == 200
        
    def change_password(self, new_password):
        """Cambiar contraseña"""
        data = {"newPassword": new_password}
        response = self.session.post(f"{self.base_url}/changepass", data=data)
        return response.status_code == 200
        
    def get_codes(self):
        """Obtener página de códigos"""
        response = self.session.get(f"{self.base_url}/codes")
        return response.text
        
    def add_code(self, code_type, value, relay):
        """Añadir un código"""
        data = {
            "type": code_type,
            "value": value,
            "relay": relay
        }
        response = self.session.post(f"{self.base_url}/codes/add", data=data)
        return response.status_code == 303
        
    def delete_code(self, code_type, value):
        """Eliminar un código"""
        params = {"type": code_type, "value": value}
        response = self.session.get(f"{self.base_url}/codes/delete", params=params)
        return response.status_code == 303
        
    def set_validation_mode(self, mode):
        """Cambiar modo de validación"""
        data = {"mode": mode}
        response = self.session.post(f"{self.base_url}/codes/mode", data=data)
        return response.status_code == 303

# Uso
if __name__ == "__main__":
    client = KC868A2WebClient("192.168.1.100")
    
    # Obtener estado
    status = client.get_status()
    print("Estado del sistema obtenido")
    
    # Guardar configuración
    success = client.save_configuration("MiDispositivo", 3.0, use_dhcp=True)
    print(f"Configuración guardada: {success}")
    
    # Activar relé
    success = client.activate_relay(1)
    print(f"Relé 1 activado: {success}")
    
    # Añadir código
    success = client.add_code("PIN", "1234", 1)
    print(f"Código añadido: {success}")
    
    # Cambiar modo de validación
    success = client.set_validation_mode("local")
    print(f"Modo cambiado: {success}")
```

### 🟨 Cliente Node.js
```javascript
const axios = require('axios');

class KC868A2WebClient {
    constructor(ip, username = 'admin', password = 'admin') {
        this.baseUrl = `http://${ip}`;
        this.auth = {
            username: username,
            password: password
        };
    }
    
    async getStatus() {
        try {
            const response = await axios.get(`${this.baseUrl}/`, {
                auth: this.auth
            });
            return response.data;
        } catch (error) {
            console.error('Error obteniendo estado:', error.message);
            return null;
        }
    }
    
    async saveConfiguration(deviceName, relayDuration, useDhcp = true, 
                           staticIp = null, staticGateway = null, 
                           staticSubnet = null, staticDns = null) {
        try {
            const data = {
                deviceName: deviceName,
                releDuration: relayDuration,
                useDhcp: useDhcp ? '1' : '0'
            };
            
            if (!useDhcp) {
                data.staticIp = staticIp;
                data.staticGateway = staticGateway;
                data.staticSubnet = staticSubnet;
                data.staticDns = staticDns;
            }
            
            const response = await axios.post(`${this.baseUrl}/save`, data, {
                auth: this.auth,
                maxRedirects: 0,
                validateStatus: (status) => status === 303
            });
            
            return true;
        } catch (error) {
            console.error('Error guardando configuración:', error.message);
            return false;
        }
    }
    
    async activateRelay(relay) {
        try {
            const response = await axios.get(`${this.baseUrl}/rele?relay=${relay}`, {
                auth: this.auth,
                maxRedirects: 0,
                validateStatus: (status) => status === 303
            });
            
            return true;
        } catch (error) {
            console.error('Error activando relé:', error.message);
            return false;
        }
    }
    
    async rebootDevice() {
        try {
            const response = await axios.get(`${this.baseUrl}/reboot`, {
                auth: this.auth
            });
            
            return response.status === 200;
        } catch (error) {
            console.error('Error reiniciando dispositivo:', error.message);
            return false;
        }
    }
    
    async addCode(codeType, value, relay) {
        try {
            const data = {
                type: codeType,
                value: value,
                relay: relay
            };
            
            const response = await axios.post(`${this.baseUrl}/codes/add`, data, {
                auth: this.auth,
                maxRedirects: 0,
                validateStatus: (status) => status === 303
            });
            
            return true;
        } catch (error) {
            console.error('Error añadiendo código:', error.message);
            return false;
        }
    }
    
    async deleteCode(codeType, value) {
        try {
            const response = await axios.get(`${this.baseUrl}/codes/delete`, {
                auth: this.auth,
                params: {
                    type: codeType,
                    value: value
                },
                maxRedirects: 0,
                validateStatus: (status) => status === 303
            });
            
            return true;
        } catch (error) {
            console.error('Error eliminando código:', error.message);
            return false;
        }
    }
    
    async setValidationMode(mode) {
        try {
            const data = { mode: mode };
            
            const response = await axios.post(`${this.baseUrl}/codes/mode`, data, {
                auth: this.auth,
                maxRedirects: 0,
                validateStatus: (status) => status === 303
            });
            
            return true;
        } catch (error) {
            console.error('Error cambiando modo:', error.message);
            return false;
        }
    }
}

// Uso
async function main() {
    const client = new KC868A2WebClient('192.168.1.100');
    
    // Obtener estado
    const status = await client.getStatus();
    console.log('Estado del sistema obtenido');
    
    // Guardar configuración
    const configSaved = await client.saveConfiguration('MiDispositivo', 3.0, true);
    console.log(`Configuración guardada: ${configSaved}`);
    
    // Activar relé
    const relayActivated = await client.activateRelay(1);
    console.log(`Relé 1 activado: ${relayActivated}`);
    
    // Añadir código
    const codeAdded = await client.addCode('PIN', '1234', 1);
    console.log(`Código añadido: ${codeAdded}`);
    
    // Cambiar modo de validación
    const modeChanged = await client.setValidationMode('local');
    console.log(`Modo cambiado: ${modeChanged}`);
}

main().catch(console.error);
```

### 🔧 Cliente Bash
```bash
#!/bin/bash

# Configuración
IP="192.168.1.100"
USER="admin"
PASS="admin"
BASE_URL="http://${IP}"

# Función para hacer peticiones autenticadas
make_request() {
    local method=$1
    local url=$2
    local data=$3
    
    if [ "$method" = "GET" ]; then
        curl -s -u "${USER}:${PASS}" "${BASE_URL}${url}"
    elif [ "$method" = "POST" ]; then
        curl -s -u "${USER}:${PASS}" -X POST -d "${data}" "${BASE_URL}${url}"
    fi
}

# Obtener estado del sistema
get_status() {
    echo "Obteniendo estado del sistema..."
    make_request "GET" "/"
}

# Guardar configuración
save_config() {
    local device_name=$1
    local relay_duration=$2
    local use_dhcp=$3
    
    echo "Guardando configuración..."
    local data="deviceName=${device_name}&releDuration=${relay_duration}&useDhcp=${use_dhcp}"
    make_request "POST" "/save" "${data}"
}

# Activar relé
activate_relay() {
    local relay=$1
    echo "Activando relé ${relay}..."
    make_request "GET" "/rele?relay=${relay}"
}

# Reiniciar dispositivo
reboot_device() {
    echo "Reiniciando dispositivo..."
    make_request "GET" "/reboot"
}

# Añadir código
add_code() {
    local type=$1
    local value=$2
    local relay=$3
    
    echo "Añadiendo código ${type}:${value}..."
    local data="type=${type}&value=${value}&relay=${relay}"
    make_request "POST" "/codes/add" "${data}"
}

# Eliminar código
delete_code() {
    local type=$1
    local value=$2
    
    echo "Eliminando código ${type}:${value}..."
    make_request "GET" "/codes/delete?type=${type}&value=${value}"
}

# Cambiar modo de validación
set_validation_mode() {
    local mode=$1
    echo "Cambiando modo de validación a ${mode}..."
    local data="mode=${mode}"
    make_request "POST" "/codes/mode" "${data}"
}

# Ejemplos de uso
echo "=== Cliente Web KC868A2 ==="

# Obtener estado
get_status

# Guardar configuración
save_config "MiDispositivo" "3.0" "1"

# Activar relé
activate_relay 1

# Añadir código
add_code "PIN" "1234" "1"

# Cambiar modo de validación
set_validation_mode "local"

echo "=== Operaciones completadas ==="
```

## Códigos de Respuesta HTTP

### ✅ Respuestas Exitosas
- **200 OK**: Página cargada correctamente
- **303 See Other**: Redirección (operación exitosa)

### ❌ Respuestas de Error
- **400 Bad Request**: Parámetros inválidos
- **401 Unauthorized**: Autenticación requerida
- **404 Not Found**: Ruta no encontrada
- **500 Internal Server Error**: Error del servidor

## Validaciones y Restricciones

### 📝 Validaciones de Entrada
- **Longitud de campos**: Límites específicos
- **Formato de datos**: Validación de tipos
- **Rangos numéricos**: Valores mínimos y máximos
- **Caracteres permitidos**: Sanitización de entrada

### 🚫 Restricciones del Sistema
- **Autenticación**: Requerida para todas las operaciones
- **Sesión**: Persistente durante la navegación
- **Rate limiting**: No implementado (considerar para producción)
- **CORS**: No configurado (considerar para aplicaciones web)

## Mejores Prácticas

### 🔒 Seguridad
- **HTTPS**: Considerar para producción
- **Contraseñas**: Cambiar por defecto
- **Validación**: Verificar todos los inputs
- **Logging**: Registrar accesos y cambios

### ⚡ Rendimiento
- **Conexiones**: Reutilizar sesiones
- **Timeouts**: Configurar apropiadamente
- **Caché**: Considerar para datos estáticos
- **Compresión**: Habilitar si es posible

### 🛠️ Mantenimiento
- **Monitoreo**: Supervisar accesos
- **Backup**: Respaldo de configuraciones
- **Logs**: Mantener logs de acceso
- **Testing**: Pruebas regulares

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
