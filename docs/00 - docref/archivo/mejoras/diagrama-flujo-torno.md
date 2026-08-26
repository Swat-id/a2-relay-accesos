# 🔄 Diagrama de Flujo - Modo Torno

## **Flujo Principal del Modo Torno**

```
┌─────────────────────────────────────────────────────────────────┐
│                    MODO TORNO - FLUJO PRINCIPAL                │
└─────────────────────────────────────────────────────────────────┘

┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   TECLADO 1 │    │   TECLADO 2 │    │   RELÉ 1    │    │   RELÉ 2    │
│ (GPIO 33/14)│    │ (GPIO 4/16) │    │             │    │             │
└──────┬──────┘    └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
       │                  │                  │                  │
       │ Código recibido  │ Código recibido  │                  │
       ▼                  ▼                  │                  │
┌─────────────────────────────────────────────────────────────────┐
│                VALIDACIÓN LOCAL                                │
│  • Verificar código en EEPROM                                 │
│  • Determinar relé según teclado origen                       │
│  • Guardar solicitud pendiente                                │
└─────────────────────────────────────────────────────────────────┘
       │                  │
       │ Válido           │ Válido
       ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│              ENVÍO MQTT (SIEMPRE RELÉ 1)                      │
│  • Mensaje: "Código 1234, Teclado X, Relé 1"                 │
│  • Esperar respuesta del servidor remoto                      │
└─────────────────────────────────────────────────────────────────┘
       │                  │
       │ Respuesta        │ Respuesta
       ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│              PROCESAMIENTO DE RESPUESTA                        │
│  • Si APROBADO: Abrir relé según teclado origen               │
│  • Si DENEGADO: Bloquear acceso                               │
│  • Si TIMEOUT: Abrir relé según teclado origen                │
└─────────────────────────────────────────────────────────────────┘
       │                  │
       │ Abrir Relé 1     │ Abrir Relé 2
       ▼                  ▼
┌─────────────┐    ┌─────────────┐
│   RELÉ 1    │    │   RELÉ 2    │
│   ACTIVADO  │    │   ACTIVADO  │
└─────────────┘    └─────────────┘
```

## **Configuración del Modo Torno**

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONFIGURACIÓN TORNO                         │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   MODO NORMAL   │    │   MODO TORNO    │    │   CONFIGURACIÓN │
│                 │    │                 │    │                 │
│ • Código → Relé │    │ • Teclado → Relé│    │ • Teclado 1 → ? │
│ • MQTT específico│    │ • MQTT genérico │    │ • Teclado 2 → ? │
│ • Flexibilidad  │    │ • Control fijo  │    │ • Persistencia  │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## **Gestión de Solicitudes Pendientes**

```
┌─────────────────────────────────────────────────────────────────┐
│              GESTIÓN DE SOLICITUDES PENDIENTES                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   SOLICITUD │    │   TIMEOUT   │    │   RESPUESTA │    │   LIMPIEZA  │
│   ACTIVA    │    │   (30s)     │    │   MQTT      │    │   AUTOMÁTICA│
└──────┬──────┘    └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
       │                  │                  │                  │
       │ Guardar:         │ Si no hay        │ Procesar:        │ Limpiar:
       │ • Teclado origen │ respuesta        │ • Aprobar        │ • Solicitud
       │ • Código         │ • Abrir relé     │ • Denegar        │ • Timestamp
       │ • Relé destino   │ • Según origen   │ • Según origen   │ • Estado
       ▼                  ▼                  ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                    ESTRUCTURA DE DATOS                         │
│  struct PendingRequest {                                       │
│    bool active;                                                │
│    uint8_t keyboard_id;                                        │
│    char code[17];                                              │
│    char type[5];                                               │
│    unsigned long timestamp;                                    │
│    uint8_t relay_to_open;                                      │
│  };                                                            │
└─────────────────────────────────────────────────────────────────┘
```

## **Comunicación MQTT - Modo Torno**

