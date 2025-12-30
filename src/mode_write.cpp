#include "mode_write.h"
#include "display.h"

// Variables específicas del modo WRITE
bool generating_pulse = false;
unsigned long next_pulse_time = 0;
float pulse_interval = 0;
float current_pulse_interval = 0;  // Intervalo del pulso actual (se mantiene durante HIGH y LOW)
float target_frequency = 50.0;
float current_gen_frequency = 0.0;
bool pulse_state = false;
unsigned long pulse_count_generated = 0;
TestCase current_test = TEST_CASE_1;
unsigned long test_start_time = 0;
PulsePattern pulse_pattern;
int current_pulse_index = 0;
bool pattern_ready = false;

// Variables de timing en microsegundos
unsigned long next_pulse_time_us = 0;
unsigned long last_pulse_ts_us = 0;
unsigned long pulse_end_time_us = 0;  // Tiempo en que debe terminar el pulso HIGH

// Arrays de fases para cada test case
// TC1: Arranque/Parada Rápidos (~1.5s)
PulsePhase test1_phases[] = {
  {PHASE_TRANSITION, 75.0, 43.0, 4, 3.0},   // Aceleración: 75→43ms, 4 pulsos
  {PHASE_TRANSITION, 43.0, 90.0, 5, 3.0}    // Frenado: 43→90ms, 5 pulsos
};

// TC2: Normal - Arranque-Estable-Parada (~6s)
PulsePhase test2_phases[] = {
  {PHASE_TRANSITION, 81.0, 48.0, 10, 3.0},  // Aceleración: 81→48ms, 10 pulsos
  {PHASE_STABLE, 49.0, 49.0, 60, 4.0},      // Estable: 49ms, 60 pulsos (~3s)
  {PHASE_TRANSITION, 49.0, 97.0, 15, 3.0}   // Frenado: 49→97ms, 15 pulsos
};

// TC3: Evento Compuesto - Grifo Adicional (~8.5s)
PulsePhase test3_phases[] = {
  {PHASE_TRANSITION, 73.0, 53.0, 8, 3.0},   // Aceleración inicial: 73→53ms
  {PHASE_STABLE, 53.0, 53.0, 28, 4.0},      // Flujo bajo: 53ms, 28 pulsos
  {PHASE_TRANSITION, 53.0, 28.0, 8, 3.0},   // Aceleración: 53→28ms (se abre más)
  {PHASE_STABLE, 28.0, 28.0, 71, 4.0},      // Flujo alto: 28ms, 71 pulsos (~2s)
  {PHASE_TRANSITION, 28.0, 55.0, 10, 3.0},  // Desaceleración: 28→55ms (cierra grifo)
  {PHASE_STABLE, 55.0, 55.0, 27, 4.0},      // Flujo bajo: 55ms, 27 pulsos
  {PHASE_TRANSITION, 55.0, 100.0, 12, 3.0}  // Frenado final: 55→100ms
};

// TC4: Stress Test - Flujo MUY Alto (~15s)
PulsePhase test4_phases[] = {
  {PHASE_TRANSITION, 65.0, 23.0, 8, 3.0},   // Aceleración rápida: 65→23ms
  {PHASE_STABLE, 23.0, 23.0, 543, 4.0},     // Flujo altísimo: 23ms, 543 pulsos (~12.5s)
  {PHASE_TRANSITION, 23.0, 80.0, 15, 3.0}   // Frenado: 23→80ms
};

// TC5: Pulsos Aislados - Fugas (~13s)
PulsePhase test5_phases[] = {
  {PHASE_STABLE, 2550.0, 2550.0, 4, 0},     // 4 pulsos separados 2.55s
  {PHASE_STABLE, 3050.0, 3050.0, 1, 0}      // Último pulso con pausa mayor
};

// Funciones auxiliares
float aplicarJitter(float tempo, float jitter_percent, unsigned long pulse_number) {
  if (jitter_percent <= 0.0) return tempo;
  
  unsigned long random_val = (pulse_number * 1103515245 + 12345) & 0x7FFFFFFF;
  float factor = ((random_val % 2000) / 1000.0) - 1.0;
  float variation = tempo * (jitter_percent / 100.0) * factor;
  return tempo + variation;
}

PulsePhase* getTestPhases(TestCase tc, int* phase_count) {
  switch (tc) {
    case TEST_CASE_1:
      *phase_count = sizeof(test1_phases) / sizeof(PulsePhase);
      return test1_phases;
    case TEST_CASE_2:
      *phase_count = sizeof(test2_phases) / sizeof(PulsePhase);
      return test2_phases;
    case TEST_CASE_3:
      *phase_count = sizeof(test3_phases) / sizeof(PulsePhase);
      return test3_phases;
    case TEST_CASE_4:
      *phase_count = sizeof(test4_phases) / sizeof(PulsePhase);
      return test4_phases;
    case TEST_CASE_5:
      *phase_count = sizeof(test5_phases) / sizeof(PulsePhase);
      return test5_phases;
    default:
      *phase_count = 0;
      return nullptr;
  }
}

