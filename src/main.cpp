#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "common.h"
#include "display.h"
#include "mode_read.h"
#include "mode_write.h"
#include "mode_pressure.h"
#include "mode_recirculator.h"
#include "mode_wifi.h"

// Declaraciones forward para funciones del modo
void cambiarModo(SystemMode nuevo_modo);
void manejarBotonIzquierdo();
void manejarBotonDerecho();
void mostrarInfoSensor();

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
  digitalWrite(4, HIGH);

  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  
  // Configurar el pin del sensor/generador
  pinMode(SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseInterrupt, RISING);

  // Inicializar I2C para el sensor de presión
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  Serial.println("I2C inicializado para sensor de presión WNK1MA");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Inicializar módulos
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

void cambiarModo(SystemMode nuevo_modo) {
  if (nuevo_modo == current_mode) return;
  
  SystemMode modo_anterior = current_mode;
  current_mode = nuevo_modo;
  
  updateUserActivity();
  
  switch (nuevo_modo) {
    case MODE_READ:
      inicializarModoRead();
      tft.fillScreen(TFT_BLACK);
      inicializarGrafico();
      mostrarModo();
      Serial.println("Cambiado a MODO LECTURA");
      break;
      
    case MODE_WRITE:
      detachInterrupt(digitalPinToInterrupt(SENSOR_PIN));
      pinMode(SENSOR_PIN, OUTPUT);
      digitalWrite(SENSOR_PIN, LOW);
      inicializarGenerador();
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
      Serial.println("Cambiado a MODO ESCRITURA - Voltaje desactivado");
      break;
      
    case MODE_PRESSURE:
      pinMode(SENSOR_PIN, INPUT);
      digitalWrite(SENSOR_PIN, LOW);
      inicializarModoPressure();
      tft.fillScreen(TFT_BLACK);
      inicializarGrafico();
      mostrarModo();
      Serial.println("Cambiado a MODO PRESION - Histórico reseteado");
      break;
      
    case MODE_RECIRCULATOR:
      pinMode(SENSOR_PIN, INPUT);
      digitalWrite(SENSOR_PIN, LOW);
      if (recirculator_power_state) {
        setRecirculatorPower(false);
      }
      tft.fillScreen(TFT_BLACK);
      mostrarModo();
      mostrarVoltaje();
      mostrarPantallaRecirculador();
      Serial.println("Cambiado a MODO RECIRCULADOR - Generación de pulsos detenida");
      break;
      
    case MODE_WIFI_SCAN:
      pinMode(SENSOR_PIN, INPUT);
      digitalWrite(SENSOR_PIN, LOW);
      wifi_page = 0;
      mostrarPantallaScanningWiFi();
      escanearWiFi();
      Serial.println("Cambiado a MODO WiFi SCAN");
      break;
  }
}

void manejarBotonIzquierdo() {
  updateUserActivity();
  
  if (current_mode == MODE_WRITE) {
    manejarBotonIzquierdoWrite();
  } else if (current_mode == MODE_RECIRCULATOR) {
    manejarBotonIzquierdoRecirculator();
  } else if (current_mode == MODE_WIFI_SCAN) {
    manejarBotonIzquierdoWiFi();
  }
}

void manejarBotonDerecho() {
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

void mostrarInfoSensor() {
  if (current_mode == MODE_READ) {
    mostrarInfoSensorRead();
  } else if (current_mode == MODE_PRESSURE) {
    mostrarInfoSensorPressure();
  }
}

void loop() {
  unsigned long current_time = millis();
  static unsigned long last_button_time = 0;

  // Verificar timeout para sleep
  if (!in_sleep_mode && (current_time - last_user_activity_time >= SLEEP_TIMEOUT_MS)) {
    enterSleepMode();
    return;
  }

  // Actualizar voltaje cada 500ms (DESACTIVADO en MODE_WRITE para evitar glitches)
  if (current_mode != MODE_WRITE && current_time - last_voltage_update >= VOLTAGE_UPDATE_MS) {
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
