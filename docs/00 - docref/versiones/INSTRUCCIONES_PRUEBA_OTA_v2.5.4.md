# Instrucciones para Prueba de Actualización OTA v2.5.4

## 🎯 Objetivo

Probar la funcionalidad de **actualización Over-The-Air (OTA)** del firmware v2.5.4 a través de la interfaz web del dispositivo SWATID-A2.

---

## 📋 Requisitos Previos

### Hardware
- ✅ Dispositivo SWATID-A2 funcionando
- ✅ Conexión Ethernet establecida
- ✅ Acceso a la red local del dispositivo

### Software
- ✅ Navegador web moderno (Chrome, Firefox, Safari, Edge)
- ✅ Archivo de firmware: `SWATID-A2_v2.5.4.bin` (1.1 MB)
- ✅ Acceso al dispositivo vía IP local

### Información Necesaria
- **IP del dispositivo**: (ejemplo: `192.168.1.100`)
- **Usuario admin**: (por defecto: `admin`)
- **Contraseña**: (configurada en el dispositivo)

---

## 📝 Procedimiento de Prueba

### Paso 1: Verificar Versión Actual

1. **Abrir navegador web**
   ```
   http://[IP_DEL_DISPOSITIVO]/
   ```

2. **Autenticarse**
   - Usuario: `admin`
   - Contraseña: [tu_contraseña]

3. **Verificar versión actual en la página principal**
   - Busca en el encabezado: "Firmware: vX.X.X"
   - **Anota la versión actual**: _____________

**Resultado esperado**: Debería ver la versión anterior (v2.5.3 o menor)

---

### Paso 2: Acceder a la Página de Actualización OTA

1. **Navegar a la página OTA**
   - Opción A: Haz clic en el botón "Actualizar Firmware" en la página principal
   - Opción B: Accede directamente a: `http://[IP]/ota`

2. **Verificar que la página carga correctamente**
   - Debe mostrar el título: "Actualización de Firmware"
   - Debe tener un selector de archivo
   - Debe tener un botón "Subir y Actualizar"

**Resultado esperado**: Página OTA cargada sin errores

---

### Paso 3: Preparar el Archivo de Firmware

1. **Localizar el archivo**
   ```
   firmware/SWATID-A2_v2.5.4.bin
   ```

2. **Verificar integridad (opcional pero recomendado)**
   ```bash
   # Linux/Mac
   shasum -a 256 SWATID-A2_v2.5.4.bin
   
   # Debe mostrar:
   # 30f394dfa533685d5d3bd94425ed1879191c77a420b1c68f5ba6c7fc78922e4f
   ```

3. **Verificar tamaño del archivo**
   - Tamaño esperado: **1,190,816 bytes** (1.1 MB)

**Resultado esperado**: Archivo verificado y listo

---

### Paso 4: Iniciar la Actualización

1. **Seleccionar el archivo**
   - Haz clic en "Seleccionar archivo" o "Choose File"
   - Navega hasta `SWATID-A2_v2.5.4.bin`
   - Selecciona el archivo

2. **Verificar que el nombre del archivo aparece**
   - Debe mostrar: "SWATID-A2_v2.5.4.bin"

3. **Confirmar la actualización**
   - Haz clic en "Subir y Actualizar"
   - **Confirma** en el diálogo que aparece (si lo hay)

**Resultado esperado**: Comienza el proceso de subida

⚠️ **IMPORTANTE**: 
- **NO cierres el navegador**
- **NO actualices la página**
- **NO desconectes el dispositivo**

---

### Paso 5: Monitorear el Proceso

Durante la actualización, deberías ver:

1. **Barra de progreso** (0% → 100%)
   - Tiempo estimado: 30-60 segundos

2. **Mensajes de estado**
   - "Subiendo firmware..."
   - "Firmware recibido correctamente"
   - "Validando firmware..."
   - "Instalando actualización..."
   - "Actualización exitosa"

3. **Mensaje final**
   - "El dispositivo se reiniciará en 5 segundos..."

**Resultado esperado**: Progreso visible y mensajes de estado claros

---

### Paso 6: Reinicio del Dispositivo

1. **Esperar el reinicio automático**
   - Tiempo de reinicio: 10-15 segundos
   - El LED del dispositivo parpadeará durante el reinicio

2. **No intervenir durante el reinicio**
   - Dejar que el dispositivo complete el proceso

**Resultado esperado**: Dispositivo se reinicia automáticamente

---

### Paso 7: Verificación Post-Actualización

#### 7.1 Verificar Conectividad

1. **Esperar 30 segundos** después del reinicio

2. **Intentar acceder nuevamente**
   ```
   http://[IP_DEL_DISPOSITIVO]/
   ```

3. **Autenticarse nuevamente**

**Resultado esperado**: Dispositivo accesible y funcionando

#### 7.2 Verificar Versión del Firmware

1. **En la página principal, verificar el encabezado**
   - Debe mostrar: **"Firmware: v2.5.4"**

2. **Verificar la fecha de compilación**
   - Debe mostrar: "Oct 13 2025" o similar

**Resultado esperado**: ✅ Versión actualizada a v2.5.4

#### 7.3 Verificar Nuevas Funcionalidades

