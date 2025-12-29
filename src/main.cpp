#include <Arduino.h>
#include <TFT_eSPI.h>
#include "esp_sleep.h"
#include <WiFi.h>
#include <Wire.h>
#include <float.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>

#define BUTTON_LEFT 0   // GPIO0 (izquierda)
#define BUTTON_RIGHT 35 // GPIO35 (derecha)
#define SENSOR_PIN 21   // GPIO21 (sensor de pulsos) - Cable VERDE

// --- Sensor de presión I2C Configuration ---
#define I2C_SDA 32      // GPIO32 para SDA - Cable AZUL ↔ Cable BLANCO sonda
#define I2C_SCL 22      // GPIO22 para SCL - Cable VERDE ↔ Cable AMARILLO sonda
#define WNK1MA_ADDR 0x6D
#define WNK1MA_CMD 0x06

// --- Recirculador Configuration ---
#define TEMP_SENSOR_PIN 15  // DS18B20 (cable AMARILLO)
#define RELAY_PIN 12        // Control relé (cable ROJO)
#define BUZZER_PIN 17       // PWM Buzzer (cable BLANCO)
#define NEOPIXEL_PIN 13     // WS2812B LED (cable NARANJA)

// --- Notas Musicales para Buzzer ---
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_E6  1319
#define NOTE_G6  1568
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_E7  2637
#define NOTE_G7  3136

// Melodía Mario Bros (éxito)
int mario_melody[] = {
  NOTE_E7, NOTE_E7, 0, NOTE_E7, 0, NOTE_C7, NOTE_E7, 0, NOTE_G7, 0, 0, 0,
  NOTE_G6, 0, 0, 0, NOTE_C7, 0, 0, NOTE_G6, 0, 0, NOTE_E6, 0, NOTE_A6, 0, NOTE_B6, 0, NOTE_AS6, NOTE_A6, 0
};
int mario_durations[] = {
  125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
  125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125
};
int mario_num_notes = sizeof(mario_melody) / sizeof(mario_melody[0]);

// Melodía Mario Bros Game Over (fracaso/timeout)
int mario_gameover_melody[] = {
  NOTE_C5, NOTE_G5, NOTE_E5,
  NOTE_A5, NOTE_B5, NOTE_A5, NOTE_GS5, NOTE_AS5, NOTE_GS5,
  NOTE_G5, NOTE_D5, NOTE_E5
};
int mario_gameover_durations[] = {
  250, 250, 250,
  250, 250, 250, 250, 250, 250,
  250, 250, 500
};
int mario_gameover_num_notes = sizeof(mario_gameover_melody) / sizeof(mario_gameover_melody[0]);

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
#define GRAPH_HEIGHT 75
#define GRAPH_X 20
#define GRAPH_Y 45

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
  MODE_RECIRCULATOR,
  MODE_WIFI_SCAN
};
SystemMode current_mode = MODE_READ;

// Variables para el generador de pulsos
bool generating_pulse = false;
unsigned long next_pulse_time = 0;
float pulse_interval = 0;  // Cambiar a float para mantener precisión decimal
float target_frequency = 50.0; // Frecuencia fija de 50 Hz
float current_gen_frequency = 0.0;
bool pulse_state = false;

// === FRECUENCIAS BASE SEGÚN DOCUMENTACIÓN ===
// Períodos en milisegundos - Frecuencias correspondientes
#define PERIOD_F1  23    // 43.5 Hz - Flujo muy alto
#define PERIOD_F2  39    // 25.6 Hz - Flujo alto
#define PERIOD_F3  50    // 20.0 Hz - Flujo medio-alto
#define PERIOD_F4  89    // 11.2 Hz - Flujo medio
#define PERIOD_F5  133   // 7.5 Hz  - Flujo bajo
#define PERIOD_F6  250   // 4.0 Hz  - Flujo muy bajo
#define PERIOD_F7  500   // 2.0 Hz  - Flujo mínimo
#define PERIOD_F8  10    // 100 Hz  - Límite debounce

// === TEST CASES SEGÚN DOCUMENTACIÓN ===
enum TestCase {
  TEST_CASE_6,   // Secuencia Completa (1→2→3→4→5)
  TEST_CASE_1,   // Arranque/Parada Rápidos
  TEST_CASE_2,   // Normal - Arranque-Estable-Parada
  TEST_CASE_3,   // Evento Compuesto
  TEST_CASE_4,   // Stress Test
  TEST_CASE_5    // Single Pulse
};

TestCase current_test = TEST_CASE_6;
unsigned long test_start_time = 0;

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

