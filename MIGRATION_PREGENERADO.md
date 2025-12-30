# Migración a Sistema Pre-Generado de Pulsos

## Cambios Realizados

### 1. Estructuras Añadidas (línea ~140)
```cpp
// === SISTEMA DE PRE-GENERACIÓN DE PULSOS ===
#define MAX_PULSES 1000  
struct PulsePattern {
  float periods[MAX_PULSES];  
  int count;                   
  float frequencies[GRAPH_WIDTH]; 
  int freq_count;              
};

PulsePattern pulse_pattern;
int current_pulse_index = 0;  
bool pattern_ready = false;    
```

### 2. Función `preGenerarPatron()` Añadida (línea ~790)
- Pre-calcula TODOS los pulsos del test case en el array
- Genera datos para visualización en gráfico
- Se llama al iniciar generación

### 3. Función `getTestPhases()` Añadida (línea ~712)
- Devuelve puntero a fases del test case
- Elimina referencia a TC6

### 4. Test Case 6 ELIMINADO
- Eliminado de enum TestCase
- Eliminado test6_phases[] 
- Eliminado de getTestCasePeriod()

## Cambios Pendientes (HACER MANUALMENTE)

### 1. Reemplazar `generarPulsos()` completa (línea ~865)
Reemplazar TODO el contenido de la función por:

```cpp
void generarPulsos() {
  if (!pattern_ready) return;
  if (current_pulse_index >= pulse_pattern.count) {
    generating_pulse = false;
    current_gen_frequency = 0.0;
    digitalWrite(SENSOR_PIN, LOW);
    return;
  }
  
  unsigned long current_time = millis();
  
  if (next_pulse_time == 0) {
    pulse_interval = pulse_pattern.periods[current_pulse_index];
    next_pulse_time = current_time + (pulse_interval / 2);
    pulse_state = false;
    generating_pulse = true;
  }
  
  if (current_time >= next_pulse_time) {
    long timing_error = (long)current_time - (long)next_pulse_time;
    pulse_state = !pulse_state;
    digitalWrite(SENSOR_PIN, pulse_state ? HIGH : LOW);
    static unsigned long last_pulse_ts = 0;
    
    if (pulse_state) {
      unsigned long real_period = (last_pulse_ts > 0) ? (current_time - last_pulse_ts) : 0;
      current_pulse_index++;
      
      if (current_pulse_index < pulse_pattern.count) {
        pulse_interval = pulse_pattern.periods[current_pulse_index];
        current_gen_frequency = 1000.0 / pulse_interval;
      }
      
      if (current_pulse_index <= 10 || current_pulse_index % 100 == 0) {
        Serial.print("GEN #");
        Serial.print(current_pulse_index);
        Serial.print(": real=");
        Serial.print(real_period);
        Serial.print("ms, calc=");
        Serial.print(pulse_interval, 1);
        Serial.print("ms, err=");
        Serial.print(timing_error);
        Serial.println("ms");
      }
      
      last_pulse_ts = current_time;
    }
    
    next_pulse_time += (pulse_interval / 2);
  }
}
```

### 2. Modificar `inicializarGenerador()` (línea ~534)
Cambiar:
```cpp
current_test = TEST_CASE_6;  // CAMBIAR A: TEST_CASE_1
```

Y al final añadir:
```cpp
  // Pre-generar patrón y mostrar en gráfico
  preGenerarPatron(current_test);
  
  // Dibujar patrón en gráfico
  for (int i = 0; i < pulse_pattern.freq_count; i++) {
    graph_data[i] = pulse_pattern.frequencies[i];
  }
  graph_index = pulse_pattern.freq_count;
```

### 3. Actualizar mensajes Serial (línea ~545)
Eliminar referencia a TC6, cambiar a TC1 por defecto

### 4. Actualizar `cambiarModo(MODE_WRITE)` (línea ~1690)
Añadir después de `inicializarGenerador()`:
```cpp
// Dibujar gráfico con patrón pre-generado
inicializarGrafico();
for (int i = 0; i < pulse_pattern.freq_count; i++) {
  actualizarGrafico(pulse_pattern.frequencies[i]);
}
```

### 5. Actualizar `mostrarInfoTestCases()` (línea ~1145)
Cambiar array de 6 a 5 elementos:
```cpp
const char* tc_info[] = {
  "TC1: Rapid Start/Stop",
  "TC2: Normal Flow",  
  "TC3: Compound Event",
  "TC4: Stress Test",
  "TC5: Single Pulses"
};
```

Cambiar loop:
```cpp
for (int i = 0; i < 5; i++) {  // 5 en vez de 6
```

## Flujo Final

1. Usuario entra en MODE_WRITE
2. `inicializarGenerador()` llama `preGenerarPatron(TEST_CASE_1)`
3. Patrón se dibuja en gráfico (COMPLETO, no se actualiza más)
4. `generarPulsos()` reproduce desde array pre-calculado
5. NO hay conflictos TFT/timing porque gráfico ya está dibujado
6. Usuario cambia TC con botones → se regenera patrón y re-dibuja gráfico
