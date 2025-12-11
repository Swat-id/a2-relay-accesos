# Listado de Ramas del Proyecto SWATID-A2

Este documento mantiene el registro de todas las ramas del proyecto, su propósito y estado.

---

## 📋 Estructura de Ramas

### Ramas Principales

#### `main`
- **Tipo**: Rama principal
- **Estado**: ✅ Estable - Producción
- **Versión actual**: v3.0.0
- **Descripción**: Rama principal del proyecto que contiene el código estable y listo para producción
- **Última actualización**: 11 de Diciembre, 2025
- **Protección**: Requiere revisión antes de merge

---

## 🚀 Ramas de Desarrollo

### `v3.0`
- **Tipo**: Rama de versión
- **Estado**: ✅ Estable - Producción
- **Versión**: 3.0.0
- **Creada**: 11 de Diciembre, 2025
- **Propósito**: Corrección crítica de validación remota
- **Cambios Principales**:
  - ✅ Corrección del procesamiento de mensajes MQTT `/granted`
  - ✅ Eliminación de código duplicado
  - ✅ Validación correcta de `message_id`
  - ✅ Uso correcto de campos `relay_number` y `duration`
- **Documentación**: `/docs/v3.0/`
- **Commits**: 1
- **Merge a main**: ✅ Completado (commit inicial)

