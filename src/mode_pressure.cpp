#include "mode_pressure.h"
#include "display.h"
#include <float.h>

// Variables específicas del modo PRESSURE
float pressure_graph_data[GRAPH_WIDTH];
int pressure_graph_index = 0;
float pressure_min_scale = 0.0;
float pressure_max_scale = 100.0;
float pressure_historical_min = FLT_MAX;
float pressure_historical_max = FLT_MIN;
bool pressure_history_initialized = false;
unsigned long last_pressure_read = 0;
bool pressure_auto_scale = true;

WNK1MA_Reading readWNK1MA() {
    WNK1MA_Reading reading;
    reading.isValid = false;
    reading.timestamp = millis();

    Wire.beginTransmission(WNK1MA_ADDR);
    Wire.write(WNK1MA_CMD);
    if (Wire.endTransmission(true) != 0) {
        Serial.println("[ERROR] I2C endTransmission failed");
        return reading;
    }
    
    delayMicroseconds(300);

    uint8_t bytesRead = Wire.requestFrom(WNK1MA_ADDR, 3);
    if (bytesRead == 3) {
        uint32_t raw = (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
        if (raw > 0 && raw < 16777215) {
            reading.rawValue = raw;
            reading.isValid = true;
        }
    } else {
        Serial.printf("[ERROR] I2C requestFrom failed, expected 3 bytes, got %d\n", bytesRead);
    }
    return reading;
}

void inicializarModoPressure() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  pressure_history_initialized = false;
  pressure_historical_min = FLT_MAX;
  pressure_historical_max = FLT_MIN;
  Serial.println("I2C inicializado para sensor de presión WNK1MA");
  Serial.println("Modo PRESSURE inicializado - Histórico reseteado");
}

void actualizarHistoricoPresion(float nuevo_valor) {
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
  
  float range = pressure_historical_max - pressure_historical_min;
  if (range > 0) {
    pressure_min_scale = pressure_historical_min - (range * 0.05);
    pressure_max_scale = pressure_historical_max + (range * 0.05);
  } else {
    pressure_min_scale = pressure_historical_min - 10.0;
    pressure_max_scale = pressure_historical_max + 10.0;
  }
}

void actualizarGraficoPresion(float nuevo_valor) {
  actualizarHistoricoPresion(nuevo_valor);
  actualizarGraficoGenerico(pressure_graph_data, &pressure_graph_index, nuevo_valor,
                            pressure_min_scale, pressure_max_scale,
                            TFT_BLUE, TFT_MAGENTA, pressure_auto_scale);
}

void manejarModoPressure() {
  unsigned long current_time = millis();
  
  if (current_time - last_pressure_read >= PRESSURE_READ_INTERVAL_MS) {
    WNK1MA_Reading reading = readWNK1MA();
    
    if (reading.isValid) {
      float pressure_value = (float)reading.rawValue;
      actualizarGraficoPresion(pressure_value);
      
      static float last_stable_pressure = 0.0;
      if (abs(pressure_value - last_stable_pressure) > 10.0) {
        last_stable_pressure = pressure_value;
      }
      
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
      static unsigned long last_error_time = 0;
      if (current_time - last_error_time >= SERIAL_DEBUG_SLOW_MS) {
        Serial.println("ERROR: No se puede leer el sensor de presión I2C");
        last_error_time = current_time;
      }
    }
    
    last_pressure_read = current_time;
  }
}

void mostrarInfoSensorPressure() {
  static float last_pressure_value = -1.0;
  static char last_phase_text[50] = "";
  static char last_scale_text[20] = "";
  
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
  
  const char* mode_text = "Sensor I2C @ 100Hz (Historico)";
  if (strcmp(mode_text, last_phase_text) != 0) {
    tft.fillRect(5, 25, 230, 10, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setTextFont(1);
    tft.drawString(mode_text, 5, 25);
    strcpy(last_phase_text, mode_text);
  }
  
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
