# 🎛️ ESP32 TTGO T-Display - Sistema Multi-Modo

Sistema de monitoreo y generación de pulsos para ESP32 TTGO T-Display con 4 modos de operación, gráficos en tiempo real, sensor I2C de presión y escaneo WiFi.

## ✨ Características

- **MODE_READ**: Contador de pulsos con gráfico de frecuencia (0-75Hz)
- **MODE_WRITE**: Generador de pulsos con patrón sofisticado de 29 segundos
- **MODE_PRESSURE**: Lectura de sensor I2C WNK1MA a 100Hz con auto-escalado
- **MODE_WIFI_SCAN**: Escaneo de redes WiFi con paginación y análisis RSSI
- **Sleep automático**: Deep sleep tras 5 minutos de inactividad
- **Gestión de energía**: Monitoreo de voltaje de batería

## 🚀 Quick Start

### Requisitos
- **Hardware**: TTGO T-Display ESP32 (240x135 TFT)
- **IDE**: VS Code con extensión PlatformIO
- **Framework**: Arduino for ESP32

### Compilar y Subir

```bash
# Usar tarea de PlatformIO (recomendado)
# En VS Code: Terminal > Run Task > "Build Project"
# O directamente:
~/.platformio/penv/bin/platformio run

# Para subir al ESP32:
# Terminal > Run Task > "PlatformIO Upload"
~/.platformio/penv/bin/platformio run --target upload
```

**⚠️ Importante**: PlatformIO está en un entorno virtual. NO usar `pio run` o `platformio run` directamente (no están en PATH).

## 🎮 Controles

- **Botón Derecho (GPIO35)**: Cambiar modo (READ → WRITE → PRESSURE → WIFI → ...)
- **Botón Izquierdo (GPIO0)**: Acción del modo actual (ej: cambiar página WiFi)
- **Presionar cualquier botón**: Despertar del sleep mode

## 📂 Estructura del Proyecto

```
test_pulsos_contador/
├── src/
│   └── main.cpp                          # Código principal (1286 líneas)
├── docs/
│   └── pulse_implementation_guide.md     # Código de referencia para pulsos
├── platformio.ini                        # Configuración PlatformIO
│
├── README.md                             # Introducción y Quick Start
├── ARCHITECTURE.md                       # Arquitectura del código (FSM, flujos)
├── HARDWARE.md                           # Specs hardware (pinout, rangos)
├── TODO.md                               # Mejoras pendientes + workflow
└── .copilot-instructions.md              # Patrones generales (reutilizable)
```

## 📊 Modos de Operación

### 1. MODE_READ (Lectura)
- Cuenta pulsos externos en GPIO21
- Calcula frecuencia cada 200ms
- Gráfico en tiempo real (escala 0-75Hz)
- Ideal para validar generadores externos

### 2. MODE_WRITE (Generación)
Genera un patrón sofisticado de pulsos de 29 segundos:
- **BURST1/2** (3s c/u): Gradientes suaves 30Hz→50Hz→30Hz
- **STRESS_BURST** (10s): Test de carga con frecuencias variables 15Hz-100Hz
- **PAUSAS**: 3s, 3s, 7s entre fases

Ver especificaciones completas: [`docs/pulse_implementation_guide.md`](docs/pulse_implementation_guide.md)

### 3. MODE_PRESSURE (Sensor I2C)
- Lee sensor WNK1MA a 100Hz (cada 10ms)
- Gráfico con auto-escalado dinámico
- Comunicación I2C a 100kHz (estabilidad)

### 4. MODE_WIFI_SCAN (WiFi)
- Escanea redes WiFi cada 10 segundos
- Muestra 5 redes por página (3 páginas)
- Colores según RSSI: Verde (excelente) → Rojo (débil)

## 🔧 Configuración de Hardware

### Pinout
```
GPIO0  → Botón Izquierdo (INPUT_PULLUP)
GPIO35 → Botón Derecho (INPUT_PULLUP)
GPIO21 → Sensor/Generador de Pulsos (INPUT/OUTPUT)
GPIO32 → I2C SDA (sensor presión WNK1MA)
GPIO22 → I2C SCL (sensor presión WNK1MA)
GPIO36 → ADC lectura voltaje batería
GPIO4  → Control retroiluminación TFT
```

Ver detalles completos en [`HARDWARE.md`](HARDWARE.md).

## 🏗️ Arquitectura

- **Patrón**: Single-threaded, event-driven, non-blocking
- **Framework**: Arduino (NO usa FreeRTOS)
- **FSM**: 4 estados con transiciones circulares
- **Memoria**: Buffers circulares para gráficos (200 puntos)
- **Sleep**: Deep sleep con wake-up por interrupciones

Ver arquitectura completa en [`ARCHITECTURE.md`](ARCHITECTURE.md).

## 📝 Desarrollo

### Workflow Recomendado
1. Consultar [`TODO.md`](TODO.md) para mejoras pendientes
2. Seguir instrucciones de workflow (commits, testing)
3. Compilar y probar en hardware tras cada cambio
4. Documentar cambios según formato establecido

### Mejoras Pendientes (destacadas)
- **MEJORA-017**: Extraer funciones de manejo de modos (simplificar `loop()`)
- **MEJORA-018**: Función `cambiarModo()` dedicada
- **MEJORA-003**: Renderizado incremental de gráficos (mejora 10x en performance)

Ver lista completa en [`TODO.md`](TODO.md).

## 📚 Documentación

| Archivo | Propósito |
|---------|-----------|
| `README.md` | Introducción y quick start (este archivo) |
| `ARCHITECTURE.md` | Arquitectura del código, FSM, flujos |
| `HARDWARE.md` | Especificaciones técnicas de hardware |
| `TODO.md` | Gestión de mejoras pendientes y completadas |
| `.copilot-instructions.md` | Patrones generales y workflow (reutilizable) |
| `docs/pulse_implementation_guide.md` | Implementación detallada del generador de pulsos |

### 📖 Guía de Uso de la Documentación

**Para empezar con el proyecto:**
1. Lee `README.md` (este archivo) - Quick start y características
2. Consulta `HARDWARE.md` - Conexiones y especificaciones
3. Revisa `ARCHITECTURE.md` - Cómo funciona el código

**Para desarrollar:**
1. Consulta `.copilot-instructions.md` - Patrones de código y workflow
2. Revisa `TODO.md` - Mejoras pendientes y formato de commits
3. Consulta `docs/` - Implementaciones específicas de features

## 🔋 Power Management

- **Sleep automático**: Tras 5 minutos sin presionar botones
- **Wake-up**: Presionar cualquier botón (GPIO0 o GPIO35)
- **Consumo**: ~14.5% RAM (47KB), ~62.8% Flash (822KB)

## 📊 Métricas de Compilación

```
RAM:   14.5% (47,436 bytes de 327,680 bytes)
Flash: 62.8% (822,969 bytes de 1,310,720 bytes)
```

## 🤝 Contribuir

Este proyecto sigue un flujo de trabajo estructurado:
1. Revisar [`TODO.md`](TODO.md) para tareas pendientes
2. Implementar cambios siguiendo patrones establecidos
3. Probar en hardware real
4. Commits siguiendo formato `[MEJORA-XXX] Descripción`

## 📄 Licencia

MIT License