# Análisis del Problema Actual - Sistema de Códigos

## Descripción del Problema

### 🚨 Problema Identificado
El sistema actual de almacenamiento de códigos **NO distingue entre teclados**, lo que es un error crítico para un sistema dual que puede gestionar **dos puertas independientes**.

### 📊 Estructura Actual (PROBLEMÁTICA)

#### Estructura de Código Actual
```cpp
struct CodeEntry {
  char type[5];      // "PIN" o "TAG"
  char value[17];    // Valor del código
  uint8_t relay;     // Relé a activar (1 o 2)
};
```

#### Problemas Identificados
1. **Falta `keyboard_id`**: No se especifica qué teclado puede usar el código
2. **Códigos universales**: Un código funciona en ambos teclados
3. **Control de puertas**: No se puede controlar puertas independientes
4. **Seguridad**: Un código válido abre cualquier puerta

### 🔍 Flujo Actual Problemático

#### Validación Actual
```cpp
void validateCode(const String& code, const String& type, int keyboardId) {
  // keyboardId se recibe pero NO se usa para validación
  int relayToActivate = 1;
  bool localFound = isCodeStored(type.c_str(), code.c_str(), &relayToActivate);
  
  if (localFound) {
    // ❌ PROBLEMA: Activa el relé sin verificar si el código es válido para este teclado
    controlReleWithDuration(releDuration, relayToActivate);
  }
}
```

#### Búsqueda Actual
```cpp
bool isCodeStored(const char* type, const char* value, int* relay) {
  for (int i = 0; i < storedCodes.count; i++) {
    if (strcmp(storedCodes.codes[i].type, type) == 0 &&
        strcmp(storedCodes.codes[i].value, value) == 0) {
      // ❌ PROBLEMA: Solo busca por tipo y valor, ignora teclado
      if (relay != nullptr) *relay = storedCodes.codes[i].relay;
      return true;
    }
  }
  return false;
}
```

## Escenarios Problemáticos

### 🚪 Escenario 1: Dos Puertas Independientes
- **Puerta 1**: Teclado 1 → Relé 1
- **Puerta 2**: Teclado 2 → Relé 2
- **Problema**: Código "1234" abre ambas puertas

### 🔐 Escenario 2: Control de Acceso Específico
- **Empleados**: Solo pueden acceder por Teclado 1
- **Visitantes**: Solo pueden acceder por Teclado 2
- **Problema**: No se puede implementar esta restricción

### 🏢 Escenario 3: Zonas de Seguridad
- **Zona A**: Teclado 1 (Alta seguridad)
- **Zona B**: Teclado 2 (Baja seguridad)
- **Problema**: Códigos de baja seguridad abren zona de alta seguridad

## Impacto en el Sistema

### 🔒 Seguridad
- **Vulnerabilidad**: Códigos válidos en teclados no autorizados
- **Control**: Imposible restringir acceso por teclado
- **Auditoría**: No se puede rastrear qué teclado se usó

### ⚙️ Funcionalidad
- **Puertas independientes**: No se pueden controlar por separado
- **Permisos granulares**: No se pueden implementar
- **Escalabilidad**: Limitado para sistemas complejos

### 📊 Gestión
- **Administración**: Confusa para usuarios
- **Configuración**: No se puede especificar restricciones
- **Monitoreo**: Información incompleta

## Casos de Uso Reales Afectados

### 🏭 Instalación Industrial
- **Entrada principal**: Teclado 1
- **Entrada de servicio**: Teclado 2
- **Problema**: Personal de servicio puede acceder por entrada principal

### 🏢 Oficina Corporativa
- **Recepción**: Teclado 1
- **Estacionamiento**: Teclado 2
- **Problema**: Empleados pueden acceder por recepción con códigos de estacionamiento

### 🏠 Residencial
- **Puerta principal**: Teclado 1
- **Puerta trasera**: Teclado 2
- **Problema**: Códigos de puerta trasera abren puerta principal

## Conclusión

### ❌ Estado Actual
El sistema actual es **inadecuado** para un controlador dual que debe gestionar **dos puertas independientes** con **restricciones específicas por teclado**.

### ✅ Necesidad de Mejora
Se requiere una **reestructuración completa** del sistema de almacenamiento de códigos para incluir:
1. **Identificación del teclado** (`keyboard_id`)
2. **Validación por teclado** en la búsqueda
3. **Control granular** de acceso
4. **Compatibilidad** con sistemas existentes

---

**Fecha de análisis**: Junio 2025  
**Prioridad**: CRÍTICA  
**Impacto**: ALTO  
**Esfuerzo**: MEDIO
