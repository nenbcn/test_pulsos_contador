# Análisis Detallado TC3: Compuesto (Compound)

## 📊 Resumen Ejecutivo

**Pulsos generados**: 177  
**Pulsos procesados**: 174 (**faltan 3 pulsos**)  
**Eventos generados**: 7  
**Duración total**: ~8.6s

---

## 🔍 Comparación Generación vs Procesamiento

### Patrón Generado (177 pulsos)

| Fase | Pulsos | Rango | Descripción | Periodo Promedio |
|------|--------|-------|-------------|------------------|
| 1 | #1-41 | 71.6→54.5ms | Bajada inicial progresiva | ~60ms |
| 2 | #42-48 | 49.9→32.1ms | Bajada rápida | ~40ms |
| 3 | #49-124 | 27.0→28.9ms | **Estable bajo (76 pulsos)** | ~28ms |
| 4 | #125-166 | 30.0→58.2ms | Subida gradual | ~45ms |
| 5 | #167-177 | 60.4→98.9ms | Subida final rápida | ~80ms |

### Eventos Procesados (174 pulsos)

| Evento | Secuencia | Pulsos | Tipo | Periodo Avg | Rango Real | ❌ Problema |
|--------|-----------|--------|------|-------------|------------|-------------|
| 1 | 5958-5975 | 18 | **TRANSITION** | 58ms | 72→53ms | ✅ OK - Inicio bajada |
| 2 | 5976-6003 | 28 | **STABLE** | 51ms | 55→42ms | ⚠️ **Debería ser TRANSITION** |
| 3 | 6004-6015 | 12 | **TRANSITION** | 28ms | 36→29ms | ✅ OK - Bajada rápida |
| 4 | 6016-6082 | 67 | **STABLE** | 28ms | 27→32ms | ⚠️ **Faltan ~9 pulsos** |
| 5 | 6083-6099 | 17 | **TRANSITION** | 49ms | 32→54ms | ✅ OK - Inicio subida |
| 6 | 6100-6124 | 25 | **STABLE** | 55ms | 55→68ms | ⚠️ **Debería ser TRANSITION** |
| 7 | 6125-6131 | 7 | **TRANSITION** | 83ms | 70→95ms | ✅ OK - Subida final (timeout) |

**Total procesado**: 18 + 28 + 12 + 67 + 17 + 25 + 7 = **174 pulsos** ❌

---

## ❌ PROBLEMA 1: División Incorrecta de Transiciones Largas

### Caso 1: Bajada Inicial Fragmentada

**Generado** (pulsos #1-41):
```
71.6 → 71.9 → 68.6 → 64.9 → 62.0 → ... → 54.5ms
Tendencia: DESCENDENTE constante (41 pulsos)
```

**Procesado**:
- **Evento 1** (seq 5958-5975): 18 pulsos, TRANSITION, 58ms avg
  ```
  72 → 72 → 68 → 65 → 62 → 62 → 60 → 56 → 57 → 54 → 52 → 55 → 54 → 52 → 51 → 54 → 54 → 53ms
  ```
  
- **Evento 2** (seq 5976-6003): 28 pulsos, **STABLE** ❌, 51ms avg
  ```
  55 → 53 → 53 → 51 → 54 → 53 → 52 → 54 → 54 → 52 → 51 → ... → 50ms
  ```

**❌ PROBLEMA**: El gateway detecta "estabilización" en ~53ms porque:
- Los últimos 5-10 pulsos del evento 1 tienen CV bajo (52→54ms)
- El algoritmo no detecta que la **tendencia sigue siendo descendente**
- Declara STABLE prematuramente aunque el período seguirá bajando

**Evidencia**:
```
[INFO]: State: TRANSITION → STABLE (stabilized at 53 ms, pulses=18)
```

Pero inmediatamente después:
```
[DEBUG]: Pulse received: ts=2543174, period=55, state=2
[DEBUG]: Pulse received: ts=2543229, period=55, state=2
[DEBUG]: Pulse received: ts=2543282, period=53, state=2
...
[DEBUG]: Pulse received: ts=2544392, period=50, state=2  ← Sigue bajando!
```

### Caso 2: Subida Gradual Fragmentada

**Generado** (pulsos #125-166):
```
30.0 → 31.0 → 32.2 → 35.2 → ... → 58.2ms
Tendencia: ASCENDENTE constante (42 pulsos)
```

**Procesado**:
- **Evento 5** (seq 6083-6099): 17 pulsos, TRANSITION, 49ms avg
- **Evento 6** (seq 6100-6124): 25 pulsos, **STABLE** ❌, 55ms avg

**❌ PROBLEMA**: Mismo error - detecta estabilización temporal en ~54ms aunque la tendencia es ascendente.

---

## ❌ PROBLEMA 2: Estable Incompleto

**Generado** (pulsos #49-124): **76 pulsos estables** a ~28ms

**Procesado** (evento 4, seq 6016-6082): **67 pulsos estables** a ~28ms

**❌ FALTAN 9 PULSOS** del estable bajo

**Posible causa**:
1. **Pérdida al inicio del fragmento**: El primer pulso del fragmento se cuenta en el evento anterior
2. **Detección prematura de cambio abrupto**: 
   ```
   [DEBUG]: Pulse received: ts=2546858, period=32, state=2, count=68
   [INFO]: State: STABLE → TRANSITION (abrupt change: 26 → 32 ms, diff=23.1%)
   ```
   
   El sistema detecta 26ms→32ms (+23.1%) como cambio abrupto, pero mirando los datos generados:
   - Pulso #124: **27.2ms** (último del estable)
   - Pulso #125: **30.0ms** (primer pulso de subida)
   - Diferencia real: 27.2→30.0 = +10.3% ❌
   
   El gateway está calculando mal el cambio porque usa un período anterior incorrecto.

---

## ❌ PROBLEMA 3: Pérdida de 3 Pulsos

**Análisis de secuencias**:

```
Generado: #1-177 (177 pulsos)
Procesado: seq 5958-6131 (5958 → 6131 = 174 secuencias)
```

**Posibles causas**:

### 1. Primer Pulso No Contado
```
[INFO]: State: IDLE → TRANSITION (first pulse, ts=2542123, seq=5958)
```

El primer pulso (`seq=5958`) se usa como **marca de inicio** pero podría no incluirse en el conteo del evento 1.

**Verificación**:
- Evento 1 empieza en seq 5958 y termina en 5975
- 5975 - 5958 + 1 = **18 pulsos** ✅

❌ **No es esta la causa**

### 2. Transición de Eventos (Pulsos Compartidos)

Mirando las transiciones STABLE→TRANSITION:

**Transición 1** (evento 2→3):
```
[DEBUG]: Pulse received: ts=2544597, period=36, state=2, count=29  ← Último de STABLE
[INFO]: State: STABLE → TRANSITION (abrupt change: 46 → 36 ms)
[DEBUG]: First pulse of new fragment: ts=2544631, state=1, seq=6004
```

El pulso #29 del evento 2 (period=36) **NO está incluido** en el siguiente fragmento que empieza en seq=6004.

✅ **No hay pérdida aquí**

**Transición 2** (evento 4→5):
```
[DEBUG]: Pulse received: ts=2546858, period=32, state=2, count=68  ← Último de STABLE
[INFO]: State: STABLE → TRANSITION (abrupt change: 26 → 32 ms)
[DEBUG]: First pulse of new fragment: ts=2546894, state=1, seq=6083
```

**❌ SOSPECHA**: 
- Evento 4 debería tener 68 pulsos pero reporta 67
- Hay un pulso perdido entre seq 6082 y 6083

### 3. Redondeos en Timestamps

Comparando timestamps:

**Generado**:
```
Pulse #1: ts=51836ms
Pulse #177: ts=59428ms
Duración: 7592ms
```

**Procesado**:
```
Primer pulso: ts=2542123ms
Último pulso: ts=2549715ms
Duración: 7592ms
```

✅ **Duración coincide**, por lo que no hay pérdidas significativas de tiempo.

### 4. Conclusión sobre los 3 Pulsos Perdidos

**Hipótesis más probable**:
1. **1 pulso** perdido en la transición evento 4→5 (67 vs 68 esperados)
2. **2 pulsos** perdidos por conteo incorrecto en las transiciones evento 1→2 o 2→3

---

## 🔧 CAUSAS RAÍZ IDENTIFICADAS

### CR1: Algoritmo de Estabilización No Considera Tendencia

**Ubicación**: `/Users/nenbcn/Code/mica-gateway/src/pin_receiver.cpp:239-253`

**Código actual**:
```cpp
static bool isStabilized(const uint64_t* periods, uint8_t count) {
    if (count < STABLE_CONFIRMATION) return false;  // Necesita 10 pulsos
    
    uint64_t avg = calculateAverage(periods, count, STABLE_CONFIRMATION);
    if (avg == 0) return false;
    
    // ❌ PROBLEMA: Solo verifica si todos los períodos están dentro de ±8%
    uint8_t startIdx = count - STABLE_CONFIRMATION;
    for (uint8_t i = startIdx; i < count; i++) {
        uint64_t diff = (periods[i] > avg) ? (periods[i] - avg) : (avg - periods[i]);
        if (diff > avg * STABLE_TOLERANCE) {  // 0.08 = ±8%
            return false;
        }
    }
    
    return true;  // ❌ No detecta si hay tendencia descendente/ascendente
}
```

**Constantes** (en `config.h`):
```cpp
const float STABLE_TOLERANCE = 0.08;        // 8% variación permitida
const uint8_t STABLE_CONFIRMATION = 10;     // Necesita 10 pulsos consecutivos estables
```

**Problema Identificado**:
- Solo verifica que los últimos **10 períodos** estén dentro de **±8%** del promedio
- **NO detecta tendencia direccional** (ascendente/descendente)
- En TC3, los períodos `55→53→53→51→54→53→52→54→54→52→51` pasan el test de ±8%
  - Promedio: ~53ms
  - Todos los valores: 51-55ms (variación <8% ✓)
  - Pero la tendencia es **DESCENDENTE** (sigue bajando a 50→47→42ms)

**Solución propuesta**:
```cpp
static bool isStabilized(const uint64_t* periods, uint8_t count) {
    if (count < STABLE_CONFIRMATION) return false;
    
    uint64_t avg = calculateAverage(periods, count, STABLE_CONFIRMATION);
    if (avg == 0) return false;
    
    // Verificar variación ±8%
    uint8_t startIdx = count - STABLE_CONFIRMATION;
    for (uint8_t i = startIdx; i < count; i++) {
        uint64_t diff = (periods[i] > avg) ? (periods[i] - avg) : (avg - periods[i]);
        if (diff > avg * STABLE_TOLERANCE) {
            return false;
        }
    }
    
    // ✅ NUEVO: Verificar que no haya tendencia direccional
    uint64_t firstInWindow = periods[startIdx];
    uint64_t lastInWindow = periods[count - 1];
    uint64_t trendDiff = (lastInWindow > firstInWindow) 
                         ? (lastInWindow - firstInWindow) 
                         : (firstInWindow - lastInWindow);
    
    // Si la diferencia entre primer y último período > 15% → hay tendencia
    const float TREND_THRESHOLD = 0.15;  // 15%
    if (trendDiff > avg * TREND_THRESHOLD) {
        Log::debug("Trend detected: first=%llu, last=%llu, diff=%.1f%% > threshold",
                   firstInWindow, lastInWindow, 
                   100.0 * trendDiff / avg);
        return false;  // No estable, sigue en transición
    }
    
    return true;
}
```

**Impacto esperado**:
- La bajada inicial (72→42ms) NO se fragmentará en TRANSITION + STABLE
- La subida gradual (32→68ms) NO se fragmentará en TRANSITION + STABLE  
- Solo declarará STABLE cuando los períodos estén **realmente estables** sin tendencia

### CR2: Umbral de Cambio Abrupto Demasiado Sensible

**Ubicación**: `/Users/nenbcn/Code/mica-gateway/src/pin_receiver.cpp:260-266`

**Código actual**:
```cpp
static bool isAbruptChange(uint64_t currentPeriod, uint64_t avgPeriod) {
    if (avgPeriod == 0) return false;
    
    uint64_t diff = (currentPeriod > avgPeriod) 
                    ? (currentPeriod - avgPeriod) 
                    : (avgPeriod - currentPeriod);
    
    return (diff > avgPeriod * CHANGE_THRESHOLD);  // 0.20 = 20%
}
```

**Constante** (en `config.h`):
```cpp
const float CHANGE_THRESHOLD = 0.20;  // 20% cambio rompe estado STABLE
```

**Problema Identificado**:

En el TC3, el gateway reporta:
```
[INFO]: State: STABLE → TRANSITION (abrupt change: 26 → 32 ms, diff=23.1%)
```

Pero analizando los datos **generados**:
- Último pulso del estable (generado #124): **27.2ms**
- Primer pulso de subida (generado #125): **30.0ms**
- Diferencia real: (30.0 - 27.2) / 27.2 = **10.3%** ❌

**¿Por qué el gateway calcula 23.1%?**

El código usa `g_stableAvgPeriod` que se actualiza con **EMA (Exponential Moving Average)**:

```cpp
// En pin_receiver.cpp:577
g_stableAvgPeriod = (g_stableAvgPeriod * 9 + period) / 10;
```

Por lo tanto:
1. El promedio estable está en ~28ms durante los 67 pulsos estables
2. Los últimos pulsos del estable fueron 26-27ms (variación natural)
3. El EMA se ajustó hacia abajo: 28ms → **26ms**
4. Cuando llega el pulso de 32ms: (32-26)/26 = **23.1%** ✓

**El cálculo es correcto, pero el umbral es muy sensible para períodos cortos.**

**Análisis de sensibilidad**:

| Periodo Avg | Umbral 20% | Umbral 30% | Variación Tolerada |
|-------------|------------|------------|---------------------|
| 100ms | ±20ms | ±30ms | Mejor para flujos lentos |
| 50ms | ±10ms | ±15ms | Adecuado |
| **28ms** | **±5.6ms** | **±8.4ms** | **Muy sensible con 20%** |
| 20ms | ±4ms | ±6ms | Extremadamente sensible |

**Problema**: Con períodos de 28ms y jitter de ±2ms:
- Variación normal: 26-30ms (±7%)
- Umbral 20%: 28ms ± 5.6ms = 22.4-33.6ms
- Un pulso de 30ms puede disparar cambio abrupto si el EMA baja a 26ms

**Solución propuesta**:
```cpp
const float CHANGE_THRESHOLD = 0.30;  // De 0.20 a 0.30 (30%)
```

**Verificación con 30%**:
- TC3, pulso 32ms vs avg 26ms: (32-26)/26 = 23.1% < 30% ✓ **No dispararía cambio abrupto**
- Permitiría capturar los 9 pulsos faltantes del estable

**Impacto esperado**:
- Reducirá fragmentación excesiva en estables cortos
- El estable bajo (28ms) debería capturar los **76 pulsos completos**
- Cambios reales >30% seguirán siendo detectados correctamente

### CR3: Pérdida de Pulsos en Transiciones de Estado

**Ubicación**: Múltiples puntos en `pin_receiver.cpp`

**Análisis del Conteo**:

El sistema usa dos contadores:
1. **`g_pulseCount`**: Pulsos acumulados en el fragmento actual (reinicia en cada evento)
2. **`g_sequenceAtGroupStart`**: Número de secuencia global del primer pulso del fragmento

**Lógica de secuencias**:
```cpp
data.sequenceStart = sequenceStart;
data.sequenceEnd = sequenceStart + pulseCount - 1;
```

**Transición STABLE → TRANSITION** (línea 556):
```cpp
if (isAbruptChange(period, g_stableAvgPeriod)) {
    Log::info("State: STABLE → TRANSITION (abrupt change: %llu → %llu ms, diff=%.1f%%)",
              g_stableAvgPeriod, period, ...);
    
    // Calcula pulsos del STABLE ANTES del cambio
    uint64_t stableCount = g_pulseCount - 1;  // ❌ EXCLUYE el pulso que dispara el cambio
    
    if (stableCount >= 2) {
        processPulseData(g_firstPulseTimestamp, g_lastTimestamp,
                        stableCount, g_periodSum - period,  // ❌ RESTA el período del pulso actual
                        g_sequenceAtGroupStart, STABLE,
                        NULL, 0, false, true);
        g_lastSentSequence = g_sequenceAtGroupStart + stableCount - 1;
    }
    
    // Inicia nuevo fragmento TRANSITION
    g_state = STATE_TRANSITION;
    g_firstPulseTimestamp = timestamp;  // ✅ El pulso actual inicia TRANSITION
    g_sequenceAtGroupStart = g_lastSentSequence + 1;  // ✅ Siguiente secuencia
    g_pulseCount = 1;  // ✅ El pulso actual es el primero de TRANSITION
    ...
}
```

**Verificación del Empalme**:

Evento STABLE (seq 6016-6082):
```
g_sequenceAtGroupStart = 6016
stableCount = 67
g_lastSentSequence = 6016 + 67 - 1 = 6082 ✓
```

Siguiente TRANSITION (seq 6083-6099):
```
g_sequenceAtGroupStart = 6082 + 1 = 6083 ✓
El pulso que disparó el cambio (32ms) → seq 6083 ✓
```

**✅ NO hay pérdida aquí - el conteo es correcto.**

---

**Posible Causa de los 3 Pulsos Perdidos**:

Revisando el log del gateway:
```
[DEBUG]: First pulse of new fragment: ts=2544631, state=1, seq=6004
```

Este mensaje se imprime DESPUÉS de enviar el evento anterior, pero **NO hay confirmación de que el pulso se cuente**.

**Sospecha**: En la transición del evento 2→3, el código podría estar saltando el procesamiento normal:

```cpp
// Línea 514 - después de enviar TRANSITION fragmentado
if (isStabilized(g_periodHistory, g_periodHistoryCount)) {
    g_state = STATE_STABLE;
    g_periodHistoryCount = 0;
    g_lastStableSendTime = millis();
    // ...
    continue;  // ❌ SALTA el procesamiento del pulso actual
}
```

Cuando el buffer se llena (45 períodos), el código:
1. Envía el fragmento TRANSITION
2. Verifica si hay estabilización
3. Si detecta estabilización → hace `continue;`
4. **El pulso actual NO se procesa** hasta la siguiente iteración

**Sin embargo**, la siguiente iteración procesará ese pulso normalmente, por lo que tampoco debería haber pérdida aquí.

---

**Conclusión sobre los 3 pulsos**:

Después de analizar el código, **NO encuentro un bug claro que cause pérdida de 3 pulsos**.

**Posibles explicaciones**:
1. **Redondeo en conversión μs → ms**: El timestamp se convierte de microsegundos a milisegundos (línea 442), podría haber imprecisiones acumuladas
2. **Jitter excesivo**: Algún pulso generado pudo tener un período <2ms y fue filtrado por debounce
3. **Diferencia en conteo de secuencia**: El ESP32 genera 177 pulsos físicos, pero el gateway cuenta **eventos lógicos** (para sensores volumétricos, 2 pulsos físicos = 1 evento)

**Verificación necesaria**:
```cpp
// Agregar logging en ISR (con cuidado - NO usar Log::debug en ISR)
g_totalPulsesDetected++;  // Este contador NUNCA se decrementa
// Comparar con suma de todos los pulseCount en eventos enviados
```

**Para validar**, el gateway debería mostrar:
```
Total physical pulses detected: 177
Total pulses sent in events: 174
Debounced pulses: 3 ← Aquí estarían los 3 perdidos
```

---

## 📋 Recomendaciones

### Prioridad CRÍTICA ⚠️

**1. Implementar detector de tendencia en `isStabilized()`**
   - **Archivo**: `/Users/nenbcn/Code/mica-gateway/src/pin_receiver.cpp:239`
   - **Cambio**: Agregar verificación de tendencia direccional (threshold 15%)
   - **Impacto**: Evitará fragmentar transiciones largas en múltiples eventos
   - **Código**:
     ```cpp
     // Después del loop de verificación ±8%
     uint64_t firstInWindow = periods[startIdx];
     uint64_t lastInWindow = periods[count - 1];
     uint64_t trendDiff = (lastInWindow > firstInWindow) 
                          ? (lastInWindow - firstInWindow) 
                          : (firstInWindow - lastInWindow);
     const float TREND_THRESHOLD = 0.15;
     if (trendDiff > avg * TREND_THRESHOLD) {
         return false;  // Hay tendencia, no es estable
     }
     ```
   - **Validación esperada**: TC3 debería generar solo **3-4 eventos** (vs 7 actuales)

**2. Aumentar umbral de cambio abrupto a 30%**
   - **Archivo**: `/Users/nenbcn/Code/mica-gateway/src/config.h:42`
   - **Cambio**: `const float CHANGE_THRESHOLD = 0.30;  // De 0.20 a 0.30`
   - **Impacto**: Reducirá fragmentación en estables con períodos cortos (<30ms)
   - **Validación esperada**: El estable bajo debería capturar **76 pulsos** (vs 67 actuales)

### Prioridad ALTA 🔴

**3. Auditar conteo de pulsos con logging detallado**
   - **Archivo**: `/Users/nenbcn/Code/mica-gateway/src/pin_receiver.cpp`
   - **Cambio**: Agregar logging de diagnóstico para rastrear los 3 pulsos perdidos
   - **Código**:
     ```cpp
     // Al final de pinReceiverTask(), después del loop de stats
     if (now - lastStatsLog > 60000) {
         uint32_t totalSent = g_lastSentSequence;
         uint32_t expectedSent = g_totalPulsesDetected / g_pulseGroupSize;
         int32_t diff = expectedSent - totalSent;
         
         if (diff != 0) {
             Log::warn("Pulse count mismatch: detected=%lu, sent=%lu, diff=%ld",
                       expectedSent, totalSent, diff);
         }
         
         Log::info("Stats: totalDetected=%lu, totalSent=%lu, debounced=%lu",
                   g_totalPulsesDetected, totalSent, g_debouncedPulses);
     }
     ```
   - **Validación**: Verificar si `g_debouncedPulses == 3` durante el TC3

**4. Agregar métricas de fragmentación**
   - **Propósito**: Monitorear cuántos eventos se generan vs esperados
   - **Código**:
     ```cpp
     // En processPulseData()
     static uint32_t g_eventCount = 0;
     static uint32_t g_transitionFragments = 0;
     static uint32_t g_stableFragments = 0;
     
     g_eventCount++;
     if (isContinuation && transitionType == TRANSITION) {
         g_transitionFragments++;
     }
     if (isContinuation && transitionType == STABLE) {
         g_stableFragments++;
     }
     
     Log::debug("Event #%lu: type=%d, pulses=%llu, continuation=%d",
                g_eventCount, transitionType, pulseCount, isContinuation);
     ```

### Prioridad MEDIA 🟡

**5. Validar que EMA no sea demasiado agresivo**
   - **Archivo**: `pin_receiver.cpp:577`
   - **Código actual**: `g_stableAvgPeriod = (g_stableAvgPeriod * 9 + period) / 10;`
   - **Problema**: Con α=0.1, el promedio se adapta muy rápido a variaciones
   - **Análisis**:
     - 1 pulso de 32ms en promedio de 28ms → nuevo promedio = 28.4ms
     - 2 pulsos de 32ms → nuevo promedio = 28.8ms  
     - 3 pulsos de 32ms → nuevo promedio = 29.1ms
   - **Recomendación**: Considerar α=0.05 (más lento) o usar media simple
   - **Código alternativo**:
     ```cpp
     // Opción 1: EMA más lento (α=0.05)
     g_stableAvgPeriod = (g_stableAvgPeriod * 19 + period) / 20;
     
     // Opción 2: Media simple (más estable pero más lento)
     static uint64_t g_stablePeriodSum = 0;
     static uint32_t g_stablePeriodCount = 0;
     g_stablePeriodSum += period;
     g_stablePeriodCount++;
     g_stableAvgPeriod = g_stablePeriodSum / g_stablePeriodCount;
     ```

**6. Documentar umbral mínimo de período para cambio abrupto**
   - **Propósito**: Evitar falsos positivos en períodos muy cortos
   - **Código**:
     ```cpp
     static bool isAbruptChange(uint64_t currentPeriod, uint64_t avgPeriod) {
         if (avgPeriod == 0) return false;
         
         uint64_t diff = (currentPeriod > avgPeriod) 
                         ? (currentPeriod - avgPeriod) 
                         : (avgPeriod - currentPeriod);
         
         // Solo considerar cambio abrupto si la diferencia absoluta >3ms
         // Evita falsos positivos por jitter en períodos muy cortos
         const uint64_t MIN_ABSOLUTE_CHANGE_MS = 3;
         if (diff < MIN_ABSOLUTE_CHANGE_MS) {
             return false;
         }
         
         return (diff > avgPeriod * CHANGE_THRESHOLD);
     }
     ```

### Prioridad BAJA (Hardware) 🟢

**7. Verificar filtro RC en sensor (ya implementado en software)**
   - El debounce de 2ms en software es adecuado
   - No es necesario cambio de hardware

---

## 📊 Métricas de Calidad

| Métrica | Esperado | Real | ✅/❌ |
|---------|----------|------|-------|
| Pulsos totales | 177 | 174 | ❌ -3 |
| Eventos TRANSITION | 3-4 | 4 | ✅ |
| Eventos STABLE | 1-2 | 3 | ❌ +1-2 |
| Duración total | 8.6s | 8.6s | ✅ |
| Pulsos estable bajo | 76 | 67 | ❌ -9 |

**Tasa de error**: 3/177 = **1.7%** de pérdida de pulsos

---

## 🎯 Validación Esperada Post-Fix

Después de implementar las correcciones, el TC3 debería generar:

| Evento | Tipo | Pulsos Esperados | Periodo Avg |
|--------|------|------------------|-------------|
| 1 | TRANSITION | 48 | ~58ms (bajada 71→28ms) |
| 2 | STABLE | 76 | ~28ms |
| 3 | TRANSITION | 46 | ~50ms (subida 28→95ms) |
| 4 | TRANSITION (timeout) | 7 | ~85ms |

**Total**: 177 pulsos, 3-4 eventos

---

## 📝 Notas Técnicas

### Jitter Observado

El jitter configurado (3-4%) se observa correctamente:
```
Generado: 27.0 → 28.4 → 28.0 → 27.1 → 28.9ms
Variación: ±1.9ms ≈ ±6.8% (ligeramente alto pero aceptable)
```

### Timestamps NTP

Las conversiones NTP funcionan correctamente:
```
[INFO]: NTP conversion complete: seq=[5958-5975], 
        start=1767049417555, end=1767049418501
```

Duración calculada: 946ms vs esperado ~1s ✅

### Performance MQTT

Todos los mensajes se publican exitosamente sin pérdidas:
```
[DEBUG]: Message enqueued: queue=1/5
[INFO]: Published: 441 bytes
```

Queue nunca llena (máx 1/5), sistema estable ✅
