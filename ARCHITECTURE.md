# Arquitectura del Código

## 🏗️ Visión General

Firmware para ESP32 TTGO T-Display con 5 modos de operación principales. Arquitectura **single-threaded** basada en Arduino framework (no usa FreeRTOS).

```
┌─────────────────────────────────────────────────────────┐
│                   ESP32 Main Loop                       │
│  Single-threaded, event-driven, non-blocking            │
└─────────────────────────────────────────────────────────┘
           │
           ├─> Botones (debouncing)
           ├─> Modos (FSM)
           ├─> Sensores (pulsos, presión, voltaje, temperatura)
           ├─> Generador de pulsos
           ├─> WiFi Scanner
           ├─> Recirculador (control bomba + sensor temperatura)
           └─> Display (TFT_eSPI)
```

---

## 🔄 Máquina de Estados (FSM)

### Estados Principales

```cpp
enum SystemMode {
  MODE_READ,         // Lectura de pulsos + gráfico frecuencia
  MODE_WRITE,        // Generación de pulsos (patrón sofisticado)
  MODE_PRESSURE,     // Sensor I2C WNK1MA a 100Hz + gráfico
  MODE_RECIRCULATOR, // Control bomba recirculación + sensor temperatura
  MODE_WIFI_SCAN     // Escaneo WiFi con paginación
};
```

### Transiciones de Estado

```
    [Botón Derecho]
         ↓
MODE_READ ←→ MODE_WRITE ←→ MODE_PRESSURE ←→ MODE_RECIRCULATOR ←→ MODE_WIFI_SCAN
    ↑                                                                    ↓
    └────────────────────────────────────────────────────────────────────┘
```

**Control:**
- **Botón Derecho (GPIO35)**: Cambiar modo (ciclo circular)
- **Botón Izquierdo (GPIO0)**: Acción específica del modo
  - `MODE_RECIRCULATOR`: Toggle bomba ON/OFF
  - `MODE_WIFI_SCAN`: Cambiar página WiFi

---

## 📂 Estructura del Código

### 1. Variables Globales (líneas 1-150)
```cpp
// Hardware
TFT_eSPI tft;
volatile unsigned long pulse_count;  // ISR-safe

// Sistema
SystemMode current_mode;
unsigned long last_user_activity_time;
bool in_sleep_mode;

// Gráficos circulares
float graph_data[GRAPH_WIDTH];       // MODE_READ
float pressure_graph_data[GRAPH_WIDTH]; // MODE_PRESSURE
int graph_index, pressure_graph_index;

// WiFi
WiFiNetwork wifi_networks[20];
int wifi_count, wifi_page;

// Generador de pulsos (MODE_WRITE)
TestCase current_test;          // TC6 por defecto (Secuencia Completa)
unsigned long test_start_time;

// Recirculador (MODE_RECIRCULATOR)
bool recirculator_power_state;
unsigned long recirculator_start_time;
float recirculator_temp;
float recirculator_max_temp;
```

### 2. Interrupciones (líneas 150-170)
```cpp
void IRAM_ATTR pulseInterrupt()    // Contador de pulsos (GPIO21)
void IRAM_ATTR wakeUpInterrupt()   // Wake-up desde sleep
```

### 3. Funciones de Sensores (líneas 170-300)
```cpp
WNK1MA_Reading readWNK1MA()        // I2C sensor presión
float leerVoltaje()                // ADC voltaje batería
void leerTemperaturaRecirculador() // DS18B20 OneWire temperatura
```

### 4. Funciones de Visualización (líneas 300-900)
```cpp
void mostrarGraficoFrecuencia()    // Gráfico MODE_READ
void mostrarGraficoPressure()      // Gráfico MODE_PRESSURE
void mostrarInfoSensor()           // UI MODE_READ
void mostrarInfoGenerador()        // UI MODE_WRITE
void mostrarInfoPresion()          // UI MODE_PRESSURE
void mostrarWiFiScan()             // UI MODE_WIFI_SCAN
void mostrarPantallaRecirculador() // UI MODE_RECIRCULATOR
void mostrarVoltaje()              // Barra de voltaje
```