// Variables para el recirculador
bool recirculator_power_state = false;
unsigned long recirculator_start_time = 0;
float recirculator_temp = 0.0;
float recirculator_max_temp = 30.0;
const unsigned long RECIRCULATOR_MAX_TIME = 120000; // 2 minutos en ms

// Objetos para el recirculador
OneWire oneWireRecirculator(TEMP_SENSOR_PIN);
DallasTemperature sensorTemp(&oneWireRecirculator);
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// === ESTRUCTURA PARA SIMULACIÓN REALISTA DE PULSOS ===
// En lugar de frecuencias fijas, modela transiciones progresivas y períodos estables

enum PhaseType {
  PHASE_TRANSITION_START,  // Arranque progresivo (período disminuye)
  PHASE_STABLE,            // Flujo estable con jitter natural
  PHASE_TRANSITION_STOP,   // Parada progresiva (período aumenta)
  PHASE_PAUSE              // Sin pulsos
};

struct PulsePhase {
  PhaseType type;
  unsigned long duration_ms;  // Duración de esta fase
  float period_start_ms;      // Período inicial (para transiciones)
  float period_end_ms;        // Período final (para transiciones/estables)
  float jitter_percent;       // Variación natural del período (ej: 2.0 = ±2%)
};

// Declaraciones forward
void enterSleepMode();
float getTestCaseFrequency(TestCase test, unsigned long elapsed_ms);
float getTestCasePeriod(TestCase test, unsigned long elapsed_ms);
void inicializarRecirculador();
void setRecirculatorPower(bool state);
void leerTemperaturaRecirculador();
void controlarRecirculadorAutomatico();
void mostrarPantallaRecirculador();
void manejarModoRecirculador();

// Función helper para tocar tonos con máxima potencia
void playTone(int frequency, int duration_ms) {
  if (frequency > 0) {
    ledcWriteTone(0, frequency);
    ledcWrite(0, 512); // 50% duty cycle con 10 bits (1023/2) - máxima potencia audible
  } else {
    ledcWriteTone(0, 0);
  }
  delay(duration_ms);
}

void stopTone() {
  ledcWriteTone(0, 0);
}

// === FUNCIONES PARA GENERAR PERIODOS REALISTAS ===

// Calcula el período para una transición progresiva (aceleración/desaceleración)
// Usa interpolación exponencial para simular comportamiento físico real
float calcularPeriodoTransicion(float period_start, float period_end, float progress) {
  // progress: 0.0 a 1.0 (inicio a fin de la transición)
  // Usar easing exponencial para suavidad realista (easeOutExpo)
  float eased_progress = (progress == 1.0) ? 1.0 : 1.0 - pow(2.0, -10.0 * progress);
  return period_start + (period_end - period_start) * eased_progress;
}

// Agrega jitter natural al período para simular variación física real
float aplicarJitter(float period, float jitter_percent, unsigned long seed) {
  if (jitter_percent <= 0.0) return period;
  
  // Usar seed para pseudo-aleatorio determinista (basado en tiempo)
  unsigned long random_val = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
  float random_factor = (random_val % 2000) / 1000.0 - 1.0; // -1.0 a +1.0
  
  float variation = period * (jitter_percent / 100.0) * random_factor;
  return period + variation;
}

// Calcula el período actual basado en una fase de pulsos
float calcularPeriodoDesdeFase(const PulsePhase& phase, unsigned long phase_elapsed_ms) {
  switch (phase.type) {
    case PHASE_TRANSITION_START:
    case PHASE_TRANSITION_STOP: {
      // Transición progresiva
      float progress = (float)phase_elapsed_ms / (float)phase.duration_ms;
      progress = constrain(progress, 0.0, 1.0);
      
      float base_period = calcularPeriodoTransicion(phase.period_start_ms, phase.period_end_ms, progress);
      return aplicarJitter(base_period, phase.jitter_percent, phase_elapsed_ms);
    }
    
    case PHASE_STABLE: {
      // Período estable con jitter
      return aplicarJitter(phase.period_end_ms, phase.jitter_percent, phase_elapsed_ms);
    }
    
    case PHASE_PAUSE:
    default:
      return 0.0; // Sin pulsos
  }
}

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
  
  // Inicializar con TEST_CASE_6 por defecto (Secuencia Completa)
  current_test = TEST_CASE_6;
  test_start_time = millis();
  
  Serial.println("Generador inicializado - Sistema de Test Cases:");
  Serial.println("Test Case activo: TEST_CASE_6 (Secuencia Completa)");
  Serial.println("\n=== CASOS CON TRANSICIONES PROGRESIVAS (basados en logs reales) ===");
  Serial.println("  6: Secuencia Completa - Todos los casos (~40s)");
  Serial.println("     Ejecuta tests 1→2→3→4→5 consecutivamente con pausas");
  Serial.println();
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
  Serial.println();
  Serial.println("  6: Secuencia Completa - Todos los casos (~40s)");
  Serial.println("     Ejecuta tests 1→2→3→4→5 consecutivamente con pausas");
  Serial.println("\nUsar botones para cambiar test case en modo WRITE");
}

