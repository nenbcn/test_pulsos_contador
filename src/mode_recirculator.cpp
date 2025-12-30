#include "mode_recirculator.h"
#include "display.h"

// Variables específicas del modo RECIRCULATOR
bool recirculator_power_state = false;
unsigned long recirculator_start_time = 0;
float recirculator_temp = 0.0;
float recirculator_max_temp = 30.0;

void inicializarRecirculador() {
  Serial.println("\n=== INICIALIZANDO RECIRCULADOR ===");
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("✓ Relé (GPIO12) configurado");
  
  pinMode(BUZZER_PIN, OUTPUT);
  ledcSetup(0, 5000, 10);
  ledcAttachPin(BUZZER_PIN, 0);
  Serial.println("✓ Buzzer (GPIO17) configurado con LEDC (10 bits - máxima potencia)");
  
  Serial.println("Probando buzzer...");
  for(int i = 0; i < 3; i++) {
    playTone(1000, 100);
    stopTone();
    delay(100);
  }
  Serial.println("✓ Test buzzer completado");
  
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
  
  pixel.begin();
  pixel.setBrightness(50);
  pixel.setPixelColor(0, pixel.Color(255, 0, 0));
  pixel.show();
  Serial.println("✓ NeoPixel (GPIO13) inicializado");
  
  recirculator_power_state = false;
  recirculator_temp = 0.0;
  Serial.println("=== RECIRCULADOR LISTO ===\n");
}

void setRecirculatorPower(bool state) {
  recirculator_power_state = state;
  
  if (state) {
    playTone(1000, 150);
    stopTone();
    delay(50);
    
    digitalWrite(RELAY_PIN, HIGH);
    recirculator_start_time = millis();
    pixel.setPixelColor(0, pixel.Color(0, 255, 0));
    pixel.show();
    Serial.println("✅ Bomba ENCENDIDA");
  } else {
    digitalWrite(RELAY_PIN, LOW);
    pixel.setPixelColor(0, pixel.Color(255, 0, 0));
    pixel.show();
    
    delay(100);
    playTone(500, 200);
    stopTone();
    Serial.println("🛑 Bomba APAGADA");
  }
}

void leerTemperaturaRecirculador() {
  Serial.println("📡 Solicitando temperatura...");
  sensorTemp.requestTemperatures();
  delay(10);
  
  float temp = sensorTemp.getTempCByIndex(0);
  Serial.printf("📊 Lectura raw del sensor: %.2f°C\n", temp);
  
  if (temp != -127.0 && temp != 85.0 && temp > -50.0 && temp < 125.0) {
    recirculator_temp = temp;
    Serial.printf("✅ Temperatura válida actualizada: %.2f°C\n", recirculator_temp);
  } else {
    static bool simulation_warned = false;
    if (!simulation_warned) {
      Serial.println("⚠️ SENSOR NO RESPONDE - ACTIVANDO MODO SIMULACIÓN");
      Serial.println("   Temperatura simulada: 25°C + variación aleatoria");
      simulation_warned = true;
    }
    
    static float simulated_temp = 25.0;
    simulated_temp += (random(-10, 10) / 10.0);
    simulated_temp = constrain(simulated_temp, 23.0, 27.0);
    
    recirculator_temp = simulated_temp;
    Serial.printf("🎭 Temperatura SIMULADA: %.2f°C\n", recirculator_temp);
  }
}

void controlarRecirculadorAutomatico() {
  if (!recirculator_power_state) return;
  
  unsigned long elapsed = millis() - recirculator_start_time;
  
  if (recirculator_temp >= recirculator_max_temp) {
    setRecirculatorPower(false);
    delay(200);
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
      delay(100);
    }
    stopTone();
    Serial.println("🎯 Temperatura alcanzada - Apagado automático");
    return;
  }
  
  if (elapsed >= RECIRCULATOR_MAX_TIME) {
    setRecirculatorPower(false);
    delay(200);
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
  
  bool needs_update = (last_power_state != recirculator_power_state) ||
                      (abs(last_temp - recirculator_temp) > 0.5) ||
                      (abs((long)last_elapsed - (long)elapsed) >= 1);
  
  if (!needs_update) return;
  
  tft.fillRect(0, 22, 240, 113, TFT_BLACK);
  
  tft.setTextColor(TFT_ORANGE);
  tft.setTextFont(2);
  tft.drawString("RECIRCULATOR", 10, 25);
  
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
  
  tft.setTextColor(TFT_CYAN);
  tft.setTextFont(2);
  char temp_str[30];
  snprintf(temp_str, sizeof(temp_str), "Temp: %.1fC", recirculator_temp);
  tft.drawString(temp_str, 10, 70);
  
  tft.setTextColor(TFT_YELLOW);
  char max_temp_str[30];
  snprintf(max_temp_str, sizeof(max_temp_str), "Max:  %.1fC", recirculator_max_temp);
  tft.drawString(max_temp_str, 10, 90);
  
  if (recirculator_power_state) {
    tft.setTextColor(TFT_MAGENTA);
    char time_str[30];
    unsigned long total_seconds = RECIRCULATOR_MAX_TIME / 1000;
    snprintf(time_str, sizeof(time_str), "Tiempo: %02lu:%02lu / %02lu:%02lu",
             elapsed / 60, elapsed % 60,
             total_seconds / 60, total_seconds % 60);
    tft.drawString(time_str, 10, 110);
  }
  
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
  
  if (current_time - last_temp_read >= 1000) {
    leerTemperaturaRecirculador();
    last_temp_read = current_time;
  }
  
  if (current_time - last_debug_print >= 3000) {
    Serial.printf("🌡️ Temp actual: %.2f°C | Max: %.1f°C | Bomba: %s\n", 
                  recirculator_temp, recirculator_max_temp, 
                  recirculator_power_state ? "ON" : "OFF");
    last_debug_print = current_time;
  }
  
  controlarRecirculadorAutomatico();
  mostrarPantallaRecirculador();
}

void manejarBotonIzquierdoRecirculator() {
  setRecirculatorPower(!recirculator_power_state);
}
