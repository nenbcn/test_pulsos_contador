#include <Arduino.h>
#include <TFT_eSPI.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <Wire.h>
#include <float.h>

#define BUTTON_LEFT 0   // GPIO0 (izquierda)
#define BUTTON_RIGHT 35 // GPIO35 (derecha)
#define SENSOR_PIN 21   // GPIO21 (sensor de pulsos) - Cable VERDE

// --- Sensor de presión I2C Configuration ---
#define I2C_SDA 32      // GPIO32 para SDA - Cable AZUL ↔ Cable BLANCO sonda
#define I2C_SCL 22      // GPIO22 para SCL - Cable VERDE ↔ Cable AMARILLO sonda
#define WNK1MA_ADDR 0x6D
#define WNK1MA_CMD 0x06

// --- Constantes de Timing y Configuración ---
#define BUTTON_DEBOUNCE_MS 300       // Debounce de botones en milisegundos
#define PULSE_CALC_INTERVAL_MS 200   // Intervalo para calcular frecuencia de pulsos
#define VOLTAGE_UPDATE_MS 500        // Actualizar voltaje cada 500ms
#define WIFI_SCAN_INTERVAL_MS 10000  // Escanear WiFi cada 10 segundos
#define PRESSURE_READ_INTERVAL_MS 10 // Leer presión cada 10ms (100Hz)
#define SLEEP_TIMEOUT_MS 300000      // Sleep después de 5 minutos (300,000ms)
#define SERIAL_DEBUG_INTERVAL_MS 1000 // Debug por serial cada segundo
#define SERIAL_DEBUG_SLOW_MS 5000    // Debug lento cada 5 segundos
#define VOLTAGE_ADC_SAMPLES 32       // Número de muestras para promedio de voltaje

// --- Constantes del Gráfico ---
#define GRAPH_WIDTH 200
#define GRAPH_HEIGHT 60
#define GRAPH_X 20
#define GRAPH_Y 60

TFT_eSPI tft = TFT_eSPI();

// Variables para el voltaje
float voltaje = 0.0;
unsigned long last_voltage_update = 0;

// Variables para el sensor de pulsos
volatile unsigned long pulse_count = 0;
unsigned long last_pulse_count = 0;
unsigned long last_pulse_time = 0;
float pulse_frequency = 0.0;

// Variables para el gráfico
float graph_data[GRAPH_WIDTH];
int graph_index = 0;
float max_freq_scale = 100.0; // Escala fija del gráfico para cubrir hasta 80Hz (STRESS_BURST), mejor visibilidad para bajas frecuencias

// Variables para el sistema de modos
enum SystemMode {
  MODE_READ,
  MODE_WRITE,
  MODE_PRESSURE,
  MODE_WIFI_SCAN
};
SystemMode current_mode = MODE_READ;

// Variables para el generador de pulsos
bool generating_pulse = false;
unsigned long next_pulse_time = 0;
unsigned long pulse_interval = 0;
float target_frequency = 50.0; // Frecuencia fija de 50 Hz
float current_gen_frequency = 0.0;
bool pulse_state = false;

// Variables para el patrón sofisticado (29 segundos)
enum PatternPhase {
  PHASE_BURST1,       // 3s con gradientes 1Hz→5Hz→1Hz (test baja frecuencia)
  PHASE_PAUSE1,       // 3s parado
  PHASE_BURST2,       // 3s con gradientes 30Hz→50Hz→30Hz (frecuencias originales)
  PHASE_PAUSE2,       // 3s parado
  PHASE_STRESS_BURST, // 10s test de carga múltiples frecuencias
  PHASE_PAUSE3        // 7s parado
};
PatternPhase current_phase = PHASE_BURST1;
unsigned long phase_start_time = 0;
unsigned long phase_durations[6] = {3000, 3000, 3000, 3000, 10000, 7000}; // Duraciones en ms

// Variables para sleep
unsigned long last_user_activity_time = 0;  // Solo actividad del usuario (botones)
bool in_sleep_mode = false;

// Variables para WiFi Scan
struct WiFiNetwork {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t encryption;
  uint16_t color;
};
WiFiNetwork wifi_networks[20]; // Máximo 20 redes
int wifi_count = 0;
unsigned long last_wifi_scan = 0;
bool wifi_scanning = false;
int wifi_page = 0; // Página actual (0, 1, 2)
const int NETWORKS_PER_PAGE = 5; // Redes por página
const int MAX_WIFI_PAGES = 3; // Máximo 3 páginas

// Variables para el sensor de presión I2C
typedef struct {
    uint32_t rawValue;
    unsigned long timestamp;
    bool isValid;
} WNK1MA_Reading;

float pressure_graph_data[GRAPH_WIDTH];
int pressure_graph_index = 0;
float pressure_min_scale = 0.0;
float pressure_max_scale = 100.0;
float pressure_historical_min = FLT_MAX;  // Mínimo histórico desde inicio
float pressure_historical_max = FLT_MIN;  // Máximo histórico desde inicio
bool pressure_history_initialized = false; // Flag para primer valor
unsigned long last_pressure_read = 0;
const unsigned long PRESSURE_READ_INTERVAL = 10; // 100Hz = cada 10ms
bool pressure_auto_scale = true;

// Declaraciones forward
void enterSleepMode();
float getBurstFrequency(unsigned long phase_elapsed);
float getBurst2Frequency(unsigned long phase_elapsed);
float getStressFrequency(unsigned long stress_elapsed);

// Función de interrupción para contar pulsos
void IRAM_ATTR pulseInterrupt() {
  pulse_count++;
}

// Función de interrupción para despertar del sleep
void IRAM_ATTR wakeUpInterrupt() {
  // Esta función se ejecuta cuando se presiona cualquier botón
  // Solo necesita existir para despertar el ESP32
}

// Función para leer el sensor de presión WNK1MA por I2C
WNK1MA_Reading readWNK1MA() {
    WNK1MA_Reading reading;
    reading.isValid = false;
    reading.timestamp = millis();

    Wire.beginTransmission(WNK1MA_ADDR);
    Wire.write(WNK1MA_CMD);
    if (Wire.endTransmission(true) != 0) {
        Serial.println("[ERROR] I2C endTransmission failed");
        return reading; // Return invalid reading
    }
    
    delayMicroseconds(300); // Pause for sensor stabilization

    uint8_t bytesRead = Wire.requestFrom(WNK1MA_ADDR, 3);
    if (bytesRead == 3) {
        uint32_t raw = (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
        if (raw > 0 && raw < 16777215) { // Validate 24-bit range
            reading.rawValue = raw;
            reading.isValid = true;
        }
    } else {
        Serial.printf("[ERROR] I2C requestFrom failed, expected 3 bytes, got %d\n", bytesRead);
    }
    return reading;
}
void updateUserActivity() {
  last_user_activity_time = millis();
}

// Función para dibujar líneas de referencia (solo cuando cambia la escala)
void dibujarLineasReferencia() {
  // Dibujar líneas de referencia horizontales
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/2, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/2, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/4, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + 3*GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + 3*GRAPH_HEIGHT/4, TFT_DARKGREY);
}

