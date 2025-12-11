# Diagrama de Pines - KC868A2

## Descripción
Diagrama completo de pines y conexiones del sistema KC868A2, incluyendo configuración de hardware y esquemas de conexión.

## Configuración de Pines

### ⚡ Relés
- **Relé 1**: GPIO 15
- **Relé 2**: GPIO 2
- **Voltaje**: 5V/12V (configurable)
- **Corriente**: 10A máximo

### 🔐 Teclados Wiegand

#### Teclado 1 (Principal)
- **D0**: GPIO 33
- **D1**: GPIO 14
- **Alimentación**: 12V DC
- **Protocolo**: Wiegand 26/34 bits

#### Teclado 2 (Secundario)
- **D0**: GPIO 4
- **D1**: GPIO 16
- **Alimentación**: 12V DC
- **Protocolo**: Wiegand 26/34 bits

### 🌐 Ethernet
- **PHY**: LAN8720
- **MDC**: GPIO 23
- **MDIO**: GPIO 18
- **CLK**: GPIO 17
- **PWR**: GPIO 5
- **Velocidad**: 10/100 Mbps

### 📡 RS485
- **RX**: GPIO 35
- **TX**: GPIO 32
- **Baudrate**: 9600
- **Protocolo**: Modbus RTU

## Esquema de Conexiones

### 🔌 Conectores Principales
- **J1**: Alimentación 12V DC
- **J2**: Ethernet RJ45
- **J3**: Teclado Wiegand 1
- **J4**: Teclado Wiegand 2
- **J5**: Relés (NO/NC)
- **J6**: RS485 (A/B)

### ⚡ Alimentación
- **Voltaje**: 12V DC
- **Corriente**: 2A máximo
- **Protección**: Fusible 2A
- **LED**: Indicador de alimentación

---

**Última actualización**: Junio 2025  
**Versión**: 1.0  
**Compatibilidad**: Firmware 1.7.0+
