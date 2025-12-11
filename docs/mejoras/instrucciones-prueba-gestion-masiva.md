# Instrucciones de Prueba - Gestión Masiva de Códigos

## 🎯 Funcionalidades Implementadas

### 1. **Importación Masiva de Códigos**
- **Ruta**: `/codes/bulk-import`
- **Método**: POST
- **Parámetros**: `defaultKeyboard`, `defaultRelay`, `csvFile`

### 2. **Lectura de Tags en Tiempo Real**
- **Rutas**:
  - `/codes/start-tag-reading` (POST)
  - `/codes/stop-tag-reading` (POST)
  - `/codes/read-tags-status` (GET)
  - `/codes/export-read-tags` (GET)

## 🧪 Pruebas de Importación Masiva

### Paso 1: Preparar Archivo CSV
Usar el archivo de ejemplo: `docs/ejemplos/codigos_ejemplo.csv`

```csv
Tipo,Codigo,Teclado,Rele,Fecha_Creacion
PIN,1234,1,1,2024-01-15T10:30:00
TAG,12345678,1,1,2024-01-15T10:31:00
PIN,5678,2,2,2024-01-15T10:32:00
TAG,87654321,2,2,2024-01-15T10:33:00
PIN,9999,0,1,2024-01-15T10:34:00
TAG,ABCD1234,0,2,2024-01-15T10:35:00
```

### Paso 2: Acceder a la Página de Códigos
1. Abrir navegador web
2. Ir a `http://[IP_DISPOSITIVO]/codes`
3. Autenticarse con credenciales (admin/admin)

### Paso 3: Probar Importación Masiva
1. **Configurar valores por defecto**:
   - Teclado por defecto: "Ambos teclados"
   - Relé por defecto: "Relé 1"

2. **Seleccionar archivo CSV**:
   - Hacer clic en "Seleccionar archivo"
   - Elegir `codigos_ejemplo.csv`

3. **Importar**:
   - Hacer clic en "Importar Códigos"
   - Verificar resultado de importación

### Paso 4: Verificar Resultados
1. **Revisar página de códigos**:
   - Verificar que se añadieron 6 códigos
   - Comprobar configuración de teclado y relé

2. **Probar códigos**:
   - Probar PIN 1234 en teclado 1 → debe activar relé 1
   - Probar TAG 12345678 en teclado 1 → debe activar relé 1
   - Probar PIN 5678 en teclado 2 → debe activar relé 2

## 🧪 Pruebas de Lectura de Tags en Tiempo Real

### Paso 1: Configurar Lectura
1. **Ir a la sección "Lectura de Tags en Tiempo Real"**
2. **Configurar accesos**:
   - Teclado 1 → Relé 1: "Relé 1"
   - Teclado 1 → Relé 2: "Deshabilitado"
   - Teclado 2 → Relé 1: "Relé 1"
   - Teclado 2 → Relé 2: "Deshabilitado"
   - ✅ Marcar "Guardar en memoria"

### Paso 2: Iniciar Lectura
1. **Hacer clic en "Iniciar Lectura"**
2. **Verificar estado**:
   - Estado debe cambiar a "Activo"
   - Botón "Iniciar Lectura" debe deshabilitarse
   - Botón "Parar Lectura" debe habilitarse
   - Configuración debe atenuarse

### Paso 3: Leer Tags
1. **Pasar tags por los teclados**:
   - Usar diferentes tags RFID/NFC
   - Pasar por teclado 1 y teclado 2
   - Verificar que aparecen en la lista en tiempo real

2. **Verificar comportamiento**:
   - Tags deben aparecer en la lista inmediatamente
   - Contador debe incrementarse
   - Tags duplicados deben ignorarse

### Paso 4: Parar Lectura
1. **Hacer clic en "Parar Lectura"**
2. **Verificar estado**:
   - Estado debe cambiar a "Finalizado"
   - Botón "Exportar CSV" debe habilitarse
   - Configuración debe volver a normal

