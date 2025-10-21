# Implementación de Especificaciones de Pulsos

## Basado en: [GitHub - test_pulse_specifications.md](https://github.com/nenbcn/mica-gateway/blob/main/test/pulse-generator/test_pulse_specifications.md)

### Estructura del Ciclo Completo (29 segundos)

```cpp
enum PatternPhase {
    PHASE_BURST1,      // 3s - 50Hz con gradientes
    PHASE_PAUSE1,      // 3s - sin pulsos  
    PHASE_BURST2,      // 3s - 50Hz con gradientes
    PHASE_PAUSE2,      // 3s - sin pulsos
    PHASE_STRESS_BURST,// 10s - test de carga múltiples frecuencias
    PHASE_PAUSE3       // 7s - sin pulsos
};

unsigned long phase_durations[6] = {3000, 3000, 3000, 3000, 10000, 7000};
```

### Implementación de Gradientes

```cpp
// Función para frecuencia con gradiente en BURST (3s)
float getBurstFrequency(unsigned long phase_elapsed) {
    if (phase_elapsed < 500) {
        // Gradiente arranque: 30Hz → 50Hz en 500ms
        return 30.0 + (20.0 * phase_elapsed / 500.0);
    } else if (phase_elapsed > 2500) {
        // Gradiente parada: 50Hz → 30Hz en 500ms finales
        unsigned long remaining = 3000 - phase_elapsed;
        return 30.0 + (20.0 * remaining / 500.0);
    } else {
        // Frecuencia nominal: 50Hz estable
        return 50.0;
    }
}
```

### Implementación del Stress Test (10s)

```cpp
float getStressFrequency(unsigned long stress_elapsed) {
    // Patrón decreciente inicial (5s)
    if (stress_elapsed < 500) return 80.0;
    else if (stress_elapsed < 1000) return 70.0;
    else if (stress_elapsed < 1500) return 60.0;
    else if (stress_elapsed < 2000) return 50.0;
    else if (stress_elapsed < 2500) return 40.0;
    else if (stress_elapsed < 3000) return 35.0;
    else if (stress_elapsed < 3500) return 30.0;
    else if (stress_elapsed < 4000) return 25.0;
    else if (stress_elapsed < 4500) return 20.0;
    else if (stress_elapsed < 5000) return 15.0;
    
    // Segmento variable (2.5s) - cambio cada 100ms
    else if (stress_elapsed < 7500) {
        uint8_t segment = (stress_elapsed - 5000) / 100;
        // Frecuencias específicas según especificaciones (máximo 100Hz)
        float frequencies[] = {90, 70, 85, 45, 75, 35, 95, 60, 80, 25, 
                              88, 55, 78, 30, 92, 40, 82, 65, 86, 20,
                              96, 58, 74, 48, 100};
        return frequencies[segment % 25];
    }
    
    // Burst final alta frecuencia (2.5s) - máximo 100Hz
    else return 100.0;
}
```

### Métricas Esperadas

#### BURST (3s cada uno):
- **Total pulsos**: ~140-150 por burst
- **Eventos esperados**: ~9 eventos por burst
- **Frecuencias**: 30Hz → 35Hz → 40Hz → 45Hz → 50Hz → 45Hz → 40Hz → 35Hz → 30Hz

#### STRESS_BURST (10s):
- **Total pulsos**: ~640 pulsos  
- **Eventos esperados**: 55-60 eventos pequeños
- **Máxima frecuencia**: 100Hz (período 10ms)
- **Mínima frecuencia**: 15Hz (período 66.67ms)

### Implementación en Código Actual

```cpp
// Reemplazar la función generarPulsos() actual con:
void generarPulsosEspecificados() {
    unsigned long current_time = millis();
    unsigned long phase_elapsed = current_time - phase_start_time;
    
    // Verificar cambio de fase
    if (phase_elapsed >= phase_durations[current_phase]) {
        current_phase = (PatternPhase)((current_phase + 1) % 6);
        phase_start_time = current_time;
        phase_elapsed = 0;
        digitalWrite(SENSOR_PIN, LOW);
        pulse_state = false;
    }
    
    // Determinar frecuencia según fase
    float target_freq = 0.0;
    switch(current_phase) {
        case PHASE_BURST1:
        case PHASE_BURST2:
            target_freq = getBurstFrequency(phase_elapsed);
            break;
        case PHASE_STRESS_BURST:
            target_freq = getStressFrequency(phase_elapsed);
            break;
        default:
            target_freq = 0.0; // Pausas
            break;
    }
    
    // Generar pulsos si hay frecuencia
    if (target_freq > 0) {
        generating_pulse = true;
        current_gen_frequency = target_freq;
        pulse_interval = 1000.0 / target_freq;
        
        if (current_time >= next_pulse_time) {
            pulse_state = !pulse_state;
            digitalWrite(SENSOR_PIN, pulse_state ? HIGH : LOW);
            next_pulse_time = current_time + (pulse_interval / 2); // 50% duty cycle
        }
    } else {
        generating_pulse = false;
        current_gen_frequency = 0.0;
        digitalWrite(SENSOR_PIN, LOW);
    }
}
```

### Validación del Algoritmo

Con estas especificaciones, el dispositivo puede validar:
- **Tolerancia 12.5%**: Verificar que el algoritmo detecta eventos correctamente
- **Eventos múltiples**: STRESS_BURST genera 40-60 eventos para test de carga
- **Gradientes**: Validar detección de cambios graduales de frecuencia
- **Alta frecuencia**: Hasta 200Hz para test de rendimiento