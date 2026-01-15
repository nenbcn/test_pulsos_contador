# Cambio Instantáneo entre Test Cases

## Problema Original

Cuando se generaban pulsos de test, el usuario tenía que esperar a que el patrón completo se generara antes de poder cambiar al siguiente test case. Esto era especialmente molesto en test cases largos como:
- **TC4 (Stress Test)**: ~15 segundos, 543 pulsos
- **TC5 (Single Pulse)**: ~13 segundos

## Solución Implementada

Se ha modificado el código para permitir **interrumpir la generación de pulsos inmediatamente** al presionar el botón izquierdo.

### Cambios Realizados

#### 1. Detección inmediata en `generarPulsos()` 
[src/mode_write.cpp](../src/mode_write.cpp#L220-L229)

```cpp
void generarPulsos() {
  // Verificar si se debe detener la generación (puede ser por cambio de test case)
  if (!pattern_ready || !generating_pulse) {
    if (pulse_state) {
      digitalWrite(SENSOR_PIN, LOW);
      pulse_state = false;
    }
    return;
  }
  // ... resto del código
}
```

**Beneficio**: Si `generating_pulse` se pone en `false`, la función detiene inmediatamente la generación y asegura que el pin del sensor quede en LOW.

#### 2. Reset completo en `manejarBotonIzquierdoWrite()`
[src/mode_write.cpp](../src/mode_write.cpp#L296-L331)

```cpp
void manejarBotonIzquierdoWrite() {
  // DETENER generación INMEDIATAMENTE
  generating_pulse = false;
  next_pulse_time = 0;
  next_pulse_time_us = 0;
  last_pulse_ts_us = 0;
  pulse_end_time_us = 0;
  pulse_state = false;
  pattern_ready = false;
  current_pulse_index = 0;
  digitalWrite(SENSOR_PIN, LOW);  // Asegurar que el pin esté LOW
  
  // Cambiar al siguiente test case
  current_test = (TestCase)((current_test + 1) % 5);
  // ...
}
```

**Beneficio**: 
- Detiene inmediatamente la generación poniendo `generating_pulse = false`
- Resetea todos los contadores y timers
- Asegura que el pin sensor esté en LOW
- Muestra un mensaje en el Serial indicando la interrupción

## Flujo de Funcionamiento

1. **Usuario presiona botón izquierdo** durante la generación de pulsos
2. **Loop principal** detecta el botón (con debounce de 200ms)
3. **`manejarBotonIzquierdoWrite()`** se ejecuta:
   - Pone `generating_pulse = false`
   - Resetea todos los estados
   - Cambia al siguiente test case
   - Pre-genera el nuevo patrón
   - Actualiza la pantalla
4. **Próxima llamada a `generarPulsos()`**:
   - Detecta `generating_pulse = false`
   - Asegura que el pin esté en LOW
   - Retorna inmediatamente

## Uso

En modo **WRITE**:
1. El sistema empieza generando **TC1** automáticamente
2. Presiona **botón izquierdo** en cualquier momento para:
   - **Detener** la generación actual
   - **Cambiar** al siguiente test case
3. Orden cíclico: TC1 → TC2 → TC3 → TC4 → TC5 → TC1 → ...

## Mensajes Serial

Cuando se interrumpe un patrón:
```
*** PATRÓN INTERRUMPIDO - Cambiando a: TC2: Normal ***
✓ Patrón generado: 85 pulsos, 6.3s, 240 puntos gráfico
Listo para generar nuevo patrón. El patrón anterior se detuvo.
```

## Mejoras Futuras (Opcional)

- Añadir un contador visual en pantalla que muestre cuántos pulsos se han generado del total
- Permitir pausar/reanudar la generación sin cambiar de test case
- Agregar un botón de "reset" que reinicie el test case actual desde cero
