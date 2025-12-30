# Guía de Uso del Código Modular

## 🎯 ¿Cómo Trabajar con Esta Estructura?

### Para Modificar un Modo Específico

#### Ejemplo: Cambiar el Modo de Lectura (READ)

1. Abre `src/mode_read.cpp` o `include/mode_read.h`
2. Modifica la lógica que necesites
3. Compila el proyecto (solo se recompilará lo modificado)

```bash
~/.platformio/penv/bin/platformio run
```

### Para Agregar un Nuevo Modo

#### Ejemplo: Crear MODE_TEMPERATURE

1. **Crear el header** `include/mode_temperature.h`:
```cpp
#ifndef MODE_TEMPERATURE_H
#define MODE_TEMPERATURE_H

#include "common.h"

// Variables específicas
extern float current_temperature;

// Funciones
void inicializarModoTemperature();
void manejarModoTemperature();
void mostrarInfoSensorTemperature();

#endif
```

2. **Crear la implementación** `src/mode_temperature.cpp`:
```cpp
#include "mode_temperature.h"
#include "display.h"

float current_temperature = 0.0;

void inicializarModoTemperature() {
  Serial.println("Modo TEMPERATURE inicializado");
  // Tu código de inicialización
}

void manejarModoTemperature() {
  // Tu lógica del modo
}

void mostrarInfoSensorTemperature() {
  // Tu código de visualización
}
```

3. **Actualizar common.h** para agregar el modo:
```cpp
enum SystemMode {
  MODE_READ,
  MODE_WRITE,
  MODE_PRESSURE,
  MODE_RECIRCULATOR,
  MODE_WIFI_SCAN,
  MODE_TEMPERATURE  // ← Nuevo
};
```

4. **Actualizar main.cpp**:
```cpp
#include "mode_temperature.h"

// En cambiarModo():
case MODE_TEMPERATURE:
  inicializarModoTemperature();
  // Configuración de pantalla, etc.
  break;

// En loop():
case MODE_TEMPERATURE:
  manejarModoTemperature();
  break;
```

### Para Modificar Constantes o Pines

Edita `include/config.h`:
```cpp
#define MI_NUEVO_PIN 25
#define MI_CONSTANTE 1000
```

### Para Agregar Funciones Compartidas

#### Opción 1: Función de Visualización
Añade a `include/display.h` y `src/display.cpp`

#### Opción 2: Función de Sistema
Añade a `include/common.h` y `src/common.cpp`

## 📝 Reglas de Estilo

### Nombres de Archivos
- Headers: `mode_nombre.h`
- Implementación: `mode_nombre.cpp`
- Funciones del modo: `manejarModoNombre()`, `inicializarModoNombre()`

### Variables
- Variables globales del modo: Definir en `.cpp`, declarar como `extern` en `.h`
- Variables locales: Declarar dentro de funciones
- Constantes: En `config.h`

### Includes
```cpp
// En archivos .h
#ifndef NOMBRE_H
#define NOMBRE_H

#include "common.h"  // Siempre primero

// Declaraciones...

#endif

// En archivos .cpp
#include "nombre.h"
#include "display.h"  // Si es necesario
```

## 🔧 Compilación y Debug

### Compilar solo (sin subir)
```bash
~/.platformio/penv/bin/platformio run
```

### Compilar y subir
```bash
~/.platformio/penv/bin/platformio run --target upload
```

### Monitor serial
```bash
~/.platformio/penv/bin/platformio device monitor
```

### Limpiar y recompilar todo
```bash
~/.platformio/penv/bin/platformio run --target clean
~/.platformio/penv/bin/platformio run
```

## 🐛 Solución de Problemas Comunes

### Error: "multiple definition of..."
- **Causa**: Variable definida en .h en lugar de declarada como `extern`
- **Solución**: Mover definición a .cpp, dejar solo `extern` en .h

### Error: "undefined reference to..."
- **Causa**: Función declarada pero no implementada
- **Solución**: Implementar la función en el .cpp correspondiente

### Warning: "unused variable..."
- **Causa**: Variable declarada pero no usada
- **Solución**: Eliminar la variable o usarla

### No compila después de agregar archivo
- **Causa**: PlatformIO no detectó el nuevo archivo
- **Solución**: Hacer clean y recompilar:
```bash
~/.platformio/penv/bin/platformio run --target clean
~/.platformio/penv/bin/platformio run
```

## 📊 Estructura Recomendada para Nuevos Módulos

```cpp
// ========================================
// include/mode_nuevo.h
// ========================================
#ifndef MODE_NUEVO_H
#define MODE_NUEVO_H

#include "common.h"

// Variables específicas
extern tipo_dato variable_modo;

// Funciones públicas
void inicializarModoNuevo();
void manejarModoNuevo();
void manejarBotonIzquierdoNuevo();  // Opcional
void mostrarInfoNuevo();  // Opcional

#endif

// ========================================
// src/mode_nuevo.cpp
// ========================================
#include "mode_nuevo.h"
#include "display.h"

// Variables
tipo_dato variable_modo = valor_inicial;

// Variables privadas (static)
static unsigned long ultima_actualizacion = 0;

void inicializarModoNuevo() {
  Serial.println("Modo NUEVO inicializado");
  // Configurar pines
  // Inicializar variables
}

void manejarModoNuevo() {
  unsigned long current_time = millis();
  
  // Lógica principal del modo
  if (current_time - ultima_actualizacion >= INTERVALO) {
    // Hacer algo
    ultima_actualizacion = current_time;
  }
}

void manejarBotonIzquierdoNuevo() {
  // Acción del botón izquierdo en este modo
}

void mostrarInfoNuevo() {
  // Actualizar información en pantalla
  static char last_text[50] = "";
  char text[50];
  
  snprintf(text, sizeof(text), "Info: %d", valor);
  if (strcmp(text, last_text) != 0) {
    tft.fillRect(x, y, w, h, TFT_BLACK);
    tft.setTextColor(TFT_COLOR);
    tft.drawString(text, x, y);
    strcpy(last_text, text);
  }
}
```

## 🎓 Buenas Prácticas

1. **Un modo = Un par de archivos** (.h + .cpp)
2. **Variables globales**: Solo las necesarias, declarar como `extern`
3. **Funciones static**: Para funciones privadas del módulo
4. **Optimizar updates**: Usar `static` para comparar y actualizar solo si cambió
5. **Documentar**: Comentar código complejo
6. **Debug serial**: Usar niveles de debug (cada segundo, cada 5 segundos, etc.)
7. **Constantes**: Siempre en `config.h`, nunca hardcoded
8. **Nombres descriptivos**: `manejarModoRead()` mejor que `handle()`

## 📚 Recursos Adicionales

- [ESTRUCTURA_CODIGO.md](ESTRUCTURA_CODIGO.md) - Documentación detallada
- [DIAGRAMA_ARQUITECTURA.md](DIAGRAMA_ARQUITECTURA.md) - Diagramas visuales
- [RESUMEN_REESTRUCTURACION.md](RESUMEN_REESTRUCTURACION.md) - Cambios realizados

## ✅ Checklist para Nuevas Características

- [ ] Identificar en qué módulo va la funcionalidad
- [ ] Si es nuevo modo: crear mode_xxx.h y mode_xxx.cpp
- [ ] Actualizar common.h si se agrega modo nuevo
- [ ] Implementar funciones requeridas (inicializar, manejar)
- [ ] Actualizar main.cpp para integrar el nuevo modo
- [ ] Compilar y verificar sin errores
- [ ] Probar en hardware
- [ ] Documentar cambios en comentarios