### 5. Funciones de Lógica de Negocio (líneas 900-1100)
```cpp
// Generación de pulsos (MODE_WRITE)
float getBurstFrequency(unsigned long phase_elapsed)
float getStressFrequency(unsigned long stress_elapsed)
void generarPulsos()

// WiFi
void escanearWiFi()
uint16_t getWiFiColor(int32_t rssi)

// Recirculador (MODE_RECIRCULATOR)
void inicializarRecirculador()
void setRecirculatorPower(bool state)
void controlarRecirculadorAutomatico()

// Sleep
void updateUserActivity()
void enterSleepMode()
```

### 6. Manejo de Eventos (líneas 1070-1130)
```cpp
void manejarBotonIzquierdo()       // Navegación (ej: página WiFi)
void manejarBotonDerecho()         // Cambio de modos + lógica transición
```

### 7. Core del Sistema (líneas 1009-1286)
```cpp
void setup()                       // Inicialización hardware
void loop()                        // Main event loop
```

---

## ⚙️ Flujo del `loop()` Principal

```cpp
void loop() {
  unsigned long current_time = millis();
  
  // 1. Lectura de botones (debouncing)
  if (digitalRead(BUTTON_LEFT) == LOW && tiempo > debounce) {
    manejarBotonIzquierdo();
  }
  if (digitalRead(BUTTON_RIGHT) == LOW && tiempo > debounce) {
    manejarBotonDerecho();
  }
  
  // 2. Lógica específica del modo actual
  switch (current_mode) {
    case MODE_READ:
      // Calcular frecuencia de pulsos
      // Actualizar gráfico
      // Mostrar UI
      break;
      
    case MODE_WRITE:
      generarPulsos();  // Patrón sofisticado 29s
      // Actualizar UI
      break;
      
    case MODE_PRESSURE:
      // Leer sensor I2C @ 100Hz
      // Auto-escalar gráfico
      // Mostrar UI
      break;
      
    case MODE_RECIRCULATOR:
      // Leer temperatura DS18B20 @ 1Hz
      // Control automático bomba (temp/timeout)
      // Mostrar UI + indicadores LED
      break;
      
    case MODE_WIFI_SCAN:
      // Escanear WiFi periódicamente
      // Mostrar redes por páginas
      break;
  }
  
  // 3. Actualizar voltaje (500ms)
  if (current_time - last_voltage_update > 500) {
    voltaje = leerVoltaje();
    mostrarVoltaje();
  }
  
  // 4. Verificar timeout para sleep (5min)
  if (current_time - last_user_activity_time > SLEEP_TIMEOUT_MS) {
    enterSleepMode();
  }
}
```

---

## 🎯 MODE_WRITE: Patrón de Pulsos (29 segundos)

### Fases del Ciclo

```cpp
enum PatternPhase {
  PHASE_BURST1,       // 3s: Gradientes 30Hz→50Hz→30Hz
  PHASE_PAUSE1,       // 3s: Sin pulsos
  PHASE_BURST2,       // 3s: Gradientes 30Hz→50Hz→30Hz
  PHASE_PAUSE2,       // 3s: Sin pulsos
  PHASE_STRESS_BURST, // 10s: Test de carga (15Hz-100Hz variables)
  PHASE_PAUSE3        // 7s: Sin pulsos
};
```

### Lógica de Generación

```cpp
void generarPulsos() {
  // 1. Verificar cambio de fase
  if (phase_elapsed >= phase_durations[current_phase]) {
    current_phase = (PatternPhase)((current_phase + 1) % 6);
    phase_start_time = millis();
  }
  
  // 2. Obtener frecuencia según fase
  float frequency;
  if (current_phase == PHASE_BURST1 || current_phase == PHASE_BURST2) {
    frequency = getBurstFrequency(phase_elapsed);
  } else if (current_phase == PHASE_STRESS_BURST) {
    frequency = getStressFrequency(phase_elapsed);
  } else {
    frequency = 0; // PAUSE
  }
  
  // 3. Generar pulso con timing preciso
  if (frequency > 0 && millis() >= next_pulse_time) {
    pulse_state = !pulse_state;
    digitalWrite(SENSOR_PIN, pulse_state ? HIGH : LOW);
    next_pulse_time = millis() + (1000.0 / (2 * frequency));
  }
}
```