// === DEFINICIÓN DE TEST CASES REALISTAS CON TRANSICIONES PROGRESIVAS ===

// TEST CASE 1: Arranque/Parada Rápidos SIN Estabilización
// Arranque rápido que NO da tiempo a estabilizar antes de parar
PulsePhase test1_phases[] = {
  // Arranque muy rápido (0.25s, 75ms→43ms)
  {PHASE_TRANSITION_START, 250, 75.0, 43.0, 2.0},
  
  // Parada inmediata sin fase estable (0.3s, 43ms→90ms)
  {PHASE_TRANSITION_STOP, 300, 43.0, 90.0, 2.5},
  
  {PHASE_PAUSE, 1000, 0, 0, 0}
};

// TEST CASE 2: Normal (Arranque-Estable-Parada)
// Patrón completo basado en logs reales
PulsePhase test2_phases[] = {
  // Arranque realista (0.7s, 81ms→48ms) - datos reales del log
  {PHASE_TRANSITION_START, 700, 81.0, 48.0, 2.5},
  
  // Flujo estable (3s, ~49ms ≈ 20.4Hz con jitter real ±5%)
  {PHASE_STABLE, 3000, 49.0, 49.0, 4.5},
  
  // Parada realista (1.1s, 49ms→97ms) - datos reales del log
  {PHASE_TRANSITION_STOP, 1100, 49.0, 97.0, 3.0},
  
  {PHASE_PAUSE, 1000, 0, 0, 0}
};

// TEST CASE 3: Evento Compuesto (Apertura de grifo adicional)
// Simula: grifo abierto → se abre segundo grifo → cierra uno → cierra todo
PulsePhase test3_phases[] = {
  // Arranque inicial (0.6s, 73ms→53ms)
  {PHASE_TRANSITION_START, 600, 73.0, 53.0, 2.5},
  
  // Flujo bajo estable (1.5s, ~53ms ≈ 19Hz)
  {PHASE_STABLE, 1500, 53.0, 53.0, 4.0},
  
  // Sube caudal: nuevo grifo (0.4s, 53ms→28ms) - SUBIDA DRAMÁTICA
  {PHASE_TRANSITION_START, 400, 53.0, 28.0, 2.5},
  
  // Flujo MUY alto estable (2s, ~28ms ≈ 36.4Hz)
  {PHASE_STABLE, 2000, 28.0, 28.0, 5.0},
  
  // Baja: cierra un grifo (0.5s, 28ms→55ms)
  {PHASE_TRANSITION_STOP, 500, 28.0, 55.0, 3.0},
  
  // Vuelve a flujo bajo (1.5s, ~55ms ≈ 18.2Hz)
  {PHASE_STABLE, 1500, 55.0, 55.0, 4.0},
  
  // Parada final (0.9s, 55ms→100ms)
  {PHASE_TRANSITION_STOP, 900, 55.0, 100.0, 3.0},
  
  {PHASE_PAUSE, 1000, 0, 0, 0}
};

// TEST CASE 4: Flujo Alto Prolongado (Stress Test - muchos pulsos)
// Genera gran cantidad de pulsos para probar límites del sistema
PulsePhase test4_phases[] = {
  // Arranque rápido a flujo MUY alto (0.35s, 65ms→23ms)
  {PHASE_TRANSITION_START, 350, 65.0, 23.0, 2.0},
  
  // Flujo MUY ALTO prolongado (12.5s, ~23ms ≈ 44.4Hz) - genera ~555 pulsos
  {PHASE_STABLE, 12500, 23.0, 23.0, 5.0},
  
  // Parada (0.6s, 23ms→80ms)
  {PHASE_TRANSITION_STOP, 600, 23.0, 80.0, 2.5},
  
  {PHASE_PAUSE, 1500, 0, 0, 0}
};

// TEST CASE 5: Single Pulse (Detección de fugas/pulsos aislados)
// UN SOLO pulso seguido de pausa de 1 segundo (timeout)
PulsePhase test5_phases[] = {
  // Pulso 1: UN solo pulso (~53ms para completar el ciclo)
  {PHASE_STABLE, 53, 50.0, 50.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},  // Pausa 1s (timeout)
  
  // Pulso 2: UN solo pulso
  {PHASE_STABLE, 55, 53.0, 53.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},  // Pausa 1s
  
  // Pulso 3: UN solo pulso
  {PHASE_STABLE, 50, 48.0, 48.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},  // Pausa 1s
  
  // Pulso 4: UN solo pulso
  {PHASE_STABLE, 54, 52.0, 52.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},  // Pausa 1s
  
  // Pulso 5: UN solo pulso final
  {PHASE_STABLE, 51, 49.0, 49.0, 0},
  {PHASE_PAUSE, 1500, 0, 0, 0}   // Pausa final
};