**Issues Resueltos**:
- [#SWAT-001] Validación remota no funcional
- [#SWAT-002] Error 7 en mensajes válidos
- [#SWAT-003] message_id no validado correctamente

**Archivos Clave**:
- `firmware/SWATID-A2_v3.0.0.bin`
- `firmware/README_v3.0.0.md`
- `firmware/RELEASE_NOTES_v3.0.0.md`
- `docs/v3.0/README.md`

---

## 📚 Historial de Versiones

### Serie v3.x - Estabilización y Correcciones Críticas

#### v3.0.0 (11 de Diciembre, 2025) - ACTUAL
**Rama**: `v3.0`  
**Tipo**: Major Release  
**Estado**: ✅ Producción

**Cambios Principales**:
- Corrección crítica de validación remota
- Eliminación de código legacy duplicado
- Mejoras en logging y debugging
- Documentación completa actualizada

**Impacto**: CRÍTICO  
**Compatibilidad**: Backend v2.x  
**Recomendación**: Actualización obligatoria

**Métricas**:
- Flash: 90.5% (1,185,949 bytes)
- RAM: 15.3% (50,180 bytes)
- Tests: 24/24 pasados (100%)

---

### Serie v2.5.x - Desarrollo Legacy (Consolidada en v3.0)

#### v2.5.4
- Última versión de la serie v2.5.x
- Incluía correcciones parciales
- Código duplicado causaba problemas
- **Estado**: Deprecada, usar v3.0.0

#### v2.5.3
- Gestión de códigos remotos
- Mejoras en visualización de horarios
- Ampliación de límites de memoria

#### v2.5.2
- Tests de OTA
- Correcciones de compilación

#### v2.5.1
- Correcciones de OTA
- Mejoras de estabilidad

#### v2.5.0
- Base de la serie v2.5.x

---

## 🎯 Roadmap de Ramas Futuras

### v3.1.0 (Planeada - Q1 2026)
**Prioridad**: Alta  
**Objetivo**: Optimización y Nuevas Funcionalidades

**Funcionalidades Planeadas**:
- [ ] Optimización de uso de Flash (reducir del 90.5%)
- [ ] WiFi directo sin AP intermedio
- [ ] Gestión de múltiples redes WiFi
- [ ] Tests automatizados
- [ ] Métricas de telemetría

**Rama**: `v3.1` (a crear)  
**Tipo**: Minor Release

---

### v4.0.0 (Planeada - Q2 2026)
**Prioridad**: Media  
**Objetivo**: Funcionalidades Avanzadas

**Funcionalidades Planeadas**:
- [ ] Integración con sistemas de videovigilancia
- [ ] API REST adicional
- [ ] Sistema de backup automático
- [ ] Dashboard de monitoreo avanzado
- [ ] Soporte para más protocolos de tarjetas

**Rama**: `v4.0` (a crear)  
**Tipo**: Major Release

---

## 🔄 Política de Ramas

### Nomenclatura
- **Ramas de versión**: `vX.Y` (ej: `v3.0`, `v3.1`)
- **Ramas de features**: `feature/nombre-descriptivo`
- **Ramas de corrección**: `fix/nombre-bug`
- **Ramas de hotfix**: `hotfix/nombre-critico`

### Flujo de Trabajo

1. **Desarrollo de Nueva Funcionalidad**
   ```
   main → feature/nueva-funcionalidad → main
   ```

2. **Corrección de Bug**
   ```
   main → fix/nombre-bug → main
   ```

3. **Hotfix Crítico**
   ```
   main → hotfix/critico → main (fast-forward)
   ```

4. **Nueva Versión**
   ```
   main → vX.Y (tag vX.Y.Z)
   ```

### Protección de Ramas

#### `main`
- ✅ Requiere pull request
- ✅ Requiere revisión de código
- ✅ Requiere tests pasados
- ❌ No permite push directo
- ❌ No permite force push

#### Ramas de versión (`vX.Y`)
- ✅ Permite commits directos para documentación
- ⚠️ Cambios de código requieren PR
- ❌ No permite force push

---

## 📊 Estado Actual del Proyecto

### Rama Activa
- **Nombre**: `main`
- **Versión**: v3.0.0
- **Commit**: `da0e8b6`
- **Mensaje**: "feat: Initial commit - Firmware v3.0.0 with critical remote validation fix"

### Estadísticas
- **Total de ramas**: 2 (main, v3.0)
- **Total de commits**: 1
- **Archivos rastreados**: 120
- **Líneas de código**: 58,049+

### Último Cambio
- **Fecha**: 11 de Diciembre, 2025
- **Tipo**: Corrección crítica
- **Impacto**: Alta prioridad
- **Tests**: 24/24 pasados

---

## 🏷️ Sistema de Tags

### Tags de Versión
Cada versión estable tiene un tag correspondiente:

```
v3.0.0  →  Firmware SWATID-A2 v3.0.0
v2.5.4  →  Última versión legacy
v2.5.3  →  Versión anterior
...
```

### Crear Tag
```bash
git tag -a v3.0.0 -m "Release v3.0.0 - Critical remote validation fix"
git push origin v3.0.0
```

---

## 📝 Convenciones de Commits

### Formato
```
<tipo>(<scope>): <descripción corta>

<descripción detallada>

<referencias y notas>
```

### Tipos
- `feat`: Nueva funcionalidad
- `fix`: Corrección de bug
- `docs`: Cambios en documentación
- `style`: Cambios de formato (no afectan código)
- `refactor`: Refactorización de código
- `perf`: Mejoras de rendimiento
- `test`: Añadir o corregir tests
- `chore`: Tareas de mantenimiento

### Ejemplos
```bash
feat(mqtt): Add message_id validation to remote validation response
fix(relay): Correct relay_number extraction from JSON message
docs(v3.0): Add comprehensive branch documentation
refactor(mqtt): Remove duplicated /granted topic processing
```

---

## 🔍 Información de Branches

### Listar Branches
```bash
# Locales
git branch

# Todas (locales y remotas)
git branch -a

# Con último commit
git branch -v
```

### Cambiar de Branch
```bash
git checkout v3.0
git checkout main
```

### Crear Branch
```bash
git branch nombre-rama
git checkout -b nombre-rama  # crear y cambiar
```

### Eliminar Branch
```bash
git branch -d nombre-rama    # solo si está mergeada
git branch -D nombre-rama    # forzar eliminación
```

---

## 📞 Contacto y Soporte

Para preguntas sobre las ramas o el flujo de trabajo:
1. Revisar este documento
2. Consultar documentación de la rama específica en `/docs/vX.Y/`
3. Revisar commits y mensajes de commit
4. Contactar al equipo de desarrollo

---

## 🔗 Referencias

- [Documentación Principal](/docs/README.md)
- [Rama v3.0](/docs/v3.0/README.md)
- [Release Notes v3.0.0](/firmware/RELEASE_NOTES_v3.0.0.md)
- [Git Flow](https://nvie.com/posts/a-successful-git-branching-model/)

---

**Mantenido por**: Equipo SWAT ID  
**Última actualización**: 11 de Diciembre, 2025  
**Versión del documento**: 1.0  
**Próxima revisión**: Cada nueva versión

