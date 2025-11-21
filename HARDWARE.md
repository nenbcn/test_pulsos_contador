# Especificaciones Técnicas para GitHub Copilot

## Hardware Target: TTGO T-Display ESP32-S3

### 📌 GPIOs Expuestos en la Placa (Verificados Físicamente)
```
Pines disponibles: 2, 3, 12, 13, 15, 17, 21, 22, 25, 26, 27, 32, 33, 36, 37, 38, 39

Características:
- GPIO2, 3, 12, 13, 15, 17, 21, 22, 25, 26, 27, 32, 33: Digitales (INPUT/OUTPUT)
- GPIO36, 37, 38, 39: Solo INPUT (ADC)
- Botones built-in: GPIO0 (izquierdo), GPIO35 (derecho)
- Evitar: GPIO16 (Touch INT), GPIO4 (usado internamente)
```

### Pinout Crítico
```
GPIO0  -> Botón izquierdo (INPUT_PULLUP) - Built-in
GPIO35 -> Botón derecho (INPUT_PULLUP) - Built-in
GPIO4  -> Control retroiluminación TFT (OUTPUT) - Built-in
GPIO15 -> Sensor Temperatura DS18B20 (INPUT) - Recirculador
GPIO13 -> LED NeoPixel (OUTPUT) - Recirculador
GPIO2  -> Buzzer PWM (OUTPUT) - Recirculador
GPIO12 -> Relé (OUTPUT) - Recirculador
GPIO21 -> Sensor/Generador pulsos (INPUT/OUTPUT)
GPIO22 -> I2C SCL (sensor presión WNK1MA)
GPIO32 -> I2C SDA (sensor presión WNK1MA)
GPIO36 -> ADC lectura voltaje (INPUT)
```

### Configuración I2C
```cpp
#define I2C_SDA 32  // GPIO32 para SDA
#define I2C_SCL 22  // GPIO22 para SCL
#define WNK1MA_ADDR 0x6D
#define WNK1MA_CMD 0x06
Wire.begin(I2C_SDA, I2C_SCL);
Wire.setClock(100000); // 100kHz
```

### Configuración Recirculador
```cpp
#define TEMP_SENSOR_PIN 15  // DS18B20 1-Wire
#define NEOPIXEL_PIN 13     // WS2812B LED
#define BUZZER_PIN 17       // PWM para tonos
#define RELAY_PIN 12        // Control relé
```

---

## 🔌 MAPA COMPLETO DE CONECTORES

### 🔴 CONECTOR 1: Sensor de Pulsos (3 pines)
```
Cable NEGRO  → GND (tierra)
Cable ROJO   → 3V3 (alimentación 3.3V)
Cable VERDE  → GPIO21 (señal pulsos) ✅ VERIFICADO
```

### 🟣 CONECTOR 2: Sensor de Presión WNK1MA I2C (4 pines)
```
Cable NEGRO  → GND (tierra)
Cable ROJO   → 3V3 (alimentación 3.3V)
Cable AZUL   → GPIO32 (I2C SDA - datos)
Cable VERDE  → GPIO22 (I2C SCL - reloj)
```

### 🟡 CONECTOR 3: Recirculador Principal (5 pines)
```
Cable NEGRO    → GND (tierra)
Cable AZUL     → 5V (alimentación 5V)
Cable VERDE    → 3V3 (alimentación 3.3V)
Cable AMARILLO → GPIO15 (Sensor Temp DS18B20)
Cable ROJO     → GPIO12 (Control Relé)
```

### 🟠 CONECTOR 4: LED NeoPixel (3 pines)
```
Cable NEGRO   → GND (tierra)
Cable ROJO    → 5V (alimentación 5V)
Cable NARANJA → GPIO13 (señal NeoPixel)
```

### ⚪ CONECTOR 5: Buzzer (3 pines)
```
Cable NEGRO  → GND (tierra)
Cable ROJO   → 5V (alimentación 5V)
Cable BLANCO → GPIO17 (señal PWM)
```

### ⚠️ Tabla Resumen de Cables por Color
| Color | Conector | Destino | Función |
|-------|----------|---------|---------|
| **NEGRO** | Todos (5) | GND | Tierra común |
| **ROJO** | Pulsos | 3V3 | Alim. sensor pulsos |
| **ROJO** | Presión | 3V3 | Alim. sensor presión |
| **ROJO** | Recirculador | GPIO12 | ⚠️ Control relé (SEÑAL) |
| **ROJO** | NeoPixel | 5V | Alim. LED |
| **ROJO** | Buzzer | 5V | Alim. Buzzer |
| **VERDE** | Pulsos | GPIO21 | Señal pulsos |
| **VERDE** | Presión | GPIO22 | I2C SCL |
| **VERDE** | Recirculador | 3V3 | Alimentación 3.3V |
| **AZUL** | Presión | GPIO32 | I2C SDA |
| **AZUL** | Recirculador | 5V | Alimentación 5V |
| **AMARILLO** | Recirculador | GPIO15 | Temp DS18B20 |
| **NARANJA** | NeoPixel | GPIO13 | Señal LED |
| **BLANCO** | Buzzer | GPIO17 | PWM Buzzer |

**IMPORTANTE**: El cable ROJO del recirculador NO es alimentación, es señal de control del relé.

### Configuración TFT
```cpp
TFT_eSPI tft = TFT_eSPI();
tft.init();
tft.setRotation(1); // Horizontal 240x135
```

### Rangos y Límites
- **Gráfico pulsos**: 0-75 Hz (escala fija)
- **Gráfico presión**: Escalado histórico (min/max desde inicio)  
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
