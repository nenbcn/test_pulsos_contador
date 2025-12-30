# Diagrama de Arquitectura del Proyecto

```
┌─────────────────────────────────────────────────────────────────────┐
│                            src/main.cpp                              │
│  • setup() - Inicialización                                         │
│  • loop() - Bucle principal                                         │
│  • cambiarModo() - Gestión de transiciones                          │
│  • Control de botones                                               │
└─────────────────────────────────────────────────────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            │                       │                       │
            ▼                       ▼                       ▼
┌──────────────────────┐  ┌──────────────────┐  ┌──────────────────────┐
│   include/config.h   │  │ include/common.h │  │  include/display.h   │
│                      │  │                  │  │                      │
│ • Pines GPIO         │  │ • Enumeraciones  │  │ • mostrarVoltaje()   │
│ • Constantes I2C     │  │ • Estructuras    │  │ • mostrarModo()      │
│ • Notas musicales    │  │ • Variables      │  │ • inicializarGrafico │
│ • Timing             │  │   globales       │  │ • actualizarGrafico  │
│ • Gráfico            │  │ • Objetos HW     │  │ • playTone()         │
│ • Períodos           │  │                  │  │                      │
└──────────────────────┘  └──────────────────┘  └──────────────────────┘
                                    │                       │
                                    ▼                       ▼
                          ┌──────────────────┐  ┌──────────────────────┐
                          │  src/common.cpp  │  │  src/display.cpp     │
                          │                  │  │                      │
                          │ • Implementación │  │ • Implementación de  │
                          │   de variables   │  │   funciones de       │
                          │ • Melodías       │  │   visualización      │
                          │ • leerVoltaje()  │  │ • Funciones de       │
                          │ • enterSleep()   │  │   gráfico            │
                          └──────────────────┘  └──────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                          MÓDULOS DE MODOS                            │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────┐  ┌─────────────────────┐  ┌────────────────────┐
│ include/mode_read.h │  │include/mode_write.h │  │include/            │
│ src/mode_read.cpp   │  │src/mode_write.cpp   │  │  mode_pressure.h   │
│                     │  │                     │  │src/mode_pressure.  │
│ MODO LECTURA        │  │ MODO ESCRITURA      │  │  cpp               │
│                     │  │                     │  │                    │
│ • pulseInterrupt()  │  │ • test cases        │  │ MODO PRESIÓN       │
│ • pulse_count       │  │ • preGenerarPatron  │  │                    │
│ • pulse_frequency   │  │ • generarPulsos()   │  │ • readWNK1MA()     │
│ • manejarModoRead() │  │ • manejarModoWrite  │  │ • Escala histórica │
│ • mostrarInfoSensor │  │ • Fases de test     │  │ • manejarModo      │
│   Read()            │  │                     │  │   Pressure()       │
└─────────────────────┘  └─────────────────────┘  └────────────────────┘

┌─────────────────────┐  ┌─────────────────────┐
│include/             │  │ include/mode_wifi.h │
│  mode_recirculator.h│  │ src/mode_wifi.cpp   │
│src/mode_            │  │                     │
│  recirculator.cpp   │  │ MODO WIFI SCANNER   │
│                     │  │                     │
│ MODO RECIRCULADOR   │  │ • escanearWiFi()    │
│                     │  │ • wifi_networks[]   │
│ • DS18B20           │  │ • mostrarListaWiFi  │
│ • Relé control      │  │ • wifi_page         │
│ • NeoPixel LED      │  │ • manejarModoWiFi() │
│ • Buzzer            │  │                     │
│ • Control temp      │  │                     │
│ • Melodías Mario    │  │                     │
└─────────────────────┘  └─────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                         FLUJO DE DATOS                               │
└─────────────────────────────────────────────────────────────────────┘

Usuario presiona botón
        │
        ▼
main.cpp detecta botón
        │
        ├─→ Botón DERECHO → cambiarModo() → Inicializa nuevo modo
        │
        └─→ Botón IZQUIERDO → Delega a modo activo:
                │
                ├─→ MODE_WRITE → manejarBotonIzquierdoWrite()
                ├─→ MODE_RECIRCULATOR → manejarBotonIzquierdoRecirculator()
                └─→ MODE_WIFI → manejarBotonIzquierdoWiFi()

loop() principal
        │
        ├─→ Actualiza voltaje (cada 500ms)
        ├─→ Verifica botones (con debounce)
        ├─→ Verifica timeout sleep (5 min)
        │
        └─→ Ejecuta lógica del modo actual:
                │
                ├─→ MODE_READ → manejarModoRead()
                ├─→ MODE_WRITE → manejarModoWrite()
                ├─→ MODE_PRESSURE → manejarModoPressure()
                ├─→ MODE_RECIRCULATOR → manejarModoRecirculador()
                └─→ MODE_WIFI_SCAN → manejarModoWiFi()

┌─────────────────────────────────────────────────────────────────────┐
│                    CÓDIGO COMPARTIDO                                 │
└─────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  Gráfico (display.cpp)                                       │
│  ┌────────────────┐                                          │
│  │ graph_data[]   │ ← READ y WRITE usan el mismo array      │
│  └────────────────┘                                          │
│  ┌────────────────┐                                          │
│  │ pressure_      │ ← PRESSURE usa array separado           │
│  │ graph_data[]   │                                          │
│  └────────────────┘                                          │
│                                                              │
│  actualizarGraficoGenerico() ← Función reutilizable para    │
│                                ambos tipos de gráfico        │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  Gestión de Sistema (common.cpp)                            │
│  • updateUserActivity() → Todos los modos                   │
│  • enterSleepMode() → Sistema principal                     │
│  • leerVoltaje() → Loop principal                           │
│  • Objetos TFT, sensores → Compartidos por todos            │
└──────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    DEPENDENCIAS                                      │
└─────────────────────────────────────────────────────────────────────┘

main.cpp
  ├─ include config.h
  ├─ include common.h (depende de config.h)
  ├─ include display.h (depende de common.h)
  └─ include mode_*.h (cada uno depende de common.h)

Cada mode_*.cpp
  ├─ include mode_*.h
  └─ include display.h

display.cpp
  ├─ include display.h
  └─ include mode_read.h, mode_pressure.h (para acceder a variables)

common.cpp
  └─ include common.h
```

## Resumen de Interacciones

1. **main.cpp** es el coordinador central
2. **Módulos de modo** son independientes entre sí
3. **common.cpp/h** provee infraestructura compartida
4. **display.cpp/h** provee funciones de visualización
5. **config.h** centraliza todas las configuraciones

Esta arquitectura permite:
- Modificar un modo sin afectar otros
- Agregar nuevos modos fácilmente
- Reutilizar código común eficientemente
- Mantener el código organizado y legible