// TEST CASE 6: Secuencia Completa (todos los casos anteriores en orden)
// Ejecuta: Test1 → pausa → Test2 → pausa → Test3 → pausa → Test4 → pausa → Test5
PulsePhase test6_phases[] = {
  // === TEST 1: Arranque/Parada Rápidos ===
  {PHASE_TRANSITION_START, 250, 75.0, 43.0, 2.0},
  {PHASE_TRANSITION_STOP, 300, 43.0, 90.0, 2.5},
  {PHASE_PAUSE, 1500, 0, 0, 0},  // Pausa entre tests
  
  // === TEST 2: Normal ===
  {PHASE_TRANSITION_START, 700, 81.0, 48.0, 2.5},
  {PHASE_STABLE, 3000, 49.0, 49.0, 4.5},
  {PHASE_TRANSITION_STOP, 1100, 49.0, 97.0, 3.0},
  {PHASE_PAUSE, 1500, 0, 0, 0},  // Pausa entre tests
  
  // === TEST 3: Evento Compuesto ===
  {PHASE_TRANSITION_START, 600, 73.0, 53.0, 2.5},
  {PHASE_STABLE, 1500, 53.0, 53.0, 4.0},
  {PHASE_TRANSITION_START, 400, 53.0, 28.0, 2.5},
  {PHASE_STABLE, 2000, 28.0, 28.0, 5.0},
  {PHASE_TRANSITION_STOP, 500, 28.0, 55.0, 3.0},
  {PHASE_STABLE, 1500, 55.0, 55.0, 4.0},
  {PHASE_TRANSITION_STOP, 900, 55.0, 100.0, 3.0},
  {PHASE_PAUSE, 1500, 0, 0, 0},  // Pausa entre tests
  
  // === TEST 4: Stress Test ===
  {PHASE_TRANSITION_START, 350, 65.0, 23.0, 2.0},
  {PHASE_STABLE, 12500, 23.0, 23.0, 5.0},
  {PHASE_TRANSITION_STOP, 600, 23.0, 80.0, 2.5},
  {PHASE_PAUSE, 1500, 0, 0, 0},  // Pausa entre tests
  
  // === TEST 5: Single Pulse ===
  {PHASE_STABLE, 53, 50.0, 50.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},
  {PHASE_STABLE, 55, 53.0, 53.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},
  {PHASE_STABLE, 50, 48.0, 48.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},
  {PHASE_STABLE, 54, 52.0, 52.0, 0},
  {PHASE_PAUSE, 1000, 0, 0, 0},
  {PHASE_STABLE, 51, 49.0, 49.0, 0},
  {PHASE_PAUSE, 2500, 0, 0, 0}   // Pausa final
};

// Función para obtener el período (en ms) según el test case y tiempo transcurrido
float getTestCasePeriod(TestCase test, unsigned long elapsed_ms) {
  PulsePhase* phases = nullptr;
  int num_phases = 0;
  
  // Seleccionar las fases según el test case
  switch(test) {
    case TEST_CASE_1:
      phases = test1_phases;
      num_phases = sizeof(test1_phases) / sizeof(test1_phases[0]);
      break;
    case TEST_CASE_2:
      phases = test2_phases;
      num_phases = sizeof(test2_phases) / sizeof(test2_phases[0]);
      break;
    case TEST_CASE_3:
      phases = test3_phases;
      num_phases = sizeof(test3_phases) / sizeof(test3_phases[0]);
      break;
    case TEST_CASE_4:
      phases = test4_phases;
      num_phases = sizeof(test4_phases) / sizeof(test4_phases[0]);
      break;
    case TEST_CASE_5:
      phases = test5_phases;
      num_phases = sizeof(test5_phases) / sizeof(test5_phases[0]);
      break;
    case TEST_CASE_6:
      phases = test6_phases;
      num_phases = sizeof(test6_phases) / sizeof(test6_phases[0]);
      break;
    default:
      // Casos legacy eliminados
      return 0.0;
  }
  
  // Encontrar la fase actual
  unsigned long accumulated_time = 0;
  for (int i = 0; i < num_phases; i++) {
    unsigned long phase_end = accumulated_time + phases[i].duration_ms;
    
    if (elapsed_ms < phase_end) {
      // Estamos en esta fase
      unsigned long phase_elapsed = elapsed_ms - accumulated_time;
      return calcularPeriodoDesdeFase(phases[i], phase_elapsed);
    }
    
    accumulated_time = phase_end;
  }
  
  // Si pasamos todas las fases, sin pulsos
  return 0.0;
}

