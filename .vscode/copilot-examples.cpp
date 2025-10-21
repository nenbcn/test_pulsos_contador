// Ejemplos de código para GitHub Copilot

// PATRON: Función de actualización de UI con optimización
void mostrarElemento() {
  static String last_text = "";
  String current_text = "nuevo valor";
  
  if (current_text != last_text) {
    tft.fillRect(x, y, width, height, TFT_BLACK); // Limpiar área
    tft.setTextColor(TFT_COLOR);
    tft.drawString(current_text, x, y);
    last_text = current_text;
  }
}

// PATRON: Lectura de sensor I2C con validación
SensorReading readSensor() {
  SensorReading reading;
  reading.isValid = false;
  
  Wire.beginTransmission(SENSOR_ADDR);
  Wire.write(SENSOR_CMD);
  if (Wire.endTransmission(true) != 0) return reading;
  
  uint8_t bytes = Wire.requestFrom(SENSOR_ADDR, 3);
  if (bytes == 3) {
    reading.value = (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
    reading.isValid = true;
  }
  return reading;
}

// PATRON: Control de botones con debounce
static unsigned long last_button_time = 0;
if (millis() - last_button_time > 300) {
  if (digitalRead(BUTTON_PIN) == LOW) {
    // Acción del botón
    updateActivity();
    last_button_time = millis();
  }
}

// PATRON: Actualización de gráfico con array circular
void updateGraph(float new_value) {
  graph_data[graph_index] = new_value;
  
  // Dibujar gráfico completo
  tft.fillRect(GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_BLACK);
  for (int i = 1; i < GRAPH_WIDTH; i++) {
    int idx1 = (graph_index - GRAPH_WIDTH + i + GRAPH_WIDTH) % GRAPH_WIDTH;
    int idx2 = (graph_index - GRAPH_WIDTH + i + 1 + GRAPH_WIDTH) % GRAPH_WIDTH;
    
    int y1 = GRAPH_Y + GRAPH_HEIGHT - (graph_data[idx1] * GRAPH_HEIGHT / max_scale);
    int y2 = GRAPH_Y + GRAPH_HEIGHT - (graph_data[idx2] * GRAPH_HEIGHT / max_scale);
    
    tft.drawLine(GRAPH_X + i - 1, y1, GRAPH_X + i, y2, TFT_CYAN);
  }
  
  graph_index = (graph_index + 1) % GRAPH_WIDTH;
}
