#include "display.h"
#include "mode_read.h"
#include "mode_pressure.h"

void playTone(int frequency, int duration_ms) {
  if (frequency > 0) {
    ledcWriteTone(0, frequency);
    ledcWrite(0, 512);
  } else {
    ledcWriteTone(0, 0);
  }
  delay(duration_ms);
}

void stopTone() {
  ledcWriteTone(0, 0);
}

uint16_t getSignalColor(int32_t rssi) {
  if (rssi >= -50) {
    return 0x07E0;
  } else if (rssi >= -65) {
    return TFT_GREEN;
  } else if (rssi >= -75) {
    return TFT_YELLOW;
  } else if (rssi >= -85) {
    return TFT_ORANGE;
  } else {
    return TFT_RED;
  }
}

void mostrarVoltaje() {
  static char last_voltage_text[10] = "";
  char voltage_text[10];
  
  snprintf(voltage_text, sizeof(voltage_text), "%.2fV", voltaje);
  
  if (strcmp(voltage_text, last_voltage_text) != 0) {
    tft.fillRect(170, 2, 65, 15, TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(1);
    tft.setTextFont(2);
    tft.drawString(voltage_text, 175, 5);
    strcpy(last_voltage_text, voltage_text);
  }
}

void mostrarModo() {
  static SystemMode last_mode = MODE_WIFI_SCAN;
  
  if (current_mode != last_mode) {
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

void dibujarLineasReferencia() {
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/2, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/2, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/4, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + 3*GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + 3*GRAPH_HEIGHT/4, TFT_DARKGREY);
}

void actualizarGraficoGenerico(float* data, int* index, float nuevo_valor, 
                                float min_scale, float max_scale, 
                                uint16_t color_fill, uint16_t color_line,
                                bool auto_scale) {
  data[*index] = nuevo_valor;
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_BLACK);
  dibujarLineasReferencia();
  
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
      
      y1 = constrain(y1, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT);
      y2 = constrain(y2, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT);
      
      tft.drawLine(GRAPH_X + i - 1, y1, GRAPH_X + i, y2, color_line);
      tft.drawLine(GRAPH_X + i - 1, y1 + 1, GRAPH_X + i, y2 + 1, color_line);
    }
  }
  
  *index = (*index + 1) % GRAPH_WIDTH;
}

void dibujarMarcoGrafico(bool es_presion) {
  tft.drawRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_WIDTH + 2, GRAPH_HEIGHT + 2, TFT_WHITE);
  
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/2, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/2, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT/4, TFT_DARKGREY);
  tft.drawLine(GRAPH_X, GRAPH_Y + 3*GRAPH_HEIGHT/4, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + 3*GRAPH_HEIGHT/4, TFT_DARKGREY);
  
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextFont(1);
  
  if (es_presion) {
    tft.drawString("HIST", GRAPH_X - 18, GRAPH_Y - 2);
    tft.drawString("MAX", GRAPH_X - 15, GRAPH_Y + GRAPH_HEIGHT - 2);
  } else {
    tft.drawString("120", GRAPH_X - 18, GRAPH_Y - 2);
    tft.drawString("90", GRAPH_X - 15, GRAPH_Y + GRAPH_HEIGHT/4 - 2);
    tft.drawString("60", GRAPH_X - 15, GRAPH_Y + GRAPH_HEIGHT/2 - 2);
    tft.drawString("30", GRAPH_X - 15, GRAPH_Y + 3*GRAPH_HEIGHT/4 - 2);
    tft.drawString("0", GRAPH_X - 8, GRAPH_Y + GRAPH_HEIGHT - 2);
  }
}

void inicializarGrafico() {
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    graph_data[i] = 0.0;
  }
  
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    extern float pressure_graph_data[GRAPH_WIDTH];
    pressure_graph_data[i] = 0.0;
  }
  
  dibujarMarcoGrafico(current_mode == MODE_PRESSURE);
}

void actualizarGrafico(float nueva_frecuencia) {
  actualizarGraficoGenerico(graph_data, &graph_index, nueva_frecuencia,
                            0.0, max_freq_scale,
                            TFT_BLUE, TFT_CYAN, false);
}
