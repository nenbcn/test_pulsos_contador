# TODO - Mejoras Futuras

> 📖 **Instrucciones de proceso**: Ver `.copilot-instructions.md`

## 📖 INSTRUCCIONES DE USO

### 🔄 Flujo de Trabajo Recomendado

#### 1️⃣ Seleccionar Tarea
- Revisar sección **🎯 MEJORAS PENDIENTES**
- Elegir una tarea según prioridad/impacto
- Considerar dependencias entre tareas

#### 2️⃣ Planificar Implementación
- Dividir tarea grande en **sub-pasos pequeños** (máx 20-30 líneas de código por paso)
- Cada paso debe ser **compilable y testeable** independientemente
- Ejemplo para MEJORA-003:
  ```
  Paso 1: Añadir variables para tracking de segmento anterior
  Paso 2: Implementar borrado de segmento viejo
  Paso 3: Implementar dibujo solo de nuevo segmento
  Paso 4: Probar en MODE_READ
  Paso 5: Probar en MODE_PRESSURE
  ```

#### 3️⃣ Implementar Paso a Paso
- **Implementar UN solo paso**
- **Compilar** (`platformio run`)
- **Probar** funcionalidad afectada
- Si falla: **revertir y ajustar**
- Si funciona: **siguiente paso**

#### 4️⃣ Consolidar Cambio (Commit)
Cuando el conjunto de pasos forma una **unidad funcional completa**:

**Formato de commit sugerido:**
```
[MEJORA-XXX] Descripción breve (50 chars max)

- Detalle de cambio 1
- Detalle de cambio 2
- Beneficio conseguido

Archivos: src/main.cpp
Líneas: +XX/-YY
Testeado: MODE_READ, MODE_WRITE, etc.
```

**Ejemplos:**
```bash
# Commit después de completar MEJORA-001
git add src/main.cpp
git commit -m "[MEJORA-001] Unificar funciones de gráfico

- Creada actualizarGraficoGenerico() genérica
- actualizarGrafico() y actualizarGraficoPresion() ahora wrappers
- Eliminadas ~150 líneas duplicadas

Archivos: src/main.cpp
Líneas: +65/-150
Testeado: MODE_READ, MODE_PRESSURE OK"
```

```bash
# Commit después de completar MEJORA-013
git add src/main.cpp
git commit -m "[MEJORA-013] Reemplazar String por buffers estáticos

- mostrarVoltaje() usa snprintf() con buffer 10 bytes
- mostrarInfoSensor() usa 4 buffers estáticos (140 bytes total)
- Eliminada fragmentación de heap

Archivos: src/main.cpp
Líneas: +45/-35
Testeado: Todos los modos, textos OK, sin memory leaks"
```

```bash
# Commit para múltiples mejoras pequeñas relacionadas
git add src/main.cpp
git commit -m "[MEJORA-008,011] Limpieza y constantes

- Eliminado código comentado de calibración voltaje
- Definidas 9 constantes de timing/config

Archivos: src/main.cpp
Líneas: +15/-20
Testeado: Lectura voltaje OK, timings correctos"
```

#### 5️⃣ Actualizar TODO.md
- Marcar tarea como **completada** (mover a historial)
- Añadir detalles de implementación:
  - Cambios realizados
  - Impacto medido
  - Beneficios conseguidos
  - Fecha de completado

**Ejemplo:**
```markdown
#### ✅ MEJORA-003: Renderizado incremental de gráfico (19-OCT-2025)
- **Cambios**: 
  - Añadidas variables last_x, last_y para tracking
  - Función borrarSegmentoAntiguo() implementada
  - Solo dibuja nuevo segmento en actualizarGraficoGenerico()
- **Impacto**: Tiempo de actualización reducido de ~15ms a ~1.5ms
- **Beneficio**: 10x mejora velocidad, gráfico más fluido @ 100Hz
```

#### 6️⃣ Commit del TODO
```bash
git add TODO.md
git commit -m "[DOC] Actualizar TODO - MEJORA-003 completada

Renderizado incremental implementado exitosamente
Tiempo actualización: 15ms → 1.5ms (10x mejora)"
```

### 📋 Gestión de Tareas

#### Añadir Nueva Tarea Pendiente
1. Identificar categoría apropiada en **🎯 MEJORAS PENDIENTES**
2. Añadir con formato:
```markdown
- [ ] **MEJORA-XXX**: Título breve
  - **Descripción**: Qué hace la mejora
  - **Impacto/Problema actual**: Por qué es necesaria
  - **Solución propuesta**: Cómo implementarla
  - **Beneficio**: Qué se gana
  - **Complejidad**: Baja/Media/Alta
```

#### Priorizar Tareas
**Alta prioridad** (hacer pronto):
- 🔴 Bugs críticos o regresiones
- 🟠 Optimizaciones de rendimiento visibles
- 🟡 Mejoras que desbloquean otras tareas

**Media prioridad** (próximas iteraciones):
- 🟢 Refactorizaciones de arquitectura
- 🔵 Mejoras de UX

**Baja prioridad** (nice to have):
- ⚪ Documentación
- ⚪ Features opcionales

