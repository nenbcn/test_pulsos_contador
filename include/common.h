#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Enumeraciones
enum SystemMode {
  MODE_READ,
  MODE_WRITE,
  MODE_PRESSURE,
  MODE_RECIRCULATOR,
  MODE_WIFI_SCAN
};

enum TestCase {
  TEST_CASE_1,
  TEST_CASE_2,
  TEST_CASE_3,
  TEST_CASE_4,
  TEST_CASE_5
};

enum PhaseType {
  PHASE_TRANSITION,  // Cambio progresivo de tempo
  PHASE_STABLE       // Tempo constante
};

// Estructuras
struct PulsePhase {
  PhaseType type;
  float tempo_start_ms;   // Tiempo entre pulsos al inicio (ms)
  float tempo_end_ms;     // Tiempo entre pulsos al final (ms)
  int num_pulses;         // Número de pulsos a generar en esta fase
  float jitter_percent;   // % de variación aleatoria
};

struct PulsePattern {
  float periods[MAX_PULSES];
  int count;
  float frequencies[GRAPH_WIDTH];
  int freq_count;
};

struct WiFiNetwork {
  String ssid;
  int32_t rssi;
  wifi_auth_mode_t encryption;
  uint16_t color;
};

typedef struct {
    uint32_t rawValue;
    unsigned long timestamp;
    bool isValid;
} WNK1MA_Reading;

// Variables globales - Display y hardware
extern TFT_eSPI tft;
extern OneWire oneWireRecirculator;
extern DallasTemperature sensorTemp;
extern Adafruit_NeoPixel pixel;

// Variables globales - Sistema
extern SystemMode current_mode;
extern float voltaje;
extern unsigned long last_voltage_update;
extern unsigned long last_user_activity_time;
extern bool in_sleep_mode;

// Variables globales - Gráfico
extern float graph_data[GRAPH_WIDTH];
extern int graph_index;
extern float max_freq_scale;

// Melodías Mario Bros
extern int mario_melody[];
extern int mario_durations[];
extern int mario_num_notes;
extern int mario_gameover_melody[];
extern int mario_gameover_durations[];
extern int mario_gameover_num_notes;

// Arrays de test cases
extern const char* TEST_CASE_NAMES[];

// Funciones comunes
void updateUserActivity();
void enterSleepMode();
float leerVoltaje();
void IRAM_ATTR wakeUpInterrupt();

#endif
