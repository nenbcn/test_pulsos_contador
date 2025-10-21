# Especificaciones Técnicas para GitHub Copilot

## Hardware Target: TTGO T-Display ESP32-S3

### Pinout Crítico
```
GPIO0  -> Botón izquierdo (INPUT_PULLUP)
GPIO35 -> Botón derecho (INPUT_PULLUP) 
GPIO4  -> Control retroiluminación TFT (OUTPUT)
GPIO21 -> Sensor/Generador pulsos (INPUT/OUTPUT)
GPIO32 -> I2C SDA (sensor presión WNK1MA)
GPIO22 -> I2C SCL (sensor presión WNK1MA)
GPIO36 -> ADC lectura voltaje (INPUT)
```

### Configuración I2C
```cpp
#define I2C_SDA 32
#define I2C_SCL 22
#define WNK1MA_ADDR 0x6D
#define WNK1MA_CMD 0x06
Wire.begin(I2C_SDA, I2C_SCL);
Wire.setClock(100000); // 100kHz
```

### Configuración TFT
```cpp
TFT_eSPI tft = TFT_eSPI();
tft.init();
tft.setRotation(1); // Horizontal 240x135
```

### Rangos y Límites
- **Gráfico pulsos**: 0-75 Hz (escala fija)
- **Gráfico presión**: AUTO-escala dinámica  
- **Array gráficos**: 200 puntos circular
- **Frecuencia I2C**: 100Hz (cada 10ms)
- **Debounce botones**: 300ms
- **Sleep timeout**: 5 minutos
- **Voltaje range**: 0-3.6V

### Colores TFT Estándar
- `TFT_GREEN`: Modo READ, voltaje
- `TFT_RED`: Modo WRITE, errores
- `TFT_MAGENTA`: Modo PRESSURE
- `TFT_CYAN`: Modo WiFi, gráfico pulsos
- `TFT_YELLOW`: Frecuencia, warnings
- `TFT_DARKGREY`: Labels, referencias
- `TFT_WHITE`: Texto principal
- `TFT_BLACK`: Background, limpiar áreas

### Patrones de Memoria
```cpp
// Arrays estáticos para gráficos
float graph_data[GRAPH_WIDTH];        // 200 * 4 = 800 bytes
float pressure_graph_data[GRAPH_WIDTH]; // 200 * 4 = 800 bytes

// Optimización redibujado
static String last_text = "";
if (new_text != last_text) {
  // Solo redibujar si cambió
}
```
