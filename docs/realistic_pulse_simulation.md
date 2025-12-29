# Simulación Realista de Pulsos

## ⚙️ Configuración del Sistema

**Test Case por Defecto**: `TEST_CASE_6 (Secuencia Completa)`
- Al iniciar MODE_WRITE, se ejecuta automáticamente TEST_CASE_6 (~40s)
- Cambiar entre test cases: **Botón izquierdo** en MODE_WRITE
- Orden cíclico: TC6 → TC1 → TC2 → TC3 → TC4 → TC5 → TC6...

## Problema Original

El sistema anterior generaba pulsos con **frecuencias fijas** que cambiaban abruptamente:
- ❌ Cambios instantáneos de frecuencia (poco realista)
- ❌ No modelaba el comportamiento físico de arranque/parada
- ❌ Difícil validar transiciones del sistema receptor

## Nueva Arquitectura

### Concepto: Transiciones Progresivas

El sistema ahora modela el comportamiento **físico real** de un medidor de flujo:
- ✅ **Arranque progresivo**: El período entre pulsos disminuye gradualmente
- ✅ **Parada progresiva**: El período entre pulsos aumenta gradualmente  
- ✅ **Jitter natural**: Variación realista del período (±2-3%)
- ✅ **Fases estables**: Flujo constante con variación natural

### Estructura de Datos

```cpp
enum PhaseType {
  PHASE_TRANSITION_START,  // Arranque (período disminuye)
  PHASE_STABLE,            // Flujo estable con jitter
  PHASE_TRANSITION_STOP,   // Parada (período aumenta)
  PHASE_PAUSE              // Sin pulsos
};

struct PulsePhase {
  PhaseType type;
  unsigned long duration_ms;  // Duración de la fase
  float period_start_ms;      // Período inicial
  float period_end_ms;        // Período final
  float jitter_percent;       // Variación natural (%)
};
```

### Algoritmo de Transición

**Easing Exponencial** para simular aceleración física:

```cpp
float calcularPeriodoTransicion(float period_start, float period_end, float progress) {
  // progress: 0.0 → 1.0
  // easeOutExpo: suavidad realista
  float eased = (progress == 1.0) ? 1.0 : 1.0 - pow(2.0, -10.0 * progress);
  return period_start + (period_end - period_start) * eased;
}
```

**Jitter Natural** para variación realista:

```cpp
float aplicarJitter(float period, float jitter_percent, unsigned long seed) {
  // Variación determinista basada en tiempo
  // ±jitter_percent del período base
  float random_factor = pseudo_random(seed); // -1.0 a +1.0
  float variation = period * (jitter_percent / 100.0) * random_factor;
  return period + variation;
}
```

## Test Cases Realistas

### TEST CASE 1: Arranque Realista Simple
**Basado en logs reales** (`flow_compression_test_20251213_194807.txt`)

```cpp
PulsePhase test1_phases[] = {
  // Arranque progresivo (1.3s): 162ms → 102ms
  {PHASE_TRANSITION_START, 1300, 162.0, 102.0, 2.0},
  
  // Flujo estable (4s): ~102ms ≈ 10Hz
  {PHASE_STABLE, 4000, 102.0, 102.0, 3.0},
  
  // Parada progresiva (0.7s): 102ms → 200ms
  {PHASE_TRANSITION_STOP, 700, 102.0, 200.0, 2.0},
  
  // Pausa final
  {PHASE_PAUSE, 2000, 0, 0, 0}
};
```

**Comportamiento esperado:**
- **0-1.3s**: Arranque progresivo, períodos disminuyen exponencialmente
- **1.3-5.3s**: Flujo estable ≈10Hz con jitter ±3%
- **5.3-6.0s**: Parada progresiva, períodos aumentan
- **6.0-8.0s**: Sin pulsos

### TEST CASE 2: Arranque Gradual Multi-Escalón

```cpp
PulsePhase test2_phases[] = {
  // Arranque lento: 250ms → 150ms (2s)
  {PHASE_TRANSITION_START, 2000, 250.0, 150.0, 2.5},
  
  // Aceleración media: 150ms → 100ms (1.5s)
  {PHASE_TRANSITION_START, 1500, 150.0, 100.0, 2.0},
  
  // Aceleración final: 100ms → 80ms (1s)
  {PHASE_TRANSITION_START, 1000, 100.0, 80.0, 2.0},
  
  // Flujo alto estable: ~80ms ≈ 12.5Hz (5s)
  {PHASE_STABLE, 5000, 80.0, 80.0, 3.0},
  
  // Desaceleración rápida: 80ms → 150ms (0.8s)
  {PHASE_TRANSITION_STOP, 800, 80.0, 150.0, 2.0},
  
  // Parada final: 150ms → 300ms (0.5s)
  {PHASE_TRANSITION_STOP, 500, 150.0, 300.0, 2.0},
  
  {PHASE_PAUSE, 2000, 0, 0, 0}
};
```