// === FUNCIÓN LEGACY ELIMINADA - Todos los casos usan sistema de fases ===
float getTestCaseFrequency(TestCase test, unsigned long elapsed_ms) {
  // Todos los test cases ahora usan getTestCasePeriod()
  return 0.0;
}

// Función para generar pulsos según test cases documentados
void generarPulsos() {
  unsigned long current_time = millis();
  unsigned long test_elapsed = current_time - test_start_time;
  
  // NUEVO: Intentar obtener período directamente (para casos con transiciones progresivas)
  float target_period = getTestCasePeriod(current_test, test_elapsed);
  float target_freq = 0.0;
  
  // Si no hay período del nuevo sistema, usar el sistema legacy de frecuencias
  if (target_period == 0.0) {
    target_freq = getTestCaseFrequency(current_test, test_elapsed);
    if (target_freq > 0) {
      target_period = 1000.0 / target_freq;
    }
  } else {
    // Calcular frecuencia desde el período para mostrarla
    target_freq = 1000.0 / target_period;
  }
  
  // Generar pulsos si hay período válido
  if (target_period > 0) {
    generating_pulse = true;
    current_gen_frequency = target_freq;
    pulse_interval = target_period;
    
    // Inicializar timing solo la primera vez
    if (next_pulse_time == 0) {
      next_pulse_time = current_time + (pulse_interval / 2);
      pulse_state = false;
    }
    
    // LOG DETALLADO: Verificar coherencia (solo cambios significativos)
    static unsigned long last_freq_log = 0;
    static float last_logged_freq = -1;
    static float last_logged_period = -1;
    if (current_time - last_freq_log >= SERIAL_DEBUG_INTERVAL_MS) {
      if (abs(target_freq - last_logged_freq) > 0.1 || abs(target_period - last_logged_period) > 1.0) {
        Serial.print("PULSE_GEN - Test: ");
        Serial.print(current_test);
        Serial.print(", Elapsed: ");
        Serial.print(test_elapsed);
        Serial.print("ms, Period: ");
        Serial.print(target_period, 2);
        Serial.print("ms, Freq: ");
        Serial.print(target_freq, 2);
        Serial.println("Hz");
        last_logged_freq = target_freq;
        last_logged_period = target_period;
      }
      last_freq_log = current_time;
    }
    
    // NO actualizar actividad durante generación automática
    // El sleep solo se controla por actividad del usuario (botones)
    
    if (current_time >= next_pulse_time) {
      // Calcular error de timing antes de actualizar
      long timing_error = (long)current_time - (long)next_pulse_time;
      
      pulse_state = !pulse_state;
      digitalWrite(SENSOR_PIN, pulse_state ? HIGH : LOW);
      
      unsigned long actual_pulse_time = next_pulse_time; // Guardar para log
      next_pulse_time += (pulse_interval / 2); // 50% duty cycle - sumar al tiempo anterior para evitar drift
      
      // CONTADOR DE PULSOS: Log detallado de timestamps (primeros 20 pulsos)
      static unsigned long pulse_count_generated = 0;
      
      if (pulse_state) { // Solo contar flancos positivos (pulsos completos)
        pulse_count_generated++;
        
        // LOG DETALLADO: Timestamp de cada pulso (primeros 20 pulsos)
        if (pulse_count_generated <= 20) {
          Serial.print("PULSE #");
          Serial.print(pulse_count_generated);
          Serial.print(" - ts=");
          Serial.print(current_time);
          Serial.print(", period=");
          Serial.print(pulse_interval, 2);
          Serial.print("ms, error=");
          Serial.print(timing_error);
          Serial.println("ms");
        }
      }
    }
  } else {
    // Fases de pausa
    generating_pulse = false;
    current_gen_frequency = 0.0;
    digitalWrite(SENSOR_PIN, LOW);
    pulse_state = false;
    next_pulse_time = 0; // Reiniciar para la próxima fase
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
    } else if (current_mode == MODE_RECIRCULATOR) {
      tft.setTextColor(TFT_ORANGE);
      tft.drawString("RECIR", 175, 25);
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
    
    // Mostrar test case actual y tiempo transcurrido
    unsigned long test_elapsed = millis() - test_start_time;
    float elapsed_sec = test_elapsed / 1000.0;
    
    const char* test_names[] = {"TC6:Full", "TC1:Rapid", "TC2:Normal", "TC3:Compound", "TC4:Stress", "TC5:Single"};
    char status_text[40];
    snprintf(status_text, sizeof(status_text), "%s: %.1fs", test_names[current_test], elapsed_sec);
    
    if (strcmp(status_text, last_total_text) != 0) {
      tft.fillRect(5, 25, 160, 15, TFT_BLACK);
      uint16_t color;
      if (current_test == TEST_CASE_1 || current_test == TEST_CASE_2) {
        color = TFT_CYAN; // Tests básicos
      } else if (current_test == TEST_CASE_6) {
        color = TFT_MAGENTA; // Test secuencial completo
      } else {
        color = TFT_GREEN; // Otros tests
      }
      tft.setTextColor(color);
      tft.setTextSize(1);
      tft.setTextFont(2);
      tft.drawString(status_text, 5, 25);
      strcpy(last_total_text, status_text);
    }
    
    // Mostrar descripción del test case actualizado
    const char* pattern_text = "Test Cases: F1-F8 (2-100Hz)";
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
    
    // Mostrar información del modo con histórico
    const char* mode_text = "Sensor I2C @ 100Hz (Historico)";
    if (strcmp(mode_text, last_phase_text) != 0) {
      tft.fillRect(5, 25, 230, 10, TFT_BLACK);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.setTextFont(1);
      tft.drawString(mode_text, 5, 25);
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

// ============================================================================
// FUNCIONES DEL RECIRCULADOR
// ============================================================================

void inicializarRecirculador() {
  Serial.println("\n=== INICIALIZANDO RECIRCULADOR ===");
  
  // Configurar pines
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("✓ Relé (GPIO12) configurado");
  
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Configurar LEDC para el buzzer (necesario en ESP32)
  // 10 bits = máximo duty cycle 1023 (más potencia que 8 bits)
  ledcSetup(0, 5000, 10); // Canal 0, 5kHz, resolución 10 bits
  ledcAttachPin(BUZZER_PIN, 0);
  Serial.println("✓ Buzzer (GPIO17) configurado con LEDC (10 bits - máxima potencia)");
  
  // Test del buzzer - 3 beeps cortos a máxima potencia
  Serial.println("Probando buzzer...");
  for(int i = 0; i < 3; i++) {
    playTone(1000, 100); // 1kHz
    stopTone();
    delay(100);
  }
  Serial.println("✓ Test buzzer completado");
  
  // Inicializar sensor DS18B20
  Serial.println("Inicializando DS18B20 en GPIO15...");
  sensorTemp.begin();
  int deviceCount = sensorTemp.getDeviceCount();
  Serial.printf("DS18B20 devices found: %d\n", deviceCount);
  
  if (deviceCount == 0) {
    Serial.println("⚠️ NO SE DETECTÓ SENSOR DS18B20!");
    Serial.println("   Verifica:");
    Serial.println("   - Cable AMARILLO conectado a GPIO15");
    Serial.println("   - Sensor alimentado (VCC y GND)");
    Serial.println("   - Resistencia pull-up 4.7kΩ entre DATA y VCC");
  } else {
    // Hacer una lectura de prueba
    Serial.println("Haciendo lectura de prueba...");
    sensorTemp.requestTemperatures();
    delay(100);
    float testTemp = sensorTemp.getTempCByIndex(0);
    Serial.printf("Temperatura inicial: %.2f°C\n", testTemp);
    if (testTemp == -127.0) {
      Serial.println("⚠️ Sensor responde pero no lee temperatura correctamente");
    } else if (testTemp == 85.0) {
      Serial.println("⚠️ Sensor devuelve temperatura por defecto (85°C)");
    }
  }
  
  // Inicializar NeoPixel
  pixel.begin();
  pixel.setBrightness(50);
  pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Rojo = apagado
  pixel.show();
  Serial.println("✓ NeoPixel (GPIO13) inicializado");
  
  recirculator_power_state = false;
  recirculator_temp = 0.0;
  Serial.println("=== RECIRCULADOR LISTO ===\n");
}

void setRecirculatorPower(bool state) {
  recirculator_power_state = state;
  
  if (state) {
    // ENCENDER bomba
    // Beep ANTES de encender (para oírlo)
    playTone(1000, 150);
    stopTone();
    delay(50);
    
    digitalWrite(RELAY_PIN, HIGH);
    recirculator_start_time = millis();
    pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Verde
    pixel.show();
    Serial.println("✅ Bomba ENCENDIDA");
  } else {
    // APAGAR bomba
    digitalWrite(RELAY_PIN, LOW);
    pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Rojo
    pixel.show();
    
    // Beep DESPUÉS de apagar (para oírlo)
    delay(100);
    playTone(500, 200);
    stopTone();
    Serial.println("🛑 Bomba APAGADA");
  }
}

void leerTemperaturaRecirculador() {
  Serial.println("📡 Solicitando temperatura...");
  sensorTemp.requestTemperatures();
  delay(10); // Pequeña espera para que el sensor responda
  
  float temp = sensorTemp.getTempCByIndex(0);
  Serial.printf("📊 Lectura raw del sensor: %.2f°C\n", temp);
  
  // Filtrar errores comunes del DS18B20
  if (temp != -127.0 && temp != 85.0 && temp > -50.0 && temp < 125.0) {
    recirculator_temp = temp;
    Serial.printf("✅ Temperatura válida actualizada: %.2f°C\n", recirculator_temp);
  } else {
    // MODO SIMULACIÓN: Usar temperatura simulada si sensor no responde
    static bool simulation_warned = false;
    if (!simulation_warned) {
      Serial.println("⚠️ SENSOR NO RESPONDE - ACTIVANDO MODO SIMULACIÓN");
      Serial.println("   Temperatura simulada: 25°C + variación aleatoria");
      simulation_warned = true;
    }
    
    // Simular temperatura ambiente con pequeña variación (25°C ±2°C)
    static float simulated_temp = 25.0;
    simulated_temp += (random(-10, 10) / 10.0); // Variación de ±1°C
    simulated_temp = constrain(simulated_temp, 23.0, 27.0);
    
    recirculator_temp = simulated_temp;
    Serial.printf("🎭 Temperatura SIMULADA: %.2f°C\n", recirculator_temp);
  }
}

void controlarRecirculadorAutomatico() {
  if (!recirculator_power_state) return; // Solo si está encendido
  
  unsigned long elapsed = millis() - recirculator_start_time;
  
  // CONDICIÓN 1: Temperatura alcanzada
  if (recirculator_temp >= recirculator_max_temp) {
    setRecirculatorPower(false);
    // Melody Mario Bros (éxito) - Tocar 2 veces
    delay(200); // Esperar a que se apague la bomba
    for (int repeat = 0; repeat < 2; repeat++) {
      for (int i = 0; i < mario_num_notes; i++) {
        if (mario_melody[i] == 0) {
          stopTone();
          delay(mario_durations[i] * 1.25);
        } else {
          playTone(mario_melody[i], mario_durations[i] * 1.25);
        }
      }
      stopTone();
      delay(100); // Pequeña pausa entre repeticiones
    }
    stopTone();
    Serial.println("🎯 Temperatura alcanzada - Apagado automático");
    return;
  }
  
  // CONDICIÓN 2: Timeout (2 minutos)
  if (elapsed >= RECIRCULATOR_MAX_TIME) {
    setRecirculatorPower(false);
    // Melody Mario Bros Game Over (fracaso)
    delay(200); // Esperar a que se apague la bomba
    for (int i = 0; i < mario_gameover_num_notes; i++) {
      playTone(mario_gameover_melody[i], mario_gameover_durations[i]);
    }
    stopTone();
    Serial.println("⏱️ Timeout 2 minutos - Apagado automático");
    return;
  }
}

void mostrarPantallaRecirculador() {
  static bool last_power_state = false;
  static float last_temp = -999;
  static unsigned long last_elapsed = 0;
  
  unsigned long current_time = millis();
  unsigned long elapsed = recirculator_power_state ? 
                          (current_time - recirculator_start_time) / 1000 : 0;
  
  // Solo actualizar si hay cambios significativos
  bool needs_update = (last_power_state != recirculator_power_state) ||
                      (abs(last_temp - recirculator_temp) > 0.5) ||
                      (abs((long)last_elapsed - (long)elapsed) >= 1);
  
  if (!needs_update) return;
  
  // Limpiar área de contenido
  tft.fillRect(0, 22, 240, 113, TFT_BLACK);
  
  // Título
  tft.setTextColor(TFT_ORANGE);
  tft.setTextFont(2);
  tft.drawString("RECIRCULATOR", 10, 25);
  
  // Estado
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(2);
  tft.drawString("Estado:", 10, 45);
  
  if (recirculator_power_state) {
    tft.setTextColor(TFT_GREEN);
    tft.drawString("ENCENDIDO", 80, 45);
  } else {
    tft.setTextColor(TFT_RED);
    tft.drawString("APAGADO", 80, 45);
  }
  
  // Temperatura actual
  tft.setTextColor(TFT_CYAN);
  tft.setTextFont(2);
  char temp_str[30];
  snprintf(temp_str, sizeof(temp_str), "Temp: %.1fC", recirculator_temp);
  tft.drawString(temp_str, 10, 70);
  
  // Temperatura máxima
  tft.setTextColor(TFT_YELLOW);
  char max_temp_str[30];
  snprintf(max_temp_str, sizeof(max_temp_str), "Max:  %.1fC", recirculator_max_temp);
  tft.drawString(max_temp_str, 10, 90);
  
  // Tiempo transcurrido (solo si está encendido)
  if (recirculator_power_state) {
    tft.setTextColor(TFT_MAGENTA);
    char time_str[30];
    unsigned long total_seconds = RECIRCULATOR_MAX_TIME / 1000;
    snprintf(time_str, sizeof(time_str), "Tiempo: %02lu:%02lu / %02lu:%02lu",
             elapsed / 60, elapsed % 60,
             total_seconds / 60, total_seconds % 60);
    tft.drawString(time_str, 10, 110);
  }
  
  // Instrucciones
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextFont(1);
  tft.drawString("[IZQ] ON/OFF", 10, 115);
  tft.drawString("[DER] Cambiar modo", 10, 125);
  
  last_power_state = recirculator_power_state;
  last_temp = recirculator_temp;
  last_elapsed = elapsed;
}

void manejarModoRecirculador() {
  static unsigned long last_temp_read = 0;
  static unsigned long last_debug_print = 0;
  unsigned long current_time = millis();
  
  // Leer temperatura cada 1 segundo
  if (current_time - last_temp_read >= 1000) {
    leerTemperaturaRecirculador();
    last_temp_read = current_time;
  }
  
  // Debug cada 3 segundos
  if (current_time - last_debug_print >= 3000) {
    Serial.printf("🌡️ Temp actual: %.2f°C | Max: %.1f°C | Bomba: %s\n", 
                  recirculator_temp, recirculator_max_temp, 
                  recirculator_power_state ? "ON" : "OFF");
    last_debug_print = current_time;
  }
  
  // Control automático
  controlarRecirculadorAutomatico();
  
  // Actualizar pantalla
  mostrarPantallaRecirculador();
}

// ============================================================================
// FIN FUNCIONES DEL RECIRCULADOR
// ============================================================================

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
  inicializarRecirculador();
  
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
  Serial.println("GPIO15/12/17/13 - Recirculador (Temp/Relé/Buzzer/LED)");
  Serial.println("Botón IZQUIERDO: Toggle bomba / Cambiar página WiFi");
  Serial.println("Botón DERECHO: Ciclar READ->WRITE->PRESSURE->RECIR->WiFi->READ");
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
  
  if (current_mode == MODE_WRITE) {
    // En modo WRITE: cambiar test case
    int next_test = (int)current_test + 1;
    if (next_test > TEST_CASE_5) next_test = 0;
    current_test = (TestCase)next_test;
    test_start_time = millis();  // Reiniciar timer del test
    
    String test_names[] = {"TEST_6 (Sequential)", "TEST_1 (Rapid)", "TEST_2 (Normal)", "TEST_3 (Compound)", 
                          "TEST_4 (Stress)", "TEST_5 (Single)"};
    Serial.println("Cambiando a: " + test_names[current_test]);
    
    // Limpiar pantalla para mostrar nuevo test case
    tft.fillRect(0, 0, 240, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(1);
    tft.setTextFont(2);
    tft.drawString(test_names[current_test], 5, 2);
    
  } else if (current_mode == MODE_RECIRCULATOR) {
    // En modo Recirculador: Toggle bomba ON/OFF
    setRecirculatorPower(!recirculator_power_state);
  } else if (current_mode == MODE_WIFI_SCAN) {
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
      
    case MODE_RECIRCULATOR:
      // Apagar bomba al entrar al modo si estaba encendida
      if (recirculator_power_state) {
        setRecirculatorPower(false);
      }
      tft.fillScreen(TFT_BLACK);
      mostrarModo();
      mostrarVoltaje();
      mostrarPantallaRecirculador();
      Serial.println("Cambiado a MODO RECIRCULADOR");
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
  // Botón derecho: Ciclar entre READ -> WRITE -> PRESSURE -> RECIRCULATOR -> WiFi -> READ
  switch (current_mode) {
    case MODE_READ:
      cambiarModo(MODE_WRITE);
      break;
    case MODE_WRITE:
      cambiarModo(MODE_PRESSURE);
      break;
    case MODE_PRESSURE:
      cambiarModo(MODE_RECIRCULATOR);
      break;
    case MODE_RECIRCULATOR:
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
    case MODE_RECIRCULATOR:
      manejarModoRecirculador();
      break;
    case MODE_WIFI_SCAN:
      manejarModoWiFi();
      break;
  }

  // Mostrar información actualizada (solo si no estamos en modo WiFi o Recirculador)
  if (current_mode != MODE_WIFI_SCAN && current_mode != MODE_RECIRCULATOR) {
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