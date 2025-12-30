# Estructura del Código - Test Pulsos Contador

## Reorganización Modular

El código ha sido reorganizado en módulos separados para mejorar la mantenibilidad y la claridad. Cada modo de operación tiene su propio par de archivos (header + implementación).

## Estructura de Archivos

### Archivos Comunes

#### `include/config.h`
Contiene todas las definiciones y constantes del proyecto:
- Pines GPIO
- Configuración I2C
- Configuración del recirculador
- Notas musicales para el buzzer
- Constantes de timing
- Constantes del gráfico
- Períodos y frecuencias

#### `include/common.h` / `src/common.cpp`
Variables y funciones compartidas entre módulos:
- Enumeraciones (SystemMode, TestCase, PhaseType)
- Estructuras de datos (PulsePhase, PulsePattern, WiFiNetwork, WNK1MA_Reading)
- Objetos globales (TFT, sensores)
- Variables del sistema
- Melodías Mario Bros
- Funciones comunes (updateUserActivity, enterSleepMode, leerVoltaje)

#### `include/display.h` / `src/display.cpp`
Funciones de visualización compartidas:
- mostrarVoltaje()
- mostrarModo()
- dibujarMarcoGrafico()
- dibujarLineasReferencia()
- inicializarGrafico()
- actualizarGrafico()
- actualizarGraficoGenerico()
- playTone() / stopTone()
- getSignalColor()

### Módulos de Modos

#### `include/mode_read.h` / `src/mode_read.cpp`
**Modo de lectura de pulsos (READ)**
- Variables: pulse_count, pulse_frequency, etc.
- Funciones:
  - pulseInterrupt() - Interrupción para contar pulsos
  - inicializarModoRead()
  - manejarModoRead()
  - mostrarInfoSensorRead()

#### `include/mode_write.h` / `src/mode_write.cpp`
**Modo de generación de pulsos (WRITE)**
- Variables: generating_pulse, pulse_pattern, test cases, etc.
- Arrays de fases para cada test case (test1_phases, test2_phases, etc.)
- Funciones:
  - inicializarGenerador()
  - preGenerarPatron()
  - generarPulsos()
  - manejarModoWrite()
  - manejarBotonIzquierdoWrite()
  - Funciones auxiliares para cálculo de períodos y jitter

#### `include/mode_pressure.h` / `src/mode_pressure.cpp`
**Modo de lectura de presión I2C (PRESSURE)**
- Variables: pressure_graph_data, escalas, históricos, etc.
- Funciones:
  - readWNK1MA() - Lectura del sensor I2C
  - inicializarModoPressure()
  - manejarModoPressure()
  - actualizarHistoricoPresion()
  - actualizarGraficoPresion()
  - mostrarInfoSensorPressure()

#### `include/mode_recirculator.h` / `src/mode_recirculator.cpp`
**Modo recirculador (RECIRCULATOR)**
- Variables: recirculator_power_state, recirculator_temp, etc.
- Funciones:
  - inicializarRecirculador()
  - setRecirculatorPower()
  - leerTemperaturaRecirculador()
  - controlarRecirculadorAutomatico()
  - mostrarPantallaRecirculador()
  - manejarModoRecirculador()
  - manejarBotonIzquierdoRecirculator()

#### `include/mode_wifi.h` / `src/mode_wifi.cpp`
**Modo escáner WiFi (WIFI_SCAN)**
- Variables: wifi_networks[], wifi_count, wifi_page, etc.
- Funciones:
  - escanearWiFi()
  - mostrarListaWiFi()
  - mostrarPantallaScanningWiFi()
  - manejarModoWiFi()
  - manejarBotonIzquierdoWiFi()

### Archivo Principal

#### `src/main.cpp`
Coordinador principal del sistema:
- setup() - Inicialización de hardware y módulos
- loop() - Bucle principal
- cambiarModo() - Gestión de transiciones entre modos
- manejarBotonIzquierdo() - Delegación de acciones del botón izquierdo
- manejarBotonDerecho() - Cambio de modo
- mostrarInfoSensor() - Delegación de visualización de info

## Ventajas de Esta Estructura

1. **Separación de responsabilidades**: Cada módulo tiene una función clara y específica
2. **Mantenibilidad**: Es más fácil encontrar y modificar código relacionado con un modo específico
3. **Reutilización**: Las funciones comunes están centralizadas en common.cpp y display.cpp
4. **Escalabilidad**: Agregar nuevos modos es tan simple como crear un nuevo par mode_xxx.h/cpp
5. **Código más limpio**: El main.cpp es mucho más pequeño y legible (aproximadamente 200 líneas vs 2000)
6. **Compilación modular**: Los cambios en un módulo solo requieren recompilar ese módulo

## Código Compartido

### Entre READ y WRITE
Ambos comparten:
- Funciones de gráfico (actualizarGrafico)
- Configuración del pin SENSOR_PIN
- Variables de gráfico (graph_data, graph_index)

### Entre todos los modos
- Gestión de voltaje (leerVoltaje, mostrarVoltaje)
- Gestión de modos (mostrarModo)
- Sistema de sleep (updateUserActivity, enterSleepMode)
- Display TFT (tft object)

## Flujo de Compilación

Cuando se compila el proyecto, PlatformIO:
1. Compila cada archivo .cpp en la carpeta `src/` de forma independiente
2. Enlaza todos los objetos compilados junto con las librerías
3. Genera el firmware final

Los archivos .h en `include/` son incluidos según sea necesario en cada .cpp.

## Notas de Implementación

- Las variables globales se definen en los archivos .cpp y se declaran como `extern` en los .h
- Las interrupciones (IRAM_ATTR) están correctamente ubicadas en sus respectivos módulos
- El sistema mantiene la misma funcionalidad que antes, pero ahora es mucho más organizado
