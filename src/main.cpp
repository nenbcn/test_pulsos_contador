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
  delay(100);  // Esperar para que serial se estabilice
  
  // Verificar si despertamos del sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool waking_from_sleep = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || 
                            wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 ||
                            wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  
  if (waking_from_sleep && had_sleep) {
    Serial.println("=== DESPERTANDO DEL SLEEP ===");
    Serial.print("Razón: ");
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) Serial.println("Botón IZQUIERDO");
    else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) Serial.println("Botón DERECHO");
    else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) Serial.println("TIMEOUT SEGURIDAD (10min)");
    Serial.print("Restaurando modo: ");
    Serial.println(saved_mode);
    // Validar que saved_mode es válido
    if (saved_mode < MODE_READ || saved_mode > MODE_WIFI_SCAN) {
      Serial.println("ERROR: modo guardado corrupto, resetear a READ");
      saved_mode = MODE_READ;
    }
  } else {
    Serial.println("=== INICIO NORMAL (NO desde sleep) ===");
    saved_mode = MODE_READ;  // Modo por defecto
    had_sleep = false;       // Resetear flag
    waking_from_sleep = false;  // Forzar inicio normal
  }
  
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  
  // No configurar GPIO21 aquí - se hará en inicializarModoRead()
  // para evitar wake-ups no deseados en modo WRITE

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
  
  // Leer voltaje inicial
  voltaje = leerVoltaje();
  
  // Inicializar timer de actividad del usuario
  last_user_activity_time = millis();
  in_sleep_mode = false;
  
  Serial.println("=== Sistema de sleep configurado ===");
  Serial.print("Tiempo hasta sleep: ");
  Serial.print(SLEEP_TIMEOUT_MS / 1000);
  Serial.println(" segundos de inactividad");
  Serial.print("Actividad inicial: ");
  Serial.print(last_user_activity_time);
  Serial.println(" ms");
  
  Serial.println("=== TTGO T-Display - Monitor/Generador/Presion/WiFi Scanner ===");
  Serial.println("GPIO21 - Sensor/Generador configurado");
  Serial.println("GPIO32/22 - I2C para sensor de presion WNK1MA (SDA/SCL)");
  Serial.println("GPIO15/12/17/13 - Recirculador (Temp/Rele/Buzzer/LED)");
  Serial.println("Boton IZQUIERDO: Toggle bomba / Cambiar pagina WiFi");
  Serial.println("Boton DERECHO: Ciclar READ->WRITE->PRESSURE->RECIR->WiFi->READ");
  Serial.println("Sleep automático: 5 minutos sin actividad de BOTONES");
  Serial.println("Wake-up: SOLO por botones GPIO0 y GPIO35 (NO por GPIO21)");
  Serial.println("Escala gráfico: 0-75Hz (fija) / AUTO (presión)");
  
  // Forzar inicialización del modo para configurar GPIO21 correctamente
  if (waking_from_sleep && had_sleep) {
    Serial.print("Sistema reactivado desde sleep - Restaurando modo: ");
    Serial.println(saved_mode);
    // IMPORTANTE: Registrar actividad al despertar para evitar sleep inmediato
    last_user_activity_time = millis();
    Serial.println("*** ACTIVIDAD REGISTRADA AL DESPERTAR - Timer reseteado ***");
    current_mode = (SystemMode)((saved_mode + 1) % 5); // Forzar diferente para que cambiarModo() ejecute
    cambiarModo(saved_mode);
  } else {
    Serial.println("Modo inicial: LECTURA");
    current_mode = MODE_WRITE; // Forzar diferente para que cambiarModo() ejecute
    cambiarModo(MODE_READ);
  }
}

void cambiarModo(SystemMode nuevo_modo) {
  if (nuevo_modo == current_mode) return;
  
  SystemMode modo_anterior = current_mode;
  current_mode = nuevo_modo;
  
  updateUserActivity();
  Serial.println("[MODO] Actividad registrada al cambiar modo");
  
  switch (nuevo_modo) {
    case MODE_READ:
      inicializarModoRead();
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_GREEN);
      tft.setTextSize(2);
      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.drawString("READ", 5, 5);
      inicializarGrafico();
      Serial.println("Cambiado a MODO LECTURA");
      break;
      
    case MODE_WRITE:
      detachInterrupt(digitalPinToInterrupt(SENSOR_PIN));
      pinMode(SENSOR_PIN, OUTPUT);
      digitalWrite(SENSOR_PIN, LOW);
      Serial.println("=== MODO WRITE INICIALIZADO ===");
      Serial.println("GPIO21: OUTPUT configurado (interrupción deshabilitada)");
      Serial.println("Pin establecido en LOW");
      inicializarGenerador();
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_YELLOW);
      tft.setTextSize(2);
      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(TEST_CASE_NAMES[current_test], 5, 5);
      inicializarGrafico();
      Serial.println("Modo WRITE inicializado");
      break;
      
    case MODE_PRESSURE:
      pinMode(SENSOR_PIN, INPUT);
      digitalWrite(SENSOR_PIN, LOW);
      inicializarModoPressure();
      tft.fillScreen(TFT_BLACK);
      inicializarGrafico();

      Serial.println("Cambiado a MODO PRESION - Histórico reseteado");
      break;
      
    case MODE_RECIRCULATOR:
      pinMode(SENSOR_PIN, INPUT);
      digitalWrite(SENSOR_PIN, LOW);
      if (recirculator_power_state) {
        setRecirculatorPower(false);
      }
      tft.fillScreen(TFT_BLACK);

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
  Serial.println("[BTN] Botón izquierdo - Actividad registrada");
  
  if (current_mode == MODE_WRITE) {
    manejarBotonIzquierdoWrite();
  } else if (current_mode == MODE_RECIRCULATOR) {
    manejarBotonIzquierdoRecirculator();
  } else if (current_mode == MODE_WIFI_SCAN) {
    manejarBotonIzquierdoWiFi();
  }
}

void manejarBotonDerecho() {
  updateUserActivity();
  Serial.println("[BTN] Botón derecho - Actividad registrada");
  
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
  static bool first_loop = true;
  
  // En el primer loop, garantizar que no entre en sleep por al menos 2 segundos
  if (first_loop) {
    last_user_activity_time = millis();
    first_loop = false;
    Serial.println("Loop iniciado - actividad registrada");
  }

  // Verificar timeout para sleep (SOLO después de 5 minutos)
  unsigned long time_since_activity = current_time - last_user_activity_time;
  
  // Debug: Mostrar tiempo restante cada 30 segundos
  static unsigned long last_sleep_debug = 0;
  if (current_time - last_sleep_debug >= 30000) {
    unsigned long time_remaining = (SLEEP_TIMEOUT_MS - time_since_activity) / 1000;
    Serial.print("[SLEEP] Tiempo hasta sleep: ");
    Serial.print(time_remaining);
    Serial.println(" segundos");
    last_sleep_debug = current_time;
  }
  
  if (!in_sleep_mode && time_since_activity >= SLEEP_TIMEOUT_MS) {
    Serial.print("*** ENTRANDO EN SLEEP - inactividad de ");
    Serial.print(time_since_activity / 1000);
    Serial.println(" segundos ***");
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
  
  // Actualizar buzzer (no bloqueante)
  updateBuzzer();
}