1. **Acceder a Códigos Remotos**
   ```
   http://[IP]/remote-codes
   ```
   - Verificar que los horarios se muestran como texto: "Lun-Vie", "Sáb-Dom", etc.

2. **Verificar Capacidad de Memoria**
   - En la página de códigos locales: debe mostrar "X/100"
   - En la página de códigos remotos: debe mostrar "X/100"

**Resultado esperado**: ✅ Nuevas funcionalidades visibles y funcionando

#### 7.4 Verificar Códigos Existentes

1. **Revisar códigos locales**
   ```
   http://[IP]/codes
   ```
   - Verificar que los códigos anteriores siguen ahí

2. **Revisar códigos remotos**
   ```
   http://[IP]/remote-codes
   ```
   - Verificar que los códigos remotos se mantienen

**Resultado esperado**: ✅ Todos los códigos se mantienen después de la actualización

---

## ✅ Lista de Verificación (Checklist)

### Antes de la Actualización
- [ ] Versión actual anotada: ___________
- [ ] Archivo `SWATID-A2_v2.5.4.bin` localizado
- [ ] Tamaño verificado: 1,190,816 bytes
- [ ] Checksum verificado (opcional): `30f394df...`
- [ ] Navegador web preparado
- [ ] IP del dispositivo confirmada: ___________

### Durante la Actualización
- [ ] Archivo seleccionado correctamente
- [ ] Proceso de subida iniciado
- [ ] Barra de progreso visible
- [ ] Sin errores durante la subida
- [ ] Mensaje de éxito mostrado
- [ ] Reinicio automático confirmado

### Después de la Actualización
- [ ] Dispositivo accesible vía web
- [ ] Versión mostrada: v2.5.4
- [ ] Horarios en texto legible: "Lun-Vie", etc.
- [ ] Capacidad mostrada: X/100 códigos
- [ ] Códigos locales mantenidos
- [ ] Códigos remotos mantenidos
- [ ] Funcionalidad MQTT operativa
- [ ] Relés funcionando correctamente

---

## 🐛 Problemas Comunes y Soluciones

### Problema 1: Error "Failed to fetch"
**Síntomas**: Mensaje de error durante la subida

**Soluciones**:
1. Verifica que el archivo sea `SWATID-A2_v2.5.4.bin` exactamente
2. Comprueba que el archivo no esté corrupto
3. Intenta con otro navegador (Chrome recomendado)
4. Verifica la conexión de red

### Problema 2: Actualización se queda en X%
**Síntomas**: Barra de progreso detenida

**Soluciones**:
1. Espera 2-3 minutos sin interrumpir
2. Si persiste, refresca la página y reintenta
3. Verifica que hay suficiente espacio en flash (debe ser < 95%)

### Problema 3: Dispositivo no responde después de actualización
**Síntomas**: No hay acceso web después del reinicio

**Soluciones**:
1. Espera 2-3 minutos adicionales
2. El dispositivo tiene **rollback automático**
3. Si después de 5 minutos no responde, desconecta y reconecta alimentación
4. El sistema debería volver a la versión anterior automáticamente

### Problema 4: Versión no cambió
**Síntomas**: Sigue mostrando versión anterior

**Soluciones**:
1. Refresca la página (Ctrl+F5 o Cmd+Shift+R)
2. Borra caché del navegador
3. Verifica que la actualización se completó sin errores
4. Reintenta el proceso

---

## 📊 Registro de Pruebas

### Información de la Prueba

| Campo | Valor |
|-------|-------|
| **Fecha de prueba** | _______________ |
| **Probado por** | _______________ |
| **Dispositivo Serial** | _______________ |
| **IP del dispositivo** | _______________ |
| **Versión inicial** | _______________ |
| **Versión final** | _______________ |

### Resultados

| Etapa | Estado | Tiempo | Notas |
|-------|--------|--------|-------|
| Acceso inicial | ⬜ OK / ⬜ Error | ___ min | _________ |
| Página OTA | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Selección archivo | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Subida (0-100%) | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Validación | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Instalación | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Reinicio | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Acceso post-update | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Verificación versión | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Códigos mantenidos | ⬜ OK / ⬜ Error | ___ seg | _________ |
| Funcionalidad MQTT | ⬜ OK / ⬜ Error | ___ min | _________ |

### Resultado Final

- [ ] ✅ **PRUEBA EXITOSA** - Todos los checks completados
- [ ] ⚠️ **PRUEBA PARCIAL** - Algunos problemas menores
- [ ] ❌ **PRUEBA FALLIDA** - Problemas críticos encontrados

### Comentarios Adicionales
```
_________________________________________________________________________
_________________________________________________________________________
_________________________________________________________________________
```

---

## 📞 Soporte

Si encuentras problemas durante la prueba:

**SWATID - Soporte Técnico**
- 📧 Email: soporte@swatid.com
- 📱 Teléfono: +34 686 103 132
- 🌐 Web: www.swatid.com

**Información a proporcionar al soporte**:
1. Serial del dispositivo
2. Versión inicial del firmware
3. Logs de la consola del navegador (F12)
4. Captura de pantalla del error (si aplica)
5. Registro de pruebas completado

---

**¡Buena suerte con la prueba! 🚀**