Ver `docs/pulse_implementation_guide.md` para detalles de implementación.

---

## 🔌 Configuración de Hardware

### Pines

| Pin | Función | Modo | Notas |
|-----|---------|------|-------|
| GPIO0 | Botón Izquierdo | INPUT_PULLUP | Wake-up desde sleep |
| GPIO35 | Botón Derecho | INPUT_PULLUP | Wake-up desde sleep |
| GPIO21 | Pulsos | INPUT/OUTPUT | Cambia según modo |
| GPIO32 | I2C SDA | I2C | Sensor WNK1MA |
| GPIO22 | I2C SCL | I2C | Sensor WNK1MA |
| GPIO36 | Voltaje ADC | INPUT | Lectura batería |
| GPIO4 | TFT Backlight | OUTPUT | Control brillo |
| GPIO15 | Sensor Temperatura | INPUT | DS18B20 OneWire |
| GPIO12 | Relé Bomba | OUTPUT | Control bomba |
| GPIO17 | Buzzer | OUTPUT | PWM para tonos |
| GPIO13 | NeoPixel LED | OUTPUT | Indicador visual |

### Interrupciones

```cpp
// MODE_READ: Interrupt en GPIO21 para contar pulsos
attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseInterrupt, RISING);

// Sleep: Interrupts en botones para wake-up
esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);   // Botón izquierdo
esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
```

---

## 📊 Gestión de Memoria

### Gráficos Circulares
```cpp
float graph_data[200];           // 200 * 4 bytes = 800 bytes
float pressure_graph_data[200];  // 200 * 4 bytes = 800 bytes
```

**Estrategia:** Buffers circulares con índice que se reinicia al llegar a GRAPH_WIDTH.

### WiFi Networks
```cpp
WiFiNetwork wifi_networks[20];   // Máximo 20 redes
// Cada red: ~50 bytes → ~1KB total
```

**Paginación:** Mostrar 5 redes por página (3 páginas máximo).

### Recirculador
```cpp
// Sin arrays grandes - solo variables de estado
bool recirculator_power_state;
float recirculator_temp;
// + librerías OneWire, DallasTemperature, Adafruit_NeoPixel
```

---

## 🌡️ MODE_RECIRCULATOR: Control de Bomba

### Propósito
Sistema de control para bomba de recirculación de agua caliente con:
- Sensor de temperatura DS18B20 (OneWire)
- Control de relé para bomba
- Indicador LED NeoPixel de estado
- Buzzer para feedback sonoro
- Apagado automático por temperatura o timeout

### Hardware Asignado
```cpp
#define TEMP_SENSOR_PIN 15   // DS18B20 (cable AMARILLO)
#define RELAY_PIN 12         // Control relé (cable ROJO)
#define BUZZER_PIN 17        // PWM Buzzer (cable BLANCO)
#define NEOPIXEL_PIN 13      // WS2812B LED (cable NARANJA)
```

### Lógica de Control

```cpp
void controlarRecirculadorAutomatico() {
  if (!recirculator_power_state) return;
  
  unsigned long elapsed = millis() - recirculator_start_time;
  
  // CONDICIÓN 1: Temperatura alcanzada
  if (recirculator_temp >= recirculator_max_temp) {
    setRecirculatorPower(false);
    // Melody de éxito
    tone(BUZZER_PIN, 523, 200); // C5
    tone(BUZZER_PIN, 659, 200); // E5
    tone(BUZZER_PIN, 784, 400); // G5
    return;
  }
  
  // CONDICIÓN 2: Timeout (2 minutos)
  if (elapsed >= 120000) {
    setRecirculatorPower(false);
    // Buzzer de timeout (2 beeps)
    tone(BUZZER_PIN, 2000, 1000); delay(1100);
    tone(BUZZER_PIN, 2000, 1000);
    return;
  }
}
```

### Estados del Sistema