```
┌─────────────────────────────────────────────────────────────────┐
│                    COMUNICACIÓN MQTT                           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   SOLICITUD     │    │   RESPUESTA     │    │   LOGGING       │
│   (Siempre      │    │   (Procesar     │    │   (Registro     │
│    Relé 1)      │    │    según origen)│    │    de eventos)  │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          │ Enviar:              │ Recibir:             │ Registrar:
          │ • Código             │ • APPROVED           │ • Solicitud
          │ • Teclado origen     │ • DENIED             │ • Respuesta
          │ • Relé 1 (fijo)      │ • Timeout            │ • Apertura
          │ • Modo torno         │ • Error              │ • Errores
          ▼                      ▼                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                    FORMATO DE MENSAJE                          │
│  {                                                             │
│    "device": "KC868A2-001",                                   │
│    "timestamp": 1703123456,                                   │
│    "type": "access_request",                                  │
│    "data": {                                                  │
│      "code": "1234",                                          │
│      "code_type": "PIN",                                      │
│      "keyboard_id": 1,                                        │
│      "relay": 1,                                              │
│      "mode": "turnstile"                                      │
│    }                                                           │
│  }                                                             │
└─────────────────────────────────────────────────────────────────┘
```

## **Estados del Sistema**

```
┌─────────────────────────────────────────────────────────────────┐
│                        ESTADOS DEL SISTEMA                     │
└─────────────────────────────────────────────────────────────────┘

┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   INICIO    │    │   NORMAL    │    │   TORNO     │    │   PENDIENTE │
│             │    │             │    │             │    │             │
│ • Cargar    │    │ • Código →  │    │ • Teclado → │    │ • Esperando │
│   config    │    │   Relé      │    │   Relé      │    │   respuesta │
│ • Verificar │    │ • MQTT      │    │ • MQTT      │    │ • Timeout   │
│   modo      │    │   específico│    │   genérico  │    │   activo    │
└──────┬──────┘    └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
       │                  │                  │                  │
       │ Config = Normal  │ Config = Torno   │ Solicitud        │ Respuesta
       ▼                  ▼                  ▼                  ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   NORMAL    │    │   TORNO     │    │   PENDIENTE │    │   TORNO     │
│   ACTIVO    │    │   ACTIVO    │    │   ACTIVO    │    │   ACTIVO    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

## **Casos de Uso Específicos**

### **Caso 1: Control de Entrada/Salida**
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   TECLADO 1 │    │   TECLADO 2 │    │   RELÉ 1    │    │   RELÉ 2    │
│  (ENTRADA)  │    │   (SALIDA)  │    │  (ENTRADA)  │    │  (SALIDA)   │
└──────┬──────┘    └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
       │                  │                  │                  │
       │ Código 1234      │ Código 1234      │                  │
       ▼                  ▼                  │                  │
┌─────────────────────────────────────────────────────────────────┐
│              CONFIGURACIÓN TORNO                               │
│  • Teclado 1 → Relé 1 (Entrada)                               │
│  • Teclado 2 → Relé 2 (Salida)                                │
│  • MQTT: Siempre relé 1                                       │
└─────────────────────────────────────────────────────────────────┘
       │                  │
       │ Abrir Relé 1     │ Abrir Relé 2
       ▼                  ▼
┌─────────────┐    ┌─────────────┐
│   ENTRADA   │    │   SALIDA    │
│   ABIERTA   │    │   ABIERTA   │
└─────────────┘    └─────────────┘
```

### **Caso 2: Control de Áreas**
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   TECLADO 1 │    │   TECLADO 2 │    │   RELÉ 1    │    │   RELÉ 2    │
│  (ÁREA A)   │    │  (ÁREA B)   │    │  (ÁREA A)   │    │  (ÁREA B)   │
└──────┬──────┘    └──────┬──────┘    └──────┬──────┘    └──────┬──────┘
       │                  │                  │                  │
       │ Código 5678      │ Código 5678      │                  │
       ▼                  ▼                  │                  │
┌─────────────────────────────────────────────────────────────────┐
│              CONFIGURACIÓN TORNO                               │
│  • Teclado 1 → Relé 1 (Área A)                                │
│  • Teclado 2 → Relé 2 (Área B)                                │
│  • MQTT: Siempre relé 1                                       │
└─────────────────────────────────────────────────────────────────┘
       │                  │
       │ Abrir Relé 1     │ Abrir Relé 2
       ▼                  ▼
┌─────────────┐    ┌─────────────┐
│   ÁREA A    │    │   ÁREA B    │
│   ABIERTA   │    │   ABIERTA   │
└─────────────┘    └─────────────┘
```

---

**📅 Fecha**: $(date)
**🔧 Estado**: Diagramas completados
**📋 Próximo Paso**: Implementación de estructuras de datos