// Función genérica para actualizar cualquier gráfico (OPTIMIZADA - líneas de referencia inteligentes)
void actualizarGraficoGenerico(float* data, int* index, float nuevo_valor, 
                                float min_scale, float max_scale, 
                                uint16_t color_fill, uint16_t color_line,
                                bool auto_scale = false) {
  // Añadir nuevo punto
  data[*index] = nuevo_valor;
  
  // Siempre limpiar área del gráfico (simplificado para mejor rendimiento)
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_BLACK);
  
  // Dibujar líneas de referencia (solo 3 líneas, muy eficiente)
  dibujarLineasReferencia();
  
  // Dibujar SOLO la línea del gráfico
  for (int i = 1; i < GRAPH_WIDTH; i++) {
    int idx1 = (*index - GRAPH_WIDTH + i + GRAPH_WIDTH) % GRAPH_WIDTH;
    int idx2 = (*index - GRAPH_WIDTH + i + 1 + GRAPH_WIDTH) % GRAPH_WIDTH;
    
    bool valid_data = auto_scale ? (data[idx1] != 0.0 && data[idx2] != 0.0) : true;
    
    if (valid_data) {
      int y1, y2;
      
      if (auto_scale) {
        float range = max_scale - min_scale;
        y1 = GRAPH_Y + GRAPH_HEIGHT - ((data[idx1] - min_scale) * GRAPH_HEIGHT / range);
        y2 = GRAPH_Y + GRAPH_HEIGHT - ((data[idx2] - min_scale) * GRAPH_HEIGHT / range);
      } else {
        y1 = GRAPH_Y + GRAPH_HEIGHT - (data[idx1] * GRAPH_HEIGHT / max_scale);
        y2 = GRAPH_Y + GRAPH_HEIGHT - (data[idx2] * GRAPH_HEIGHT / max_scale);
      }
      
      // Limitar valores dentro del gráfico
      y1 = constrain(y1, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT);
      y2 = constrain(y2, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT);
      
      // Dibujar línea del gráfico (línea doble para mejor visibilidad)
      tft.drawLine(GRAPH_X + i - 1, y1, GRAPH_X + i, y2, color_line);
      tft.drawLine(GRAPH_X + i - 1, y1 + 1, GRAPH_X + i, y2 + 1, color_line);
    }
  }
  
  // Avanzar índice
  *index = (*index + 1) % GRAPH_WIDTH;
}

// Función para actualizar histórico de presión y calcular escala
void actualizarHistoricoPresion(float nuevo_valor) {
  // Actualizar valores históricos
  if (!pressure_history_initialized) {
    pressure_historical_min = nuevo_valor;
    pressure_historical_max = nuevo_valor;
    pressure_history_initialized = true;
  } else {
    if (nuevo_valor < pressure_historical_min) {
      pressure_historical_min = nuevo_valor;
    }
    if (nuevo_valor > pressure_historical_max) {
      pressure_historical_max = nuevo_valor;
    }
  }
  
  // Calcular nueva escala con margen del 5%
  float range = pressure_historical_max - pressure_historical_min;
  if (range > 0) {
    pressure_min_scale = pressure_historical_min - (range * 0.05);
    pressure_max_scale = pressure_historical_max + (range * 0.05);
  } else {
    // Si todos los valores son iguales, usar rango por defecto
    pressure_min_scale = pressure_historical_min - 10.0;
    pressure_max_scale = pressure_historical_max + 10.0;
  }
}

// Función para actualizar el gráfico de presión con escala histórica
void actualizarGraficoPresion(float nuevo_valor) {
  // Primero actualizar el histórico y recalcular escala
  actualizarHistoricoPresion(nuevo_valor);
  
  // Luego actualizar el gráfico con la nueva escala
  actualizarGraficoGenerico(pressure_graph_data, &pressure_graph_index, nuevo_valor,
                            pressure_min_scale, pressure_max_scale,
                            TFT_BLUE, TFT_MAGENTA, pressure_auto_scale);
}

// Función para leer el voltaje de alimentación
float leerVoltaje() {
  // CONEXIÓN DIRECTA: Pin 3.3V conectado directamente a GPIO36 (VP)
  // Sin divisor de tensión - lectura directa del rail de 3.3V
  
  // Configurar ADC para GPIO36
  analogReadResolution(12);  // 0-4095
  analogSetAttenuation(ADC_11db); // Rango 0-3.3V (máximo rango)
  
  // Leer múltiples muestras para mayor precisión
  long suma = 0;
  for(int i = 0; i < VOLTAGE_ADC_SAMPLES; i++) {
    suma += analogRead(36); // GPIO36 (VP)
    delay(1);
  }
  
  float adc_reading = suma / (float)VOLTAGE_ADC_SAMPLES;
  
  // CALIBRACIÓN basada en mediciones empíricas:
  // 0V (GND) -> ADC marca 2.5V
  // 3.3V -> ADC marca 3.3V correctamente
  // Rango real del ADC: 2.5V a 3.3V (span de 0.8V)
  
  // Convertir ADC a voltaje sin calibrar
  float raw_voltage = (adc_reading * 3.3) / 4095.0;
  
  // Mapeo directo: span de 0.8V -> 0-3.3V
  float calibrated_voltage;
  if (raw_voltage >= 2.5) {
    calibrated_voltage = ((raw_voltage - 2.5) / 0.8) * 3.3;
  } else {
    calibrated_voltage = 0.0; // Por debajo del umbral mínimo
  }
  
  // Limitar a rangos realistas
  if (calibrated_voltage < 0.0) calibrated_voltage = 0.0;
  if (calibrated_voltage > 3.6) calibrated_voltage = 3.6;
  
  // Debug: mostrar valores cada 5 segundos para no saturar el serial
  static unsigned long last_debug = 0;
  if (millis() - last_debug > SERIAL_DEBUG_SLOW_MS) {
    Serial.print("DEBUG - ADC: ");
    Serial.print(adc_reading, 1);
    Serial.print(" | Raw: ");
    Serial.print(raw_voltage, 3);
    Serial.print("V | Calibrated: ");
    Serial.print(calibrated_voltage, 3);
    Serial.println("V");
    last_debug = millis();
  }
  
  return calibrated_voltage;
}