void inicializarGenerador() {
  generating_pulse = false;
  next_pulse_time = 0;
  current_gen_frequency = 0.0;
  pulse_state = false;
  pattern_ready = false;
  current_pulse_index = 0;
  
  current_test = TEST_CASE_1;
  test_start_time = millis();
  
  Serial.println("Generador inicializado - Sistema de Test Cases:");
  Serial.println("Test Case activo: TEST_CASE_1 (Arranque/Parada Rápidos)");
  Serial.println("\n=== CASOS CON TRANSICIONES PROGRESIVAS (basados en logs reales) ===");
  Serial.println("  1: Arranque/Parada Rápidos (~1.5s)");
  Serial.println("     SIN fase estable - arranque 0.25s @ 23.5Hz → parada directa");
  Serial.println();
  Serial.println("  2: Normal - Arranque-Estable-Parada (~6s)");
  Serial.println("     Arranque 0.7s → Estable 3s @ 20.4Hz → Parada 1.1s");
  Serial.println("     [Basado en logs reales - 2x velocidad]");
  Serial.println();
  Serial.println("  3: Evento Compuesto - Grifo Adicional (~8.5s)");
  Serial.println("     Bajo 1.5s @ 19Hz → SUBE 0.4s → ALTO 2s @ 36.4Hz");
  Serial.println("     → BAJA 0.5s → bajo 1.5s → parada");
  Serial.println();
  Serial.println("  4: Stress Test - Flujo MUY Alto (~15s)");
  Serial.println("     Flujo altísimo 12.5s @ 44.4Hz → ~555 pulsos");
  Serial.println();
  Serial.println("  5: Single Pulse - Fugas/Pulsos Aislados (~7s)");
  Serial.println("     5 pulsos INDIVIDUALES con timeout de 1s entre ellos");
  Serial.println("\nUsar botones para cambiar test case en modo WRITE");
  
  preGenerarPatron(current_test);
  
  for (int i = 0; i < pulse_pattern.freq_count && i < GRAPH_WIDTH; i++) {
    graph_data[i] = pulse_pattern.frequencies[i];
  }
  graph_index = pulse_pattern.freq_count;
}

void preGenerarPatron(TestCase tc) {
  Serial.println("\n=== PRE-GENERANDO PATRÓN ===");
  
  pulse_pattern.count = 0;
  pulse_pattern.freq_count = 0;
  
  int phase_count = 0;
  PulsePhase* phases = getTestPhases(tc, &phase_count);
  
  if (!phases) {
    Serial.println("ERROR: Test case inválido");
    return;
  }
  
  unsigned long total_time_ms = 0;
  int global_pulse_num = 0;
  
  // Generar pulsos fase por fase
  for (int p = 0; p < phase_count && pulse_pattern.count < MAX_PULSES; p++) {
    PulsePhase* phase = &phases[p];
    
    for (int i = 0; i < phase->num_pulses && pulse_pattern.count < MAX_PULSES; i++) {
      float progress = (phase->num_pulses > 1) ? (float)i / (float)(phase->num_pulses - 1) : 0.0;
      float tempo;
      
      if (phase->type == PHASE_STABLE) {
        tempo = phase->tempo_start_ms;  // Tempo constante
      } else {
        // PHASE_TRANSITION: interpolación progresiva con easing
        float eased_progress = (progress == 1.0) ? 1.0 : 1.0 - pow(2.0, -10.0 * progress);
        tempo = phase->tempo_start_ms + (phase->tempo_end_ms - phase->tempo_start_ms) * eased_progress;
      }
      
      // Aplicar jitter
      tempo = aplicarJitter(tempo, phase->jitter_percent, global_pulse_num);
      
      pulse_pattern.periods[pulse_pattern.count] = tempo;
      pulse_pattern.count++;
      global_pulse_num++;
      total_time_ms += (unsigned long)tempo;
    }
  }
  
  unsigned long total_duration_ms = total_time_ms;
  
  pulse_pattern.freq_count = 0;
  unsigned long sample_interval = PULSE_CALC_INTERVAL_MS;
  int pulse_idx = 0;
  unsigned long cumulative_time = 0;
  
  for (int x = 0; x < GRAPH_WIDTH; x++) {
    unsigned long sample_time = x * sample_interval;
    
    if (sample_time >= total_duration_ms) {
      pulse_pattern.frequencies[pulse_pattern.freq_count] = 0;
      pulse_pattern.freq_count++;
      continue;
    }
    
    while (pulse_idx < pulse_pattern.count && cumulative_time + (unsigned long)pulse_pattern.periods[pulse_idx] <= sample_time) {
      cumulative_time += (unsigned long)pulse_pattern.periods[pulse_idx];
      pulse_idx++;
    }
    
    float freq = 0;
    if (pulse_idx < pulse_pattern.count && pulse_pattern.periods[pulse_idx] > 0) {
      freq = 1000.0 / pulse_pattern.periods[pulse_idx];
    }
    
    pulse_pattern.frequencies[pulse_pattern.freq_count] = freq;
    pulse_pattern.freq_count++;
  }
  
  Serial.print("✓ Patrón generado: ");
  Serial.print(pulse_pattern.count);
  Serial.print(" pulsos, ");
  Serial.print(total_duration_ms / 1000.0, 1);
  Serial.print("s, ");
  Serial.print(pulse_pattern.freq_count);
  Serial.println(" puntos gráfico");
  
  pattern_ready = true;
  current_pulse_index = 0;
}