### 🎯 Reglas de Oro

1. **Nunca implementar sin compilar/probar** ❌
2. **Un cambio pequeño > un cambio grande** ✅
3. **Compilar después de cada paso** ✅
4. **Probar funcionalidad afectada antes de continuar** ✅
5. **Commit cuando algo funcional esté completo** ✅
6. **Actualizar TODO.md al completar** ✅

### 🚀 Ejemplo Completo de Flujo

```
1. Leo TODO → Elijo MEJORA-003 (renderizado incremental)
2. Planifico 5 pasos
3. Implemento Paso 1 (variables tracking)
   → Compilo ✅ → Funciona ✅
4. Implemento Paso 2 (borrar segmento viejo)
   → Compilo ✅ → Funciona ✅
5. Implemento Paso 3 (dibujar solo nuevo)
   → Compilo ✅ → Funciona ✅
6. Pruebo MODE_READ → OK ✅
7. Pruebo MODE_PRESSURE → OK ✅
8. Commit: "git commit -m '[MEJORA-003] Renderizado incremental...'"
9. Actualizo TODO.md (mover a completadas)
10. Commit TODO: "git commit -m '[DOC] TODO - MEJORA-003 done'"
```

---

## 🎯 MEJORAS PENDIENTES (Próximas Iteraciones)

### 🏗️ Arquitectura y Refactorización (ALTA PRIORIDAD)
- [ ] **MEJORA-017**: Extraer funciones de manejo de modos del loop()
  - **Descripción**: Crear funciones `manejarModoRead()`, `manejarModoWrite()`, `manejarModoPressure()`, `manejarModoWiFi()`
  - **Problema actual**: loop() tiene ~120 líneas con toda la lógica mezclada
  - **Beneficio**: Código más modular, testeable, legible. loop() quedaría en ~20 líneas
  - **Complejidad**: Baja (refactorización directa, sin cambios de lógica)
  - **Archivos**: src/main.cpp

- [ ] **MEJORA-018**: Extraer lógica de cambio de modo a función dedicada
  - **Descripción**: Crear `cambiarModo(SystemMode nuevo_modo)` con toda la lógica de transición
  - **Problema actual**: Botón derecho tiene ~50 líneas con if-else anidados para cambios de modo
  - **Beneficio**: Lógica centralizada, más fácil añadir nuevos modos
  - **Complejidad**: Baja
  - **Archivos**: src/main.cpp

- [ ] **MEJORA-019**: Extraer manejo de botones a funciones separadas
  - **Descripción**: Crear `manejarBotonIzquierdo()` y `manejarBotonDerecho()`
  - **Problema actual**: Lógica de botones mezclada en loop() (~40 líneas)
  - **Beneficio**: Código más claro, facilita añadir nuevas acciones a botones
  - **Complejidad**: Baja
  - **Archivos**: src/main.cpp

### 🚀 Optimización de Rendimiento
- [ ] **MEJORA-003**: Renderizado incremental de gráfico
  - **Descripción**: Cambiar de redibujar todo (O(n)) a solo nuevo segmento (O(1))
  - **Impacto**: Reducir lag en display TFT, especialmente en MODE_PRESSURE @ 100Hz
  - **Complejidad**: Media-Alta (requiere testing exhaustivo)
  - **Beneficio**: ~10x mejora en velocidad de actualización del gráfico

- [ ] **MEJORA-007**: Timer hardware para generación de pulsos
  - **Descripción**: Migrar de `millis()` a ESP32 hardware timer o `micros()`
  - **Impacto actual**: A 100Hz (10ms período), error puede ser >10% con millis()
  - **Beneficio**: Precisión sub-microsegundo en generación de pulsos

### 📐 Arquitectura y Diseño
- [ ] **MEJORA-020**: Crear estructura ModeHandler para cada modo
  - **Descripción**: Cada modo (READ, WRITE, PRESSURE, WIFI) como objeto/struct con funciones init(), update(), cleanup()
  - **Problema actual**: Lógica dispersa en múltiples lugares (loop, cambio modo, setup)
  - **Beneficio**: Arquitectura más escalable, fácil añadir nuevos modos, código más organizado
  - **Complejidad**: Media-Alta
  - **Archivos**: src/main.cpp, posiblemente crear src/modes.h

- [ ] **MEJORA-006**: Crear estructura DisplayState_t
  - **Descripción**: Agrupar todas las variables `static` de UI en una estructura
  - **Lugares afectados**: mostrarVoltaje(), mostrarInfoSensor(), mostrarListaWiFi()
  - **Beneficio**: Estado más claro, testeable, reseteable

- [ ] **MEJORA-009**: Nombres de variables descriptivos
  - **Ejemplos a mejorar**: `n` → `wifi_network_count`, `i` → `graph_point_index`
  - **Beneficio**: Código más auto-documentado

### 💾 Gestión de Recursos
- [ ] **MEJORA-012**: Optimizar memoria WiFi scanner
  - **Actual**: Array fijo WiFiNetwork[20] (~400 bytes siempre reservados)
  - **Solución**: Usar array dinámico o más pequeño
  - **Beneficio**: Liberar RAM (crítico en ESP32)

### 🛡️ Seguridad y Robustez
- [ ] **MEJORA-014**: Validación de rangos en sensores
  - **Descripción**: Añadir clipping para valores extremos en pressure_value
  - **Beneficio**: Sistema más robusto ante fallos de hardware

- [ ] **MEJORA-015**: Mejorar manejo de errores I2C
  - **Actual**: Solo imprime error cada 5 segundos
  - **Solución**: Reintentos automáticos, reset del bus I2C
  - **Beneficio**: Mayor confiabilidad del sistema

### 🎨 UI/UX
- [ ] **MEJORA-016**: Indicador visual de batería
  - **Actual**: Solo voltaje numérico
  - **Solución**: Icono de batería con niveles de color
  - **Beneficio**: UX mejorada, advertencia clara de batería baja

- [ ] Mejorar presentación de pantallas y legibilidad
- [ ] Añadir transiciones suaves entre modos
- [ ] Mejorar contraste y visibilidad de textos
- [ ] Optimizar distribución de elementos en pantalla

### ⚙️ Funcionalidades
- [ ] Calibración automática del sensor de presión
- [ ] Guardado de configuración en EEPROM
- [ ] Modo de análisis espectral para pulsos
- [ ] Implementar filtro digital para sensor de presión
- [ ] Implementar watchdog timer
- [ ] Mejorar precisión de medición de frecuencia

### 🔧 Hardware
- [ ] Sensor de presión por voltaje (alternativa al I2C actual)
- [ ] Mejora en la alimentación y medición de voltaje
- [ ] Protección contra sobretensiones en entradas

### 📚 Documentación
- [ ] Esquema de conexiones detallado
- [ ] Manual de usuario
- [ ] Guía de troubleshooting
- [ ] Videos demostrativos

---

## ✅ HISTORIAL DE MEJORAS COMPLETADAS

### 📅 Optimizaciones de Código (19-OCT-2025)
**Resultado: ~165 líneas eliminadas, código más limpio y eficiente**

#### ✅ MEJORA-001: Unificación de funciones de gráfico (ALTO IMPACTO)
- **Cambios**: 
  - Creada función genérica `actualizarGraficoGenerico()`
  - `actualizarGrafico()` reducida a 4 líneas (wrapper)
  - `actualizarGraficoPresion()` reducida a 4 líneas (wrapper)
- **Impacto**: ~150 líneas duplicadas eliminadas 🔥
- **Beneficio**: Mantenimiento simplificado, único punto de actualización

#### ✅ MEJORA-002: Función marco de gráfico
- **Cambios**: Creada función `dibujarMarcoGrafico(bool es_presion)` reutilizable
- **Beneficio**: Código DRY, evita duplicación

#### ✅ MEJORA-008: Limpieza de calibración de voltaje
- **Cambios**: Eliminado código duplicado de calibración (~15 líneas)
- **Beneficio**: Código más limpio y mantenible

#### ✅ MEJORA-011: Constantes definidas
- **Cambios**: Definidas 9 constantes con nombres descriptivos
  - `BUTTON_DEBOUNCE_MS` (300)
  - `PULSE_CALC_INTERVAL_MS` (200)
  - `VOLTAGE_UPDATE_MS` (500)
  - `WIFI_SCAN_INTERVAL_MS` (10000)
  - `PRESSURE_READ_INTERVAL_MS` (10)
  - `SLEEP_TIMEOUT_MS` (300000)
  - `SERIAL_DEBUG_INTERVAL_MS` (1000)
  - `SERIAL_DEBUG_SLOW_MS` (5000)
  - `VOLTAGE_ADC_SAMPLES` (32)
- **Beneficio**: Código más legible, fácil de ajustar

#### ✅ MEJORA-013: Buffers estáticos (ALTO IMPACTO)
- **Cambios**:
  - Reemplazadas todas las concatenaciones de String dinámicos
  - Uso de `snprintf()` con buffers en stack (140 bytes totales)
  - Funciones afectadas: `mostrarVoltaje()`, `mostrarInfoSensor()`
- **Impacto**: Zero fragmentación de heap 🚀
- **Beneficio**: Memoria más predecible, mejor rendimiento

#### 📊 Resumen de Impacto:
- 📉 ~165 líneas de código eliminadas
- 🎯 Código DRY y mantenible
- 💾 Memoria optimizada (sin fragmentación heap)
- ⚡ Mejor rendimiento (menos allocaciones dinámicas)
- 📖 Más legible (constantes descriptivas)
- ✅ Todo funciona correctamente después de upload

### 📅 Funcionalidades Implementadas (Previo)
- [x] Implementar especificaciones completas de pulsos (GitHub: mica-gateway/test/pulse-generator/)
- [x] Añadir STRESS_BURST con 10s de test de carga múltiples frecuencias (15Hz-100Hz)
- [x] Implementar gradientes de arranque/parada en BURST (30Hz→50Hz→30Hz)
- [x] Corregir sleep automático: solo actividad de botones evita sleep (5 minutos)
- [x] Wake-up configurado para botones BOOT (GPIO0) y botón derecho (GPIO35)