// Función para mostrar el voltaje en pantalla
void mostrarVoltaje() {
  static char last_voltage_text[10] = "";
  char voltage_text[10];
  
  snprintf(voltage_text, sizeof(voltage_text), "%.2fV", voltaje);
  
  // Solo actualizar si el texto ha cambiado
  if (strcmp(voltage_text, last_voltage_text) != 0) {
    // Borrar el texto anterior (área más grande para asegurar limpieza)
    tft.fillRect(170, 2, 65, 15, TFT_BLACK);
    
    // Dibujar el nuevo texto
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.setTextFont(2); // Usar una fuente más visible
    tft.drawString(voltage_text, 175, 5);
    
    strcpy(last_voltage_text, voltage_text);
  }
}

// Función para inicializar el generador de pulsos
void inicializarGenerador() {
  generating_pulse = false;
  next_pulse_time = 0;
  current_gen_frequency = 0.0;
  pulse_state = false;
  
  // Inicializar el patrón desde la primera fase
  current_phase = PHASE_BURST1;
  phase_start_time = millis();
  
  Serial.println("Generador inicializado - Patrón sofisticado (29s):");
  Serial.println("Fase 1: BURST1 (3s con gradientes 30-50-30Hz)");
  Serial.println("Fase 2: PAUSE1 (3s parado)");
  Serial.println("Fase 3: BURST2 (3s con gradientes 30-50-30Hz)");
  Serial.println("Fase 4: PAUSE2 (3s parado)");
  Serial.println("Fase 5: STRESS_BURST (10s test de carga 15-100Hz)");
  Serial.println("Fase 6: PAUSE3 (7s parado)");
  
  // VERIFICACIÓN INICIAL: Mostrar algunas frecuencias de muestra
  Serial.println("\n=== VERIFICACIÓN DE FRECUENCIAS ===");
  Serial.println("BURST1 - Bajas frecuencias (muestras):");
  Serial.print("  0ms: "); Serial.print(getBurstFrequency(0), 2); Serial.println("Hz (inicio: 1Hz)");
  Serial.print("  500ms: "); Serial.print(getBurstFrequency(500), 2); Serial.println("Hz (subida)");
  Serial.print("  1000ms: "); Serial.print(getBurstFrequency(1000), 2); Serial.println("Hz (pico: 5Hz)");
  Serial.print("  1500ms: "); Serial.print(getBurstFrequency(1500), 2); Serial.println("Hz (mantiene)");
  Serial.print("  2000ms: "); Serial.print(getBurstFrequency(2000), 2); Serial.println("Hz (inicio bajada)");
  Serial.print("  2500ms: "); Serial.print(getBurstFrequency(2500), 2); Serial.println("Hz (bajando)");
  Serial.print("  3000ms: "); Serial.print(getBurstFrequency(3000), 2); Serial.println("Hz (final: 1Hz)");
  
  Serial.println("BURST2 - Frecuencias originales (muestras):");
  Serial.print("  0ms: "); Serial.print(getBurst2Frequency(0), 2); Serial.println("Hz (inicio: 30Hz)");
  Serial.print("  250ms: "); Serial.print(getBurst2Frequency(250), 2); Serial.println("Hz (subida)");
  Serial.print("  500ms: "); Serial.print(getBurst2Frequency(500), 2); Serial.println("Hz (pico: 50Hz)");
  Serial.print("  1500ms: "); Serial.print(getBurst2Frequency(1500), 2); Serial.println("Hz (mantiene)");
  Serial.print("  2500ms: "); Serial.print(getBurst2Frequency(2500), 2); Serial.println("Hz (inicio bajada)");
  Serial.print("  2750ms: "); Serial.print(getBurst2Frequency(2750), 2); Serial.println("Hz (bajando)");
  Serial.print("  3000ms: "); Serial.print(getBurst2Frequency(3000), 2); Serial.println("Hz (final: 30Hz)");
  
  Serial.println("STRESS_BURST (muestras):");
  Serial.print("  0ms: "); Serial.print(getStressFrequency(0), 2); Serial.println("Hz");
  Serial.print("  2500ms: "); Serial.print(getStressFrequency(2500), 2); Serial.println("Hz");
  Serial.print("  5000ms: "); Serial.print(getStressFrequency(5000), 2); Serial.println("Hz");
  Serial.print("  6000ms: "); Serial.print(getStressFrequency(6000), 2); Serial.println("Hz");
  Serial.print("  7500ms: "); Serial.print(getStressFrequency(7500), 2); Serial.println("Hz");
  Serial.print("  9000ms: "); Serial.print(getStressFrequency(9000), 2); Serial.println("Hz");
  Serial.println("=========================================\n");
}

// Función para calcular frecuencia con gradiente en BURST (3s)
float getBurstFrequency(unsigned long phase_elapsed) {
  if (phase_elapsed < 1000) {
    // Gradiente arranque lento: 1Hz → 5Hz en 1000ms (1 segundo)
    return 1.0 + (4.0 * phase_elapsed / 1000.0);
  } else if (phase_elapsed > 2000) {
    // Gradiente parada lento: 5Hz → 1Hz en 1000ms finales
    unsigned long remaining = 3000 - phase_elapsed;
    return 1.0 + (4.0 * remaining / 1000.0);
  } else {
    // Frecuencia pico: 5Hz estable por 1 segundo
    return 5.0;
  }
}