void generarPulsos() {
  if (!pattern_ready) return;
  if (current_pulse_index >= pulse_pattern.count) {
    if (generating_pulse) {
      Serial.print("*** PATRÓN COMPLETADO: ");
      Serial.print(pulse_pattern.count);
      Serial.println(" pulsos generados ***");
    }
    generating_pulse = false;
    current_gen_frequency = 0.0;
    digitalWrite(SENSOR_PIN, LOW);
    return;
  }
  
  unsigned long current_time_us = micros();
  
  // Inicializar en el primer pulso
  if (next_pulse_time_us == 0) {
    next_pulse_time_us = current_time_us;
    last_pulse_ts_us = current_time_us;
    generating_pulse = true;
  }
  
  // Manejar fin del pulso HIGH (no bloqueante)
  if (pulse_state && current_time_us >= pulse_end_time_us) {
    digitalWrite(SENSOR_PIN, LOW);
    pulse_state = false;
  }
  
  // Generar nuevo pulso
  if (!pulse_state && current_time_us >= next_pulse_time_us) {
    long timing_error_us = (long)current_time_us - (long)next_pulse_time_us;
    
    // Iniciar pulso HIGH
    digitalWrite(SENSOR_PIN, HIGH);
    pulse_state = true;
    pulse_end_time_us = current_time_us + 5000;  // Termina en 5ms
    
    // Leer el período del array en ms (YA tiene el jitter aplicado)
    pulse_interval = pulse_pattern.periods[current_pulse_index];
    unsigned long pulse_interval_us = (unsigned long)(pulse_interval * 1000.0);
    
    unsigned long real_period_us = (last_pulse_ts_us > 0) ? (current_time_us - last_pulse_ts_us) : 0;
    
    // LOG del pulso generado
    Serial.print("[GEN] Pulse #");
    Serial.print(current_pulse_index + 1);
    Serial.print(": ts=");
    Serial.print(current_time_us / 1000);
    Serial.print("ms, period=");
    Serial.print(real_period_us / 1000.0, 1);
    Serial.print("ms, planned=");
    Serial.print(pulse_interval, 1);
    Serial.print("ms, err=");
    Serial.print(timing_error_us / 1000.0, 1);
    Serial.println("ms");
    
    last_pulse_ts_us = current_time_us;
    current_pulse_index++;
    current_gen_frequency = 1000.0 / pulse_interval;
    
    // Programar próximo pulso (basado en el tiempo ACTUAL, sin efecto del delay)
    next_pulse_time_us = current_time_us + pulse_interval_us;
  }
}

void manejarModoWrite() {
  generarPulsos();
}

void manejarBotonIzquierdoWrite() {
  current_test = (TestCase)((current_test + 1) % 5);
  
  generating_pulse = false;
  next_pulse_time = 0;
  next_pulse_time_us = 0;
  last_pulse_ts_us = 0;
  pulse_end_time_us = 0;
  pulse_state = false;
  pattern_ready = false;
  current_pulse_index = 0;
  test_start_time = millis();
  
  digitalWrite(SENSOR_PIN, LOW);  // Asegurar que el pin esté LOW
  
  Serial.println("Cambiando a: " + String(TEST_CASE_NAMES[current_test]));
  
  preGenerarPatron(current_test);
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(TEST_CASE_NAMES[current_test], 5, 5);
  
  inicializarGrafico();
  for (int i = 0; i < pulse_pattern.freq_count && i < GRAPH_WIDTH; i++) {
    actualizarGrafico(pulse_pattern.frequencies[i]);
  }
  
  // Mostrar modo DESPUÉS de dibujar todo (voltaje desactivado en WRITE)
  mostrarModo();
}