```
┌─────────────┐  Botón IZQ / MQTT    ┌─────────────┐
│   APAGADO   │ ──────────────────> │  ENCENDIDO  │
│  LED: ROJO  │                      │ LED: VERDE  │
│ Relé: LOW   │ <────────────────── │ Relé: HIGH  │
└─────────────┘  Temp ≥ Max          └─────────────┘
                 Timeout 2min
                 Botón IZQ / MQTT
```

### Pantalla UI

```
┌────────────────────────────────────┐
│ RECIRCULATOR        2.15V  [RECIR] │
├────────────────────────────────────┤
│  Estado: 🟢 ENCENDIDO               │
│                                     │
│  Temp: 28.5°C                      │
│  Max:  30.0°C                      │
│                                     │
│  Tiempo: 00:45 / 02:00             │
│                                     │
│  [IZQ] ON/OFF                      │
│  [DER] Cambiar modo                │
└────────────────────────────────────┘
```

### Indicadores LED NeoPixel

| Color | Estado | Significado |
|-------|--------|-------------|
| 🔴 ROJO | Apagado | Bomba inactiva |
| 🟢 VERDE | Encendido | Bomba funcionando |

### Feedback Sonoro

| Evento | Patrón | Notas |
|--------|--------|-------|
| Encendido | 1 beep corto | 1000Hz, 100ms |
| Apagado manual | 1 beep grave | 500Hz, 200ms |
| Temp alcanzada | Melody (3 notas) | C5-E5-G5 |
| Timeout | 2 beeps largos | 2000Hz, 1000ms × 2 |

### Librerías Requeridas

```cpp
#include <OneWire.h>           // Protocolo 1-Wire para DS18B20
#include <DallasTemperature.h>  // Driver DS18B20
#include <Adafruit_NeoPixel.h>  // Control WS2812B LED
```

**Configuración:**
```cpp
OneWire oneWireRecirculator(TEMP_SENSOR_PIN);
DallasTemperature sensorTemp(&oneWireRecirculator);
Adafruit_NeoPixel pixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
```

---

## 🔋 Power Management

### Sleep Automático
```
Usuario presiona botón → updateUserActivity() → Reset timer
       ↓
   5 minutos sin actividad de botones
       ↓
   enterSleepMode() → Deep Sleep
       ↓
   Presionar cualquier botón → Wakeup → setup()
```

**Nota:** Solo actividad de BOTONES activa el timer (sensores no cuentan).

---

## 🎨 UI/UX Patterns

### Colores por Modo
```cpp
MODE_READ:         TFT_GREEN   (pulsos), TFT_CYAN (gráfico)
MODE_WRITE:        TFT_RED     (generador)
MODE_PRESSURE:     TFT_MAGENTA (presión)
MODE_WIFI_SCAN:    TFT_CYAN    (WiFi)
MODE_RECIRCULATOR: TFT_ORANGE  (recirculador)
```

### Redibujado Optimizado
```cpp
static float last_value = -999;
if (abs(current_value - last_value) > threshold) {
  // Solo redibujar si cambió significativamente
  tft.fillRect(...);  // Borrar área
  tft.print(...);     // Dibujar nuevo valor
  last_value = current_value;
}
```

**Oportunidad de Mejora:** MEJORA-003 (renderizado incremental de gráficos).

---

## 🚀 Mejoras Pendientes

Ver `TODO.md` para lista completa. Arquitecturalmente importantes:

- **MEJORA-017**: Extraer funciones de manejo de modos
  - Crear `manejarModoRead()`, `manejarModoWrite()`, etc.
  - Reducir `loop()` de ~120 líneas a ~20 líneas

- **MEJORA-018**: Función `cambiarModo()` dedicada
  - Centralizar lógica de transición entre modos
  - Facilitar añadir nuevos modos

- **MEJORA-003**: Renderizado incremental de gráficos
  - Cambiar de O(n) a O(1) dibujando solo segmento nuevo
  - Mejora crítica para MODE_PRESSURE @ 100Hz

---

## 📚 Referencias

- **Hardware:** `HARDWARE.md`
- **Pulsos:** `docs/pulse_implementation_guide.md`
- **Tareas:** `TODO.md`
- **Contexto Copilot:** `.copilot-instructions.md`