// Función para calcular frecuencia con gradiente en BURST2 (3s) - Frecuencias originales
float getBurst2Frequency(unsigned long phase_elapsed) {
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

// Función para frecuencia en STRESS_BURST (10s)
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

// Función para generar pulsos con patrón sofisticado
void generarPulsos() {
  unsigned long current_time = millis();
  unsigned long phase_elapsed = current_time - phase_start_time;
  
  // Verificar si es tiempo de cambiar de fase
  if (phase_elapsed >= phase_durations[current_phase]) {
    // Cambiar a la siguiente fase
    current_phase = (PatternPhase)((current_phase + 1) % 6);
    phase_start_time = current_time;
    phase_elapsed = 0;
    
    // Detener pulsos al cambiar de fase
    digitalWrite(SENSOR_PIN, LOW);
    pulse_state = false;
    
    // Mostrar información de la nueva fase
    String phase_names[] = {"BURST1 (3s-gradientes)", "PAUSE1 (3s)", 
                           "BURST2 (3s-gradientes)", "PAUSE2 (3s)", 
                           "STRESS_BURST (10s-multicarga)", "PAUSE3 (7s)"};
    Serial.print("Cambio a fase: ");
    Serial.println(phase_names[current_phase]);
  }
  
  // Determinar frecuencia según fase
  float target_freq = 0.0;
  switch(current_phase) {
    case PHASE_BURST1:
      target_freq = getBurstFrequency(phase_elapsed);  // 1Hz→5Hz→1Hz
      break;
    case PHASE_BURST2:
      target_freq = getBurst2Frequency(phase_elapsed); // 30Hz→50Hz→30Hz
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
    current_gen_frequency = target_freq; // ASEGURAR: frecuencia mostrada = frecuencia generada
    pulse_interval = 1000.0 / target_freq;
    
    // LOG DETALLADO: Verificar coherencia frecuencia calculada vs mostrada
    static unsigned long last_freq_log = 0;
    static float last_logged_freq = -1;
    if (current_time - last_freq_log >= SERIAL_DEBUG_INTERVAL_MS) {
      if (abs(target_freq - last_logged_freq) > 0.1) { // Solo si hay cambio significativo
        Serial.print("FREQ_CHECK - Fase: ");
        Serial.print(current_phase);
        Serial.print(", Elapsed: ");
        Serial.print(phase_elapsed);
        Serial.print("ms, Target: ");
        Serial.print(target_freq, 2);
        Serial.print("Hz, Displayed: ");
        Serial.print(current_gen_frequency, 2);
        Serial.print("Hz, Period: ");
        Serial.print(target_freq > 0 ? (1000.0 / target_freq) : 0, 2);
        Serial.println("ms");
        last_logged_freq = target_freq;
      }
      last_freq_log = current_time;
    }
    
    // NO actualizar actividad durante generación automática
    // El sleep solo se controla por actividad del usuario (botones)
    
    if (current_time >= next_pulse_time) {
      pulse_state = !pulse_state;
      digitalWrite(SENSOR_PIN, pulse_state ? HIGH : LOW);
      next_pulse_time = current_time + (pulse_interval / 2); // 50% duty cycle
      
      // CONTADOR DE PULSOS: Verificar frecuencia real generada
      static unsigned long pulse_count_generated = 0;
      static unsigned long last_freq_measurement = 0;
      static unsigned long last_pulse_count_check = 0;
      
      if (pulse_state) { // Solo contar flancos positivos (pulsos completos)
        pulse_count_generated++;
      }
      
      // Medir frecuencia real cada 5 segundos
      if (current_time - last_freq_measurement >= SERIAL_DEBUG_SLOW_MS && last_freq_measurement > 0) {
        unsigned long time_elapsed = current_time - last_freq_measurement;
        unsigned long pulses_in_period = pulse_count_generated - last_pulse_count_check;
        float measured_freq = (pulses_in_period * 1000.0) / time_elapsed;
        
        Serial.print("REAL_FREQ_CHECK - Target: ");
        Serial.print(target_freq, 2);
        Serial.print("Hz, Measured: ");
        Serial.print(measured_freq, 2);
        Serial.print("Hz, Error: ");
        Serial.print(abs(measured_freq - target_freq), 2);
        Serial.print("Hz (");
        if (target_freq > 0) {
          Serial.print(abs(measured_freq - target_freq) / target_freq * 100, 1);
          Serial.println("%)");
        } else {
          Serial.println("N/A%)");
        }
        last_pulse_count_check = pulse_count_generated;
      }
      
      if (last_freq_measurement == 0 || current_time - last_freq_measurement >= SERIAL_DEBUG_SLOW_MS) {
        last_freq_measurement = current_time;
        last_pulse_count_check = pulse_count_generated;
      }
    }
  } else {
    // Fases de pausa
    generating_pulse = false;
    current_gen_frequency = 0.0;
    digitalWrite(SENSOR_PIN, LOW);
    pulse_state = false;
  }
}

// Función para determinar el color basado en la intensidad de señal (RSSI)
uint16_t getSignalColor(int32_t rssi) {
  // Criterios para ESP32-C3:
  // -30 a -50 dBm: Excelente (Verde brillante)
  // -51 a -65 dBm: Buena (Verde)
  // -66 a -75 dBm: Aceptable (Naranja/Amarillo)
  // -76 a -85 dBm: Débil pero funcional (Naranja)
  // < -85 dBm: Muy débil, inestable (Rojo)
  
  if (rssi >= -50) {
    return 0x07E0;         // Verde brillante
  } else if (rssi >= -65) {
    return TFT_GREEN;      // Verde
  } else if (rssi >= -75) {
    return TFT_YELLOW;     // Amarillo (aceptable)
  } else if (rssi >= -85) {
    return TFT_ORANGE;     // Naranja (débil)
  } else {
    return TFT_RED;        // Rojo (muy débil)
  }
}

// Función para escanear redes WiFi
void escanearWiFi() {
  if (wifi_scanning) return; // Ya escaneando
  
  wifi_scanning = true;
  Serial.println("Iniciando escaneo WiFi...");
  
  // Configurar WiFi en modo estación
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Iniciar escaneo
  int network_count = WiFi.scanNetworks(false, false, false, 300, 0); // Scan asíncrono
  
  wifi_count = min(network_count, 20); // Limitar a 20 redes máximo
  
  if (network_count > 0) {
    Serial.printf("Encontradas %d redes:\n", network_count);
    
    // Copiar datos de las redes encontradas
    for (int i = 0; i < wifi_count; i++) {
      wifi_networks[i].ssid = WiFi.SSID(i);
      wifi_networks[i].rssi = WiFi.RSSI(i);
      wifi_networks[i].encryption = WiFi.encryptionType(i);
      wifi_networks[i].color = getSignalColor(wifi_networks[i].rssi);
      
      Serial.printf("  %d: %s (%d dBm) %s\n", 
                    i + 1, 
                    wifi_networks[i].ssid.c_str(), 
                    wifi_networks[i].rssi,
                    (wifi_networks[i].encryption == WIFI_AUTH_OPEN) ? "[ABIERTA]" : "[PROTEGIDA]");
    }
  } else {
    Serial.println("No se encontraron redes WiFi");
    wifi_count = 0;
  }
  
  // Limpiar resultados del scan
  WiFi.scanDelete();
  wifi_scanning = false;
  last_wifi_scan = millis();
}

// Función para mostrar la lista de redes WiFi
void mostrarListaWiFi() {
  static int last_wifi_count = -1;
  static int last_page = -1;
  
  // Solo redibujar si hay cambios
  if (wifi_count != last_wifi_count || wifi_page != last_page) {
    // Limpiar área de contenido ampliada
    tft.fillRect(0, 22, 240, 113, TFT_BLACK);
    
    // Solo título compacto en una línea
    tft.fillRect(0, 0, 240, 20, TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(1);
    tft.setTextFont(2);
    tft.drawString("SCANNER WiFi", 5, 2);
    
    // Página actual (lado derecho del título)
    tft.setTextColor(TFT_MAGENTA);
    tft.setTextFont(2);
    String page_info = "Pag " + String(wifi_page + 1) + "/3";
    tft.drawString(page_info, 140, 2);
    
    // Estado/info muy compacto en la misma línea del título si es necesario
    if (wifi_scanning) {
      tft.setTextColor(TFT_YELLOW);
      tft.setTextFont(1);
      tft.drawString("Scan...", 200, 5);
    } else {
      // Tiempo hasta próximo escaneo solo si hay espacio
      unsigned long next_scan = last_wifi_scan + WIFI_SCAN_INTERVAL_MS;
      unsigned long remaining = (next_scan > millis()) ? (next_scan - millis()) / 1000 : 0;
      if (remaining > 0) {
        tft.setTextColor(TFT_DARKGREY);
        tft.setTextFont(1);
        tft.drawString(String(remaining) + "s", 210, 5);
      }
    }
    
    // Calcular redes a mostrar según la página actual
    int start_idx = wifi_page * NETWORKS_PER_PAGE;
    int end_idx = min(start_idx + NETWORKS_PER_PAGE, wifi_count);
    
    // Mostrar redes con más espacio (empezar más arriba)
    for (int i = start_idx; i < end_idx; i++) {
      int y_pos = 25 + (i - start_idx) * 20; // Más espacio entre líneas
      
      // Indicador de calidad (círculo coloreado)
      tft.fillCircle(10, y_pos + 10, 6, wifi_networks[i].color);
      tft.drawCircle(10, y_pos + 10, 6, TFT_WHITE);
      
      // SSID con fuente más grande y mucho más espacio
      String ssid = wifi_networks[i].ssid;
      if (ssid.length() > 20) {  // Mucho más espacio para el nombre
        ssid = ssid.substring(0, 20) + "..";
      }
      
      tft.setTextColor(TFT_WHITE);
      tft.setTextFont(2); // Fuente más grande
      tft.drawString(ssid, 22, y_pos + 4);
      
      // RSSI más a la derecha con más espacio
      tft.setTextColor(wifi_networks[i].color);
      tft.setTextFont(1);
      String rssi_text = String(wifi_networks[i].rssi);
      tft.drawString(rssi_text, 200, y_pos + 8);
    }
    
    // Mostrar indicadores de navegación si hay redes (más abajo)
    if (wifi_count > 0) {
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextFont(1);
      
      // Indicadores de páginas disponibles en la parte inferior
      for (int p = 0; p < MAX_WIFI_PAGES; p++) {
        int page_start = p * NETWORKS_PER_PAGE;
        if (page_start < wifi_count) {
          uint16_t color = (p == wifi_page) ? TFT_WHITE : TFT_DARKGREY;
          tft.setTextColor(color);
          tft.drawString(String(p + 1), 200 + p * 10, 130);
        }
      }
      
      // Instrucción compacta
      tft.setTextColor(TFT_DARKGREY);
      tft.drawString("Izq: Pag", 5, 130);
      
      // Info de redes encontradas
      tft.setTextColor(TFT_DARKGREY);
      String info = String(wifi_count) + " redes";
      tft.drawString(info, 80, 130);
    }
    
    last_wifi_count = wifi_count;
    last_page = wifi_page;
  }
}

// Función para mostrar el modo actual
void mostrarModo() {
  static SystemMode last_mode = MODE_WIFI_SCAN; // Inicializar con valor diferente para forzar actualización
  
  if (current_mode != last_mode) {
    // Limpiar área del modo - área más pequeña solo para el texto del modo
    tft.fillRect(170, 20, 65, 20, TFT_BLACK);
    
    tft.setTextSize(1);
    tft.setTextFont(2);
    
    if (current_mode == MODE_READ) {
      tft.setTextColor(TFT_GREEN);
      tft.drawString("READ", 175, 25);
    } else if (current_mode == MODE_WRITE) {
      tft.setTextColor(TFT_RED);
      tft.drawString("WRITE", 175, 25);
    } else if (current_mode == MODE_PRESSURE) {
      tft.setTextColor(TFT_MAGENTA);
      tft.drawString("PRES", 175, 25);
    } else if (current_mode == MODE_WIFI_SCAN) {
      tft.setTextColor(TFT_CYAN);
      tft.drawString("WiFi", 175, 25);
    }
    
    last_mode = current_mode;
  }
}

// Función para mostrar pantalla de scanning inmediatamente
void mostrarPantallaScanningWiFi() {
  // Limpiar toda la pantalla
  tft.fillScreen(TFT_BLACK);
  
  // Título compacto
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.drawString("SCANNER WiFi", 5, 5);
  
  // Mensaje de scanning más grande
  tft.setTextColor(TFT_YELLOW);
  tft.setTextFont(2);
  tft.drawString("Escaneando...", 5, 35);
  
  // Mensaje informativo
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(1);
  tft.drawString("Buscando redes disponibles", 5, 55);
  
  // Mostrar indicador de modo
  mostrarModo();
  
  // Mostrar voltaje
  mostrarVoltaje();
  
  // Animación simple - barra de progreso más grande
  for (int i = 0; i < 200; i += 20) {
    tft.drawLine(20 + i, 80, 20 + i + 15, 80, TFT_CYAN);
    tft.drawLine(20 + i, 81, 20 + i + 15, 81, TFT_CYAN); // Línea doble para más grosor
    delay(50);
  }
}

// Función para dibujar marco y grid del gráfico
void dibujarMarcoGrafico(bool es_presion) {
  // Dibujar marco del gráfico
  tft.drawRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_WIDTH + 2, GRAPH_HEIGHT + 2, TFT_WHITE);
  
  // Líneas de referencia (grid horizontal)
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/2, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/2, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/4, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + 3*GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + 3*GRAPH_HEIGHT/4, TFT_DARKGREY);
  
  // Etiquetas de escala según el tipo de gráfico
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextFont(1);
  
  if (es_presion) {
    // Etiquetas de escala para presión (escalado histórico)
    tft.drawString("HIST", GRAPH_X - 18, GRAPH_Y - 2);
    tft.drawString("MAX", GRAPH_X - 15, GRAPH_Y + GRAPH_HEIGHT - 2);
  } else {
    // Etiquetas de escala para pulsos (120Hz fijo)
    tft.drawString("120", GRAPH_X - 18, GRAPH_Y - 2);        // Máximo (120 Hz)
    tft.drawString("90", GRAPH_X - 15, GRAPH_Y + GRAPH_HEIGHT/4 - 2);  // 90 Hz
    tft.drawString("60", GRAPH_X - 15, GRAPH_Y + GRAPH_HEIGHT/2 - 2);  // 60 Hz
    tft.drawString("30", GRAPH_X - 15, GRAPH_Y + 3*GRAPH_HEIGHT/4 - 2); // 30 Hz
    tft.drawString("0", GRAPH_X - 8, GRAPH_Y + GRAPH_HEIGHT - 2);      // Mínimo (0 Hz)
  }
}

