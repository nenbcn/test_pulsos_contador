# Secuencias de Pulsos - Test Cases Gateway

Este documento describe las secuencias exactas de pulsos generadas por cada Test Case del gateway TTGO T-Display. Úsalo para validar que el contador procesa correctamente los pulsos recibidos.

## Frecuencias Base Definidas

| Código | Período | Frecuencia | Descripción |
|--------|---------|------------|-------------|
| F1 | 23ms | 43.5 Hz | Flujo muy alto |
| F2 | 39ms | 25.6 Hz | Flujo alto |
| F3 | 50ms | 20.0 Hz | Flujo medio-alto |
| F4 | 89ms | 11.2 Hz | Flujo medio |
| F5 | 133ms | 7.5 Hz | Flujo bajo |
| F6 | 250ms | 4.0 Hz | Flujo muy bajo |
| F7 | 500ms | 2.0 Hz | Flujo mínimo |
| F8 | 10ms | 100 Hz | Límite debounce |

---

## TEST_CASE_1: Startup Simple
**Duración total**: 8.5 segundos  
**Propósito**: Simular arranque básico con transición gradual

### Secuencia de Pulsos

| Tiempo | Pulsos Aprox | Frecuencia | Descripción |
|--------|--------------|------------|-------------|
| 0-1330ms | ~10 | 7.52 Hz (F5) | Arranque inicial |
| 1330-1830ms | ~10 | Alternancia F5↔F3 | Transición (alterna cada ~91ms) |
| 1830-3000ms | ~30 | 25.64 Hz (F2) | Flujo estable |
| 3000ms+ | - | 0 Hz | Parada (timeout) |

**Total esperado**: ~50 pulsos

### Validación
- Debe detectar al menos 48-52 pulsos totales
- Frecuencia promedio en fase estable: 25.64 Hz
- Timeout de 2 segundos al final

---

## TEST_CASE_2: Fragmentación TRANSITION
**Duración total**: 10 segundos  
**Propósito**: Arranque gradual con 45 pulsos de transición escalonada

### Secuencia de Pulsos

| Tiempo | Pulsos | Frecuencia | Descripción |
|--------|--------|------------|-------------|
| 0-2250ms | 9 | 4.00 Hz (F6) | Escalón 1 |
| 2250-3447ms | 9 | 7.52 Hz (F5) | Escalón 2 |
| 3447-4248ms | 9 | 11.24 Hz (F4) | Escalón 3 |
| 4248-4698ms | 9 | 20.00 Hz (F3) | Escalón 4 |
| 4698-5049ms | 9 | 25.64 Hz (F2) | Escalón 5 |
| 5049-6219ms | 30 | 25.64 Hz (F2) | Flujo estable |
| 6219ms+ | - | 0 Hz | Parada |

**Total esperado**: ~75 pulsos (45 transición + 30 estables)

### Validación
- Debe detectar incrementos graduales de frecuencia
- 5 escalones distintos claramente identificables
- Total 73-77 pulsos

---

## TEST_CASE_4: Cambio de Flujo
**Duración total**: 3.5 segundos  
**Propósito**: Cambio brusco entre dos flujos estables

### Secuencia de Pulsos

| Tiempo | Pulsos | Frecuencia | Descripción |
|--------|--------|------------|-------------|
| 0-1170ms | 30 | 25.64 Hz (F2) | Flujo alto |
| 1170-2670ms | 30 | 20.00 Hz (F3) | Flujo medio-alto |
| 2670ms+ | - | 0 Hz | Parada |

**Total esperado**: 60 pulsos

### Validación
- Cambio claramente detectado en t≈1170ms
- Primera mitad: 25.64 Hz
- Segunda mitad: 20.00 Hz
- Total 58-62 pulsos

---

## TEST_CASE_5: SINGLE_PULSE
**Duración total**: 35 segundos  
**Propósito**: Pulsos aislados con timeouts largos (detección de fugas)

### Secuencia de Pulsos