**Comportamiento esperado:**
- Tres etapas de aceleración progresiva
- Flujo estable alto (12.5Hz)
- Dos etapas de desaceleración
- Duración total: ~13s

### TEST CASE 4: Cambio de Flujo con Transiciones

```cpp
PulsePhase test4_phases[] = {
  // Arranque rápido: 140ms → 90ms (0.6s)
  {PHASE_TRANSITION_START, 600, 140.0, 90.0, 2.0},
  
  // Flujo alto: ~90ms ≈ 11Hz (2.5s)
  {PHASE_STABLE, 2500, 90.0, 90.0, 3.0},
  
  // Transición: 90ms → 120ms (0.8s)
  {PHASE_TRANSITION_STOP, 800, 90.0, 120.0, 2.5},
  
  // Flujo medio: ~120ms ≈ 8.3Hz (2s)
  {PHASE_STABLE, 2000, 120.0, 120.0, 3.0},
  
  // Parada: 120ms → 250ms (1s)
  {PHASE_TRANSITION_STOP, 1000, 120.0, 250.0, 2.0},
  
  {PHASE_PAUSE, 2000, 0, 0, 0}
};
```

**Comportamiento esperado:**
- Arranque rápido a flujo alto
- Cambio gradual de flujo alto → medio
- Parada progresiva
- Duración total: ~10s

## Análisis de Logs Reales

### Ejemplo de arranque real (del archivo de logs):

```
[INFO]: State: IDLE → TRANSITION (first pulse, ts=733390, seq=1)
[DEBUG]: Pulse received: ts=733552, period=162, state=1, count=2
[DEBUG]: Pulse received: ts=733678, period=126, state=1, count=3
[DEBUG]: Pulse received: ts=733795, period=117, state=1, count=4
[DEBUG]: Pulse received: ts=733904, period=109, state=1, count=5
[DEBUG]: Pulse received: ts=734010, period=106, state=1, count=6
[DEBUG]: Pulse received: ts=734114, period=104, state=1, count=7
...
[DEBUG]: Pulse received: ts=734717, period=99, state=1, count=13
[DEBUG]: Pulse received: ts=734818, period=101, state=1, count=14
[INFO]: State: TRANSITION → STABLE (stabilized at 102 ms, pulses=14)
```

**Observaciones:**
- Período inicial: **162ms**
- Disminución progresiva: 162 → 126 → 117 → 109 → 106 → 104 → 102ms
- Se estabiliza en: **~102ms** (≈10Hz)
- Tiempo de transición: **~1.3s** (14 pulsos)
- Variación natural: ±2-3ms en estado estable

## Ventajas del Nuevo Sistema

### 1. Realismo
- Modela comportamiento físico real de sensores de flujo
- Transiciones suaves (no saltos bruscos)
- Jitter natural

### 2. Validación Mejorada
- Permite probar detección de transiciones
- Valida algoritmos de estabilización
- Simula condiciones reales de operación

### 3. Flexibilidad
- Fácil agregar nuevos casos de prueba
- Parametrizable (duraciones, períodos, jitter)
- Compatible con sistema legacy

### 4. Debugging
- Logs detallados de cada pulso
- Tracking de períodos y frecuencias
- Validación de timing

## Uso

### Seleccionar Test Case

En modo **WRITE**, usar el botón para cambiar entre test cases:
- **Botón Izquierdo**: Siguiente test case (orden: TC6 → TC1 → TC2 → TC3 → TC4 → TC5 → TC6...)
- **Inicio automático**: Sistema arranca con TC6 (Secuencia Completa)

### Monitorear Salida Serial

```
PULSE_GEN - Test: 1, Elapsed: 500ms, Period: 142.35ms, Freq: 7.02Hz
PULSE #1 - ts=733552, period=162.00ms, error=0ms
PULSE #2 - ts=733678, period=146.23ms, error=1ms
PULSE #3 - ts=733795, period=132.45ms, error=-1ms
...
```

## Extensión Futura

### Agregar Nuevo Test Case

```cpp
// 1. Definir las fases
PulsePhase mi_test_phases[] = {
  {PHASE_TRANSITION_START, duration, period_ini, period_fin, jitter},
  {PHASE_STABLE, duration, period, period, jitter},
  {PHASE_TRANSITION_STOP, duration, period_ini, period_fin, jitter},
  {PHASE_PAUSE, duration, 0, 0, 0}
};

// 2. Agregar al switch en getTestCasePeriod()
case MI_TEST_CASE:
  phases = mi_test_phases;
  num_phases = sizeof(mi_test_phases) / sizeof(mi_test_phases[0]);
  break;
```

## Referencias

- **Logs reales**: `/Users/nenbcn/Code/mica-gateway/test-results/test-cases/flow-compression/`
- **Easing functions**: https://easings.net/
- **Documentación anterior**: [test_cases_sequences.md](test_cases_sequences.md)
