#include "mode_read.h"
#include "display.h"

// Variables específicas del modo READ
volatile unsigned long pulse_count = 0;
unsigned long last_pulse_count = 0;
unsigned long last_pulse_time = 0;
float pulse_frequency = 0.0;

// Función de interrupción para contar pulsos
void IRAM_ATTR pulseInterrupt() {
  pulse_count++;
}

void inicializarModoRead() {
  pinMode(SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseInterrupt, RISING);
  
  // Resetear contadores al entrar en modo READ
  pulse_count = 0;
  last_pulse_count = 0;
  last_pulse_time = millis();
  pulse_frequency = 0.0;
  
  Serial.println("Modo READ inicializado - Contadores reseteados");
}

void manejarModoRead() {
  unsigned long current_time = millis();
  
  // Actualizar gráfico con frecuencia leída
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

// Función para mostrar información del sensor en modo READ
void mostrarInfoSensorRead() {
  static char last_freq_text[30] = "";
  static char last_total_text[40] = "";
  static char last_scale_text[20] = "";
  
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
}
