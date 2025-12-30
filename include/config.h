#ifndef CONFIG_H
#define CONFIG_H

// Pines GPIO
#define BUTTON_LEFT 0
#define BUTTON_RIGHT 35
#define SENSOR_PIN 21

// Sensor de presión I2C
#define I2C_SDA 32
#define I2C_SCL 22
#define WNK1MA_ADDR 0x6D
#define WNK1MA_CMD 0x06

// Recirculador
#define TEMP_SENSOR_PIN 15
#define RELAY_PIN 12
#define BUZZER_PIN 17
#define NEOPIXEL_PIN 13

// Notas musicales para Buzzer
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

// Constantes de timing
#define BUTTON_DEBOUNCE_MS 300
#define PULSE_CALC_INTERVAL_MS 200
#define VOLTAGE_UPDATE_MS 500
#define WIFI_SCAN_INTERVAL_MS 10000
#define PRESSURE_READ_INTERVAL_MS 10
#define SLEEP_TIMEOUT_MS 300000
#define SERIAL_DEBUG_INTERVAL_MS 1000
#define SERIAL_DEBUG_SLOW_MS 5000
#define VOLTAGE_ADC_SAMPLES 32

// Constantes del gráfico
#define GRAPH_WIDTH 200
#define GRAPH_HEIGHT 75
#define GRAPH_X 20
#define GRAPH_Y 45

// Constantes del recirculador
#define RECIRCULATOR_MAX_TIME 120000  // 2 minutos

// Constantes WiFi
#define NETWORKS_PER_PAGE 5
#define MAX_WIFI_PAGES 3
#define MAX_WIFI_NETWORKS 20

// Constantes para generación de pulsos
#define MAX_PULSES 1000

// Períodos en milisegundos - Frecuencias correspondientes
#define PERIOD_F1  23
#define PERIOD_F2  39
#define PERIOD_F3  50
#define PERIOD_F4  89
#define PERIOD_F5  133
#define PERIOD_F6  250
#define PERIOD_F7  500
#define PERIOD_F8  10

#endif