### Paso 5: Exportar Resultados
1. **Hacer clic en "Exportar CSV"**
2. **Verificar archivo descargado**:
   - Debe contener todos los tags leídos
   - Debe incluir configuración aplicada
   - Debe indicar si se guardó en memoria

### Paso 6: Verificar Almacenamiento
1. **Revisar códigos almacenados**:
   - Los tags leídos deben aparecer en la lista de códigos
   - Deben tener la configuración correcta (teclado/relé)

## 🔍 Casos de Prueba Específicos

### Caso 1: Importación con Configuración por Defecto
**Archivo CSV** (solo tipo y código):
```csv
Tipo,Codigo
PIN,1111
TAG,AAAA1111
```

**Configuración**:
- Teclado por defecto: "Teclado 1"
- Relé por defecto: "Relé 2"

**Resultado esperado**:
- PIN 1111 → Teclado 1, Relé 2
- TAG AAAA1111 → Teclado 1, Relé 2

### Caso 2: Lectura Solo para Exportación
**Configuración**:
- Teclado 1 → Relé 1: "Relé 1"
- ❌ NO marcar "Guardar en memoria"

**Resultado esperado**:
- Tags se leen y aparecen en lista
- NO se guardan en memoria
- CSV se genera correctamente

### Caso 3: Configuración Múltiple
**Configuración**:
- Teclado 1 → Relé 1: "Relé 1"
- Teclado 1 → Relé 2: "Relé 2"
- Teclado 2 → Relé 1: "Relé 1"
- ✅ Marcar "Guardar en memoria"

**Resultado esperado**:
- Cada tag se guarda con 3 configuraciones
- CSV muestra todas las configuraciones aplicadas

## ⚠️ Validaciones a Probar

### Importación Masiva
- [ ] Archivo CSV inválido → Error apropiado
- [ ] PIN con longitud incorrecta → Error específico
- [ ] TAG con longitud incorrecta → Error específico
- [ ] Teclado inválido → Error específico
- [ ] Relé inválido → Error específico
- [ ] Código duplicado → Error específico

### Lectura de Tags
- [ ] Configuración sin accesos → Error de validación
- [ ] Tag duplicado → Ignorar y reportar
- [ ] Lectura sin tags → Lista vacía
- [ ] Interrupción de lectura → Estado correcto

## 📊 Métricas de Rendimiento

### Importación Masiva
- **Tiempo esperado**: ~5 segundos para 50 códigos
- **Memoria**: ~50 bytes por código
- **Validación**: Inmediata por línea

### Lectura de Tags
- **Latencia**: <100ms para detección
- **Actualización UI**: 1 segundo
- **Memoria**: ~30 bytes por tag leído

## 🐛 Problemas Conocidos

### Limitaciones
1. **Tamaño de archivo CSV**: Limitado por memoria disponible
2. **Tags en lectura**: Máximo 1000 (configurable)
3. **Polling**: Cada segundo (no configurable)

### Soluciones
1. **Archivos grandes**: Dividir en lotes más pequeños
2. **Muchos tags**: Parar y exportar periódicamente
3. **Polling lento**: Aceptable para uso normal

## ✅ Criterios de Éxito

### Importación Masiva
- [ ] Importa PINs y TAGS correctamente
- [ ] Aplica configuración por defecto
- [ ] Valida formato y duplicados
- [ ] Genera reporte detallado
- [ ] Maneja errores apropiadamente

### Lectura de Tags
- [ ] Detecta tags en tiempo real
- [ ] Aplica configuración múltiple
- [ ] Actualiza interfaz automáticamente
- [ ] Genera CSV con configuración
- [ ] Guarda en memoria opcionalmente

## 🚀 Próximos Pasos

1. **Probar todas las funcionalidades**
2. **Verificar casos edge**
3. **Optimizar rendimiento si es necesario**
4. **Documentar problemas encontrados**
5. **Preparar para producción**
