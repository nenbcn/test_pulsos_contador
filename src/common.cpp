#include "common.h"
#include "esp_sleep.h"

// Variables globales - Display y hardware
TFT_eSPI tft = TFT_eSPI();
OneWire oneWireRecirculator(TEMP_SENSOR_PIN);
DallasTemperature sensorTemp(&oneWireRecirculator);
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Variables globales - Sistema
SystemMode current_mode = MODE_READ;
float voltaje = 0.0;
unsigned long last_voltage_update = 0;
unsigned long last_user_activity_time = 0;
bool in_sleep_mode = false;

// Variables globales - Gráfico
float graph_data[GRAPH_WIDTH];
int graph_index = 0;
float max_freq_scale = 100.0;

// Melodías Mario Bros (éxito)
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

// Arrays de test cases
const char* TEST_CASE_NAMES[] = {
  "TC1: Rapid",
  "TC2: Normal", 
  "TC3: Compnd",
  "TC4: Strs",
  "TC5: Single"
};

void updateUserActivity() {
  last_user_activity_time = millis();
}

float leerVoltaje() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  long suma = 0;
  for(int i = 0; i < VOLTAGE_ADC_SAMPLES; i++) {
    suma += analogRead(36);
    delay(1);
  }
  
  float adc_reading = suma / (float)VOLTAGE_ADC_SAMPLES;
  float raw_voltage = (adc_reading * 3.3) / 4095.0;
  
  float calibrated_voltage;
  if (raw_voltage >= 2.5) {
    calibrated_voltage = ((raw_voltage - 2.5) / 0.8) * 3.3;
  } else {
    calibrated_voltage = 0.0;
  }
  
  if (calibrated_voltage < 0.0) calibrated_voltage = 0.0;
  if (calibrated_voltage > 3.6) calibrated_voltage = 3.6;
  
  return calibrated_voltage;
}

void IRAM_ATTR wakeUpInterrupt() {
  // Esta función se ejecuta cuando se presiona cualquier botón
}

void enterSleepMode() {
  Serial.println("Entrando en modo sleep por inactividad...");
  
  digitalWrite(4, LOW);
  tft.writecommand(0x10);
  
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  uint64_t ext1_mask = 1ULL << GPIO_NUM_35;
  esp_sleep_enable_ext1_wakeup(ext1_mask, ESP_EXT1_WAKEUP_ALL_LOW);
  
  in_sleep_mode = true;
  esp_deep_sleep_start();
}