| Tiempo | Duración Pulso | Frecuencia | Descripción |
|--------|----------------|------------|-------------|
| 0-50ms | 50ms | 25.64 Hz (F2) | Pulso aislado 1 |
| 50-5000ms | - | 0 Hz | Timeout 4.95s |
| 5000-5050ms | 50ms | 25.64 Hz (F2) | Pulso aislado 2 |
| 5050-15000ms | - | 0 Hz | Timeout 9.95s |
| 15000-15050ms | 50ms | 25.64 Hz (F2) | Pulso aislado 3 |
| 15050-30000ms | - | 0 Hz | Timeout 14.95s |
| 30000-30050ms | 50ms | 25.64 Hz (F2) | Pulso aislado 4 |
| 30050ms+ | - | 0 Hz | Final |

**Total esperado**: 4 pulsos aislados

### Validación
- 4 pulsos separados por timeouts largos (5s, 10s, 15s)
- Cada pulso de ~50ms de duración
- Frecuencia instantánea 25.64 Hz durante los pulsos
- Debe activar timeout/reset entre pulsos

---

## TEST_CASE_6: Parada Gradual
**Duración total**: 4.4 segundos  
**Propósito**: Desaceleración progresiva (cierre de grifo)

### Secuencia de Pulsos

| Tiempo | Pulsos | Frecuencia | Descripción |
|--------|--------|------------|-------------|
| 0-1170ms | 30 | 25.64 Hz (F2) | Flujo estable |
| 1170-1670ms | 10 | 20.00 Hz (F3) | Reducción 1 |
| 1670-2560ms | 10 | 11.24 Hz (F4) | Reducción 2 |
| 2560-3890ms | 10 | 7.52 Hz (F5) | Reducción 3 |
| 3890ms+ | - | 0 Hz | Parada final |

**Total esperado**: 60 pulsos

### Validación
- Desaceleración gradual en 4 etapas
- 25.64 → 20.00 → 11.24 → 7.52 Hz
- Total 58-62 pulsos
- Parada suave (no abrupta)

---

## TEST_CASE_9: Escenario Completo
**Duración total**: 271 segundos (4min 31s)  
**Propósito**: Uso doméstico realista con múltiples fases

### Secuencia de Pulsos

#### FASE 1: Apertura Total (0-62s)
| Tiempo | Pulsos Aprox | Frecuencia | Descripción |
|--------|--------------|------------|-------------|
| 0-266ms | ~2 | 7.52 Hz (F5) | Arranque gradual |
| 266-461ms | ~5 | 25.64 Hz (F2) | Aceleración |
| 461-700ms | ~10 | 43.48 Hz (F1) | Flujo máximo alcanzado |
| 700-60700ms | ~2608 | 43.48 Hz (F1) | Flujo máximo sostenido (60s) |
| 60700-61085ms | ~10 | 43.48 Hz (F1) | Inicio desaceleración |
| 61085-61670ms | ~15 | 25.64 Hz (F2) | Reducción |
| 61670-62000ms | ~2 | 7.52 Hz (F5) | Parada gradual |

**Subtotal Fase 1**: ~2652 pulsos

#### PAUSA 1: 62s-67s (5 segundos)
Sin pulsos (timeout)

#### FASE 2: Apertura Parcial (67s-255s)
| Tiempo | Pulsos Aprox | Frecuencia | Descripción |
|--------|--------------|------------|-------------|
| 67000-68330ms | ~10 | 7.52 Hz (F5) | Arranque suave |
| 68330-69000ms | ~13 | 20.00 Hz (F3) | Aceleración |
| 69000-251000ms | ~3640 | 20.00 Hz (F3) | Flujo medio sostenido (182s) |
| 251000-255000ms | ~30 | 7.52 Hz (F5) | Parada gradual |

**Subtotal Fase 2**: ~3693 pulsos

#### PAUSA 2 + MICROFUGA: 255s-271s
| Tiempo | Pulsos | Frecuencia | Descripción |
|--------|--------|------------|-------------|
| 255000-267000ms | - | 0 Hz | Pausa larga (12s) |
| 267000-267050ms | ~1 | 25.64 Hz (F2) | Microfuga (pulso aislado) |
| 267050ms+ | - | 0 Hz | Final |

**Subtotal Pausa+Fuga**: ~1 pulso

### Total Esperado TEST_CASE_9
**~6346 pulsos** en 271 segundos

### Validación
- FASE 1: Detectar flujo alto (~43.5 Hz) durante 60s
- PAUSA: Timeout de 5s
- FASE 2: Detectar flujo medio (~20 Hz) durante 180s
- PAUSA: Timeout de 12s
- MICROFUGA: Detectar pulso aislado al final
- Volumen total proporcional a ~6300-6400 pulsos

---

## TEST_LEGACY: Patrón Antiguo
**Duración**: 29 segundos (cíclico infinito)  
**Propósito**: Compatibilidad con patrón anterior

### Secuencia de Pulsos (un ciclo de 29s)

| Tiempo | Frecuencia | Descripción |
|--------|------------|-------------|
| 0-3000ms | 1→5→1 Hz | BURST1: rampa suave |
| 3000-6000ms | 0 Hz | PAUSE1 |
| 6000-9000ms | 30→50→30 Hz | BURST2: rampa rápida |
| 9000-12000ms | 0 Hz | PAUSE2 |
| 12000-12500ms | 80 Hz | STRESS_BURST escalón 1 |
| 12500-13000ms | 70 Hz | STRESS_BURST escalón 2 |
| 13000-13500ms | 60 Hz | STRESS_BURST escalón 3 |
| 13500-14000ms | 50 Hz | STRESS_BURST escalón 4 |
| 14000-22000ms | 40 Hz | STRESS_BURST sostenido |
| 22000-29000ms | 0 Hz | PAUSE3 |

**Total por ciclo**: ~380-420 pulsos

### Validación
- Detectar rampas suaves y abruptas
- Múltiples pausas/timeouts
- Frecuencias variables desde 1 Hz hasta 80 Hz
- El patrón se repite indefinidamente cada 29s

---

## Notas para Validación

### Tolerancias Esperadas
- **Conteo de pulsos**: ±2-5% del total esperado
- **Frecuencia instantánea**: ±1 Hz para frecuencias <30 Hz, ±2 Hz para >30 Hz
- **Timing de fases**: ±100ms debido a latencias del loop

### Errores Comunes a Detectar
1. **Pérdida de pulsos**: Total menor al 95% esperado
2. **Pulsos fantasma**: Total mayor al 105% esperado
3. **Frecuencia incorrecta**: Desviación >5% de la frecuencia objetivo
4. **Timeout no detectado**: Continúa contando después de pausa
5. **Cambios de fase perdidos**: No detecta transiciones

### Método de Validación Recomendado
1. Ejecutar cada test case completamente
2. Comparar total de pulsos detectados vs esperado
3. Verificar que las frecuencias medidas coincidan con las fases
4. Confirmar que los timeouts activen reset/pausa
5. Revisar que las transiciones sean detectadas correctamente

---

## Ejemplo de Análisis de Log

### Log del Gateway (TEST_CASE_1)
```
PULSE #1 - Expected: 100ms, Actual: 100ms, Error: 0ms, Freq: 7.52Hz, Period: 133.00ms
PULSE #10 - Expected: 1333ms, Actual: 1333ms, Error: 0ms, Freq: 7.52Hz, Period: 133.00ms
PULSE #20 - Expected: 1950ms, Actual: 1950ms, Error: 0ms, Freq: 25.64Hz, Period: 39.00ms
PULSE #50 - Expected: 3120ms, Actual: 3120ms, Error: 0ms, Freq: 25.64Hz, Period: 39.00ms
```

### Validación Correcta
✓ Primeros 10 pulsos a 7.52 Hz (133ms)  
✓ Transición detectada alrededor pulso #11-16  
✓ Pulsos 17-50 a 25.64 Hz (39ms)  
✓ Total: 50 pulsos en ~3 segundos  
✓ Errores de timing: 0ms (excelente precisión)

---

**Documento generado para validación de Test Cases - Gateway TTGO T-Display**  
**Versión**: 1.0  
**Fecha**: 29 Diciembre 2025