void inicializarGrafico() {
  // Inicializar array del gráfico de pulsos con ceros
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    graph_data[i] = 0.0;
  }
  
  // Inicializar array del gráfico de presión con ceros
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    pressure_graph_data[i] = 0.0;
  }
  
  // Dibujar marco y grid según el modo actual
  dibujarMarcoGrafico(current_mode == MODE_PRESSURE);
}

// Función para añadir un punto al gráfico
void actualizarGrafico(float nueva_frecuencia) {
  // Usar función genérica con escala fija (0 a max_freq_scale)
  actualizarGraficoGenerico(graph_data, &graph_index, nueva_frecuencia,
                            0.0, max_freq_scale,
                            TFT_BLUE, TFT_CYAN, false);
}

// Función para mostrar información del sensor
void mostrarInfoSensor() {
  static char last_freq_text[30] = "";
  static char last_total_text[40] = "";
  static char last_scale_text[20] = "";
  static char last_phase_text[50] = "";
  
  if (current_mode == MODE_READ) {
    // Mostrar frecuencia leída
    char freq_text[30];
    snprintf(freq_text, sizeof(freq_text), "Freq: %.1f Hz", pulse_frequency);
    if (strcmp(freq_text, last_freq_text) != 0) {
      tft.fillRect(5, 5, 160, 15, TFT_BLACK);
      tft.setTextColor(TFT_YELLOW);
      tft.setTextSize(1);
      tft.setTextFont(2);
      tft.drawString(freq_text, 5, 5);
      strcpy(last_freq_text, freq_text);
    }
    
    // Mostrar total de pulsos
    char total_text[30];
    snprintf(total_text, sizeof(total_text), "Total: %lu", pulse_count);
    if (strcmp(total_text, last_total_text) != 0) {
      tft.fillRect(5, 25, 160, 15, TFT_BLACK);
      tft.setTextColor(TFT_GREEN);
      tft.setTextSize(1);
      tft.setTextFont(2);
      tft.drawString(total_text, 5, 25);
      strcpy(last_total_text, total_text);
    }
    
    // Mostrar escala fija del gráfico
    char scale_text[20];
    snprintf(scale_text, sizeof(scale_text), "Max: %dHz", (int)max_freq_scale);
    if (strcmp(scale_text, last_scale_text) != 0) {
      tft.fillRect(5, 52, 100, 8, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextFont(1);
      tft.drawString(scale_text, 5, 52);
      strcpy(last_scale_text, scale_text);
    }
  } else if (current_mode == MODE_WRITE) {
    // Mostrar frecuencia generada con verificación de coherencia
    char freq_text[30];
    snprintf(freq_text, sizeof(freq_text), "Gen: %.1f Hz", current_gen_frequency);
    if (strcmp(freq_text, last_freq_text) != 0) {
      tft.fillRect(5, 5, 160, 15, TFT_BLACK);
      tft.setTextColor(TFT_ORANGE);
      tft.setTextSize(1);
      tft.setTextFont(2);
      tft.drawString(freq_text, 5, 5);
      strcpy(last_freq_text, freq_text);
      
      // LOG: Verificar que la frecuencia mostrada en pantalla coincide con la generada
      Serial.print("DISPLAY_CHECK - Showing: ");
      Serial.print(current_gen_frequency, 2);
      Serial.print("Hz, Period: ");
      Serial.print(current_gen_frequency > 0 ? (1000.0 / current_gen_frequency) : 0, 2);
      Serial.println("ms");
    }
    
    // Mostrar fase actual y tiempo restante
    unsigned long phase_elapsed = millis() - phase_start_time;
    float remaining = (phase_durations[current_phase] - phase_elapsed) / 1000.0;
    
    const char* phase_names[] = {"Burst1", "Pause1", "Burst2", "Pause2", "Stress", "Pause3"};
    char status_text[40];
    snprintf(status_text, sizeof(status_text), "%s: %.1fs", phase_names[current_phase], remaining);
    
    if (strcmp(status_text, last_total_text) != 0) {
      tft.fillRect(5, 25, 160, 15, TFT_BLACK);
      uint16_t color;
      if (current_phase == PHASE_BURST1 || current_phase == PHASE_BURST2) {
        color = TFT_RED; // Burst con gradientes
      } else if (current_phase == PHASE_STRESS_BURST) {
        color = TFT_MAGENTA; // Stress test
      } else {
        color = TFT_CYAN; // Pausas
      }
      tft.setTextColor(color);
      tft.setTextSize(1);
      tft.setTextFont(2);
      tft.drawString(status_text, 5, 25);
      strcpy(last_total_text, status_text);
    }
    
    // Mostrar patrón completo actualizado
    const char* pattern_text = "Gradientes+Stress: 29s total (max 100Hz)";
    if (strcmp(pattern_text, last_phase_text) != 0) {
      tft.fillRect(5, 40, 230, 10, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextFont(1);
      tft.drawString(pattern_text, 5, 40);
      strcpy(last_phase_text, pattern_text);
    }
    
    // Mostrar escala fija del gráfico
    char scale_text[20];
    snprintf(scale_text, sizeof(scale_text), "Max: %dHz", (int)max_freq_scale);
    if (strcmp(scale_text, last_scale_text) != 0) {
      tft.fillRect(5, 52, 100, 8, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextFont(1);
      tft.drawString(scale_text, 5, 52);
      strcpy(last_scale_text, scale_text);
    }
  } else if (current_mode == MODE_PRESSURE) {
    // Mostrar valor de presión actual
    static float last_pressure_value = -1.0;
    float current_pressure = pressure_graph_data[(pressure_graph_index - 1 + GRAPH_WIDTH) % GRAPH_WIDTH];
    
    if (current_pressure != last_pressure_value) {
      char pressure_text[30];
      snprintf(pressure_text, sizeof(pressure_text), "Presion: %.0f", current_pressure);
      tft.fillRect(5, 5, 160, 15, TFT_BLACK);
      tft.setTextColor(TFT_MAGENTA);
      tft.setTextSize(1);
      tft.setTextFont(2);
      tft.drawString(pressure_text, 5, 5);
      last_pressure_value = current_pressure;
    }
    
    // Mostrar rango de escala histórico
    char range_text[40];
    snprintf(range_text, sizeof(range_text), "Min:%.0f Max:%.0f", pressure_historical_min, pressure_historical_max);
    if (strcmp(range_text, last_total_text) != 0) {
      tft.fillRect(5, 25, 200, 15, TFT_BLACK); // Área más ancha para el texto
      tft.setTextColor(TFT_CYAN);
      tft.setTextSize(1);
      tft.setTextFont(1); // Fuente más pequeña para que quepa
      tft.drawString(range_text, 5, 25);
      strcpy(last_total_text, range_text);
    }
    
    // Mostrar información del modo con histórico
    const char* mode_text = "Sensor I2C @ 100Hz (Historico)";
    if (strcmp(mode_text, last_phase_text) != 0) {
      tft.fillRect(5, 40, 230, 10, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextFont(1);
      tft.drawString(mode_text, 5, 40);
      strcpy(last_phase_text, mode_text);
    }
    
    // Mostrar escalado histórico
    const char* scale_text = "Escala: HISTORICA";
    if (strcmp(scale_text, last_scale_text) != 0) {
      tft.fillRect(5, 52, 120, 8, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextFont(1);
      tft.drawString(scale_text, 5, 52);
      strcpy(last_scale_text, scale_text);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Verificar si despertamos del sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool waking_from_sleep = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1);
  
  if (waking_from_sleep) {
    Serial.println("=== DESPERTANDO DEL SLEEP ===");
  } else {
    Serial.println("=== INICIO NORMAL ===");
  }
  
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH); // Activa retroiluminación

  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  
  // Configurar el pin del sensor/generador
  pinMode(SENSOR_PIN, INPUT); // Inicialmente en modo lectura
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseInterrupt, RISING);

  // Inicializar I2C para el sensor de presión
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // 100kHz for stability
  Serial.println("I2C inicializado para sensor de presión WNK1MA");

  tft.init();
  tft.setRotation(1); // Horizontal/apaisado
  tft.fillScreen(TFT_BLACK);

  // Inicializar gráfico y generador
  inicializarGrafico();
  inicializarGenerador();
  
  // Leer y mostrar voltaje inicial
  voltaje = leerVoltaje();
  mostrarVoltaje();
  mostrarInfoSensor();
  mostrarModo();
  
  // Inicializar timer de actividad del usuario
  last_user_activity_time = millis();
  in_sleep_mode = false;
  
  Serial.println("=== TTGO T-Display - Monitor/Generador/Presión/WiFi Scanner ===");
  Serial.println("GPIO21 - Sensor/Generador configurado");
  Serial.println("GPIO32/22 - I2C para sensor de presión WNK1MA (SDA/SCL)");
  Serial.println("Botón IZQUIERDO: Cambiar página en WiFi Scanner");
  Serial.println("Botón DERECHO: Ciclar READ->WRITE->PRESSURE->WiFi->READ");
  Serial.println("Sleep automático: 5 minutos sin actividad de BOTONES");
  Serial.println("Escala gráfico: 0-75Hz (fija) / AUTO (presión)");
  Serial.println("Modo inicial: LECTURA");
  
  if (waking_from_sleep) {
    Serial.println("Sistema reactivado desde sleep");
  }
}

// Función para manejar botón izquierdo
void manejarBotonIzquierdo() {
  updateUserActivity();
  
  if (current_mode == MODE_WIFI_SCAN) {
    // En modo WiFi: cambiar página (cíclico)
    wifi_page = (wifi_page + 1) % MAX_WIFI_PAGES;
    Serial.println("Cambiando a página WiFi: " + String(wifi_page + 1));
  }
  // Nota: En otros modos el botón izquierdo no tiene función asignada
}

// Función centralizada para cambiar de modo
void cambiarModo(SystemMode nuevo_modo) {
  if (nuevo_modo == current_mode) return; // No cambiar si es el mismo modo
  
  SystemMode modo_anterior = current_mode;
  current_mode = nuevo_modo;
  
  // Lógica común de transición
  updateUserActivity();
  
  // Lógica específica según el nuevo modo
  switch (nuevo_modo) {
    case MODE_READ:
      // Configurar pin para lectura
      pinMode(SENSOR_PIN, INPUT);
      attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseInterrupt, RISING);
      inicializarGenerador(); // Detener generación si estaba activa
      // Limpiar pantalla para mostrar gráfico
      tft.fillScreen(TFT_BLACK);
      inicializarGrafico();
      mostrarModo();
      Serial.println("Cambiado a MODO LECTURA");
      break;
      
    case MODE_WRITE:
      // Configurar pin para escritura
      detachInterrupt(digitalPinToInterrupt(SENSOR_PIN));
      pinMode(SENSOR_PIN, OUTPUT);
      digitalWrite(SENSOR_PIN, LOW);
      inicializarGenerador();
      mostrarModo();
      Serial.println("Cambiado a MODO ESCRITURA");
      break;
      
    case MODE_PRESSURE:
      // Restaurar pin del sensor
      pinMode(SENSOR_PIN, INPUT);
      digitalWrite(SENSOR_PIN, LOW);
      // Resetear histórico de presión al cambiar a este modo
      pressure_history_initialized = false;
      pressure_historical_min = FLT_MAX;
      pressure_historical_max = FLT_MIN;
      // Limpiar pantalla para mostrar gráfico de presión
      tft.fillScreen(TFT_BLACK);
      inicializarGrafico();
      mostrarModo();
      Serial.println("Cambiado a MODO PRESION - Histórico reseteado");
      break;
      
    case MODE_WIFI_SCAN:
      // Iniciar modo WiFi
      wifi_page = 0; // Empezar en página 1
      // Mostrar inmediatamente pantalla de scanning
      mostrarPantallaScanningWiFi();
      // Iniciar primer escaneo inmediatamente
      escanearWiFi();
      Serial.println("Cambiado a MODO WiFi SCAN");
      break;
  }
}

// Función para manejar botón derecho (cambio de modo)
void manejarBotonDerecho() {
  // Botón derecho: Ciclar entre READ -> WRITE -> PRESSURE -> WiFi -> READ
  switch (current_mode) {
    case MODE_READ:
      cambiarModo(MODE_WRITE);
      break;
    case MODE_WRITE:
      cambiarModo(MODE_PRESSURE);
      break;
    case MODE_PRESSURE:
      cambiarModo(MODE_WIFI_SCAN);
      break;
    case MODE_WIFI_SCAN:
      cambiarModo(MODE_READ);
      break;
  }
}

// Función para manejar lógica del MODE_READ
void manejarModoRead() {
  unsigned long current_time = millis();
  
  // Modo lectura - actualizar gráfico con frecuencia leída
  if (current_time - last_pulse_time >= PULSE_CALC_INTERVAL_MS) {
    unsigned long pulses_in_interval = pulse_count - last_pulse_count;
    pulse_frequency = (pulses_in_interval * 1000.0) / (current_time - last_pulse_time);
    
    // Actualizar gráfico con frecuencia leída
    actualizarGrafico(pulse_frequency);
    
    last_pulse_count = pulse_count;
    last_pulse_time = current_time;
    
    // Información por serial cada segundo
    static unsigned long last_serial_time = 0;
    if (current_time - last_serial_time >= SERIAL_DEBUG_INTERVAL_MS) {
      Serial.print("READ - Pulsos: ");
      Serial.print(pulse_count);
      Serial.print(" | Freq: ");
      Serial.print(pulse_frequency, 2);
      Serial.println(" Hz");
      last_serial_time = current_time;
    }
  }
}

// Función para manejar lógica del MODE_WRITE
void manejarModoWrite() {
  unsigned long current_time = millis();
  
  // Modo escritura - generar pulsos
  generarPulsos();
  
  // Actualizar gráfico con frecuencia generada
  if (current_time - last_pulse_time >= PULSE_CALC_INTERVAL_MS) {
    actualizarGrafico(current_gen_frequency);
    last_pulse_time = current_time;
    
    // Información por serial cada segundo
    static unsigned long last_serial_time = 0;
    if (current_time - last_serial_time >= SERIAL_DEBUG_INTERVAL_MS) {
      Serial.print("WRITE - Gen: ");
      Serial.print(current_gen_frequency, 2);
      Serial.print(" Hz | Estado: ");
      Serial.println(generating_pulse ? "Generando" : "Pausa");
      last_serial_time = current_time;
    }
  }
}

void manejarModoPressure() {
  unsigned long current_time = millis();
  
  // Modo presión - leer sensor I2C a 100Hz
  if (current_time - last_pressure_read >= PRESSURE_READ_INTERVAL_MS) {
    WNK1MA_Reading reading = readWNK1MA();
    
    if (reading.isValid) {
      // Convertir valor raw a valor de presión (ajustar según calibración del sensor)
      float pressure_value = (float)reading.rawValue;
      
      // Actualizar gráfico con valor de presión
      actualizarGraficoPresion(pressure_value);
      
      // Actualizar actividad si hay cambios significativos
      // Si hay cambio significativo de presión, NO actualizar actividad - solo botones evitan sleep
      static float last_stable_pressure = 0.0;
      if (abs(pressure_value - last_stable_pressure) > 10.0) {
        // updateUserActivity(); // Comentado: solo botones evitan sleep
        last_stable_pressure = pressure_value;
      }
      
      // Información por serial cada segundo
      static unsigned long last_serial_time = 0;
      if (current_time - last_serial_time >= SERIAL_DEBUG_INTERVAL_MS) {
        Serial.print("PRESSURE - Raw: ");
        Serial.print(reading.rawValue);
        Serial.print(" | Value: ");
        Serial.print(pressure_value, 1);
        Serial.print(" | Range: ");
        Serial.print(pressure_min_scale, 1);
        Serial.print("-");
        Serial.println(pressure_max_scale, 1);
        last_serial_time = current_time;
      }
    } else {
      // Error leyendo sensor - mostrar en serial
      static unsigned long last_error_time = 0;
      if (current_time - last_error_time >= SERIAL_DEBUG_SLOW_MS) {
        Serial.println("ERROR: No se puede leer el sensor de presión I2C");
        last_error_time = current_time;
      }
    }
    
    last_pressure_read = current_time;
  }
}

void manejarModoWiFi() {
  unsigned long current_time = millis();
  
  // Modo WiFi Scan
  // Escanear automáticamente cada WIFI_SCAN_INTERVAL_MS
  if (!wifi_scanning && (current_time - last_wifi_scan >= WIFI_SCAN_INTERVAL_MS)) {
    escanearWiFi();
  }
  
  // Mostrar lista actualizada
  mostrarListaWiFi();
}

void loop() {
  unsigned long current_time = millis();
  static unsigned long last_button_time = 0;

  // Verificar timeout para sleep (solo si no estamos en modo sleep)
  // Sleep basado únicamente en actividad del usuario (botones), no en generación automática
  if (!in_sleep_mode && (current_time - last_user_activity_time >= SLEEP_TIMEOUT_MS)) {
    enterSleepMode();
    return; // No ejecutar el resto del loop
  }

  // Actualizar voltaje cada 500ms
  if (current_time - last_voltage_update >= VOLTAGE_UPDATE_MS) {
    voltaje = leerVoltaje();
    mostrarVoltaje();
    last_voltage_update = current_time;
  }

  // Control de botones (con debounce)
  if (current_time - last_button_time > BUTTON_DEBOUNCE_MS) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      manejarBotonIzquierdo();
      last_button_time = current_time;
    }
    
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      manejarBotonDerecho();
      last_button_time = current_time;
    }
  }

  // Ejecutar lógica según modo actual
  switch (current_mode) {
    case MODE_READ:
      manejarModoRead();
      break;
    case MODE_WRITE:
      manejarModoWrite();
      break;
    case MODE_PRESSURE:
      manejarModoPressure();
      break;
    case MODE_WIFI_SCAN:
      manejarModoWiFi();
      break;
  }

  // Mostrar información actualizada (solo si no estamos en modo WiFi)
  if (current_mode != MODE_WIFI_SCAN) {
    mostrarInfoSensor();
  }
}

// Función para entrar en modo sleep
void enterSleepMode() {
  Serial.println("Entrando en modo sleep por inactividad...");
  
  // Apagar la pantalla
  digitalWrite(4, LOW); // Apagar retroiluminación
  tft.writecommand(0x10); // Comando sleep para el display
  
  // Configurar wake-up por botones
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);   // BUTTON_LEFT (activo bajo)
  
  // Para GPIO35, usamos ext1 con bitmask
  uint64_t ext1_mask = 1ULL << GPIO_NUM_35;
  esp_sleep_enable_ext1_wakeup(ext1_mask, ESP_EXT1_WAKEUP_ALL_LOW); // GPIO35 (activo bajo)
  
  in_sleep_mode = true;
  
  // Entrar en deep sleep
  esp_deep_sleep_start();
}