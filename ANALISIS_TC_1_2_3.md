# Análisis de Test Cases 1, 2 y 3

## 📊 Resumen del Log

Analizando el log del gateway, identifico las siguientes secuencias:

---

## ✅ TEST_CASE_1: Startup Simple (seq 778-788)

### Comportamiento Observado
```
Seq: 778-788 (11 pulsos)
Tipo: TRANSITION
Período promedio: 58ms
Períodos individuales: 66→56→48→42→42→48→54→66→76→86ms
Fin: timeout (2040ms inactividad)
```

### Análisis
✅ **CORRECTO**: 
- Detectó **11 pulsos** en fase de arranque/transición
- Período promedio de **58ms** indica arranque gradual
- Los períodos decrecientes (66→42ms) muestran aceleración inicial
- Los períodos crecientes (42→86ms) muestran desaceleración
- **Timeout correcto** de ~2s al finalizar

❌ **OBSERVACIONES**:
- Esperábamos ~50 pulsos según el test case definido
- Solo registró 11 pulsos porque la secuencia fue interrumpida temprano
- **Posible causa**: El test case no se ejecutó completamente o fue detenido manualmente

**Veredicto**: Comportamiento del gateway CORRECTO, pero secuencia incompleta.

---

## ✅ TEST_CASE_2: Fragmentación TRANSITION (seq 789-808, 809-816, 817-837...)

### Primera Transición (seq 789-808)
```
Pulsos: 20
Tipo: TRANSITION → STABLE
Período promedio: 55ms
Períodos: 78→74→68→64→64→60→86→26(!)→52→48→48→48→48→46→48→48→46→50→48ms
Estado final: STABLE a 48ms
```

### Primera Fase STABLE (seq 809-816)
```
Pulsos: 8
Tipo: STABLE
Período promedio: 48ms
Períodos: 46→50→48→46→50→48→48→58ms
Cambio abrupto: 46ms → 58ms (+26.1%) → vuelve a TRANSITION
```

### Segunda Transición (seq 817-837)
```
Pulsos: 21
Tipo: TRANSITION → STABLE
Período promedio: 48ms
Estado final: STABLE a 48ms
```

### Segunda Fase STABLE (seq 838-846)
```
Pulsos: 9
Tipo: STABLE
Período promedio: 48ms
Cambio abrupto: 47ms → 68ms (+44.7%) → vuelve a TRANSITION
```

### Análisis TC2
✅ **CORRECTO**:
- El gateway **detectó correctamente múltiples escalones** de frecuencia
- Las transiciones TRANSITION→STABLE funcionan bien
- Los cambios abruptos son detectados correctamente (26.1%, 44.7%)
- La estabilización ocurre después de ~20 pulsos

❌ **PROBLEMAS DETECTADOS**:

1. **Fragmentación excesiva de eventos STABLE**:
   - Fragmento 1: solo **8 pulsos** (muy corto)
   - Fragmento 2: solo **9 pulsos** (muy corto)
   - **Causa**: Umbral de cambio abrupto (20%) muy sensible

2. **Períodos anómalos**:
   - Período de **26ms** en seq 789 (pulse count=9)
   - Período de **18ms** en seq 817 (pulse count=11)
   - Período de **10ms** en seq 919 (pulse count=6)
   - **Causa**: Rebotes o ruido en el sensor (ver WARNING más adelante)

3. **WARNING crítico**:
   ```
   [WARNING]: Critical event: type=11, context=14 pulses filtered in 60s
              - add RC circuit to sensor
   ```
   - El sistema filtró **14 pulsos** por debouncing en 60 segundos
   - **Recomendación**: añadir circuito RC al sensor físico

**Veredicto**: El gateway funciona correctamente, pero:
- **Fragmentación excesiva** → Considerar aumentar umbral de cambio abrupto de 20% a 30-35%
- **Ruido en sensor** → Añadir filtro RC hardware (el gateway ya filtra en software)

---

## ✅ TEST_CASE_3: Parada Gradual (seq 864-875 y posteriores)

### Transición final a IDLE (seq 864-875)
```
Pulsos: 12
Tipo: TRANSITION
Período promedio: 77ms
Períodos: 76→50→70→72→72→74→82→84→86→94→96ms
Fin: timeout (2041ms inactividad)
```

### Siguiente arranque (seq 876-894)
```
Pulsos: 19
Tipo: TRANSITION → STABLE
Período promedio: 56ms
Períodos: 70→68→64→62→62→58→56→58→50→52→54→52→52→50→54→52→50→54ms
Estado final: STABLE a 52ms
```

### Análisis TC3
✅ **CORRECTO**:
- Detectó **desaceleración progresiva** correctamente (períodos crecientes: 76→96ms)
- **Timeout correcto** de ~2s al finalizar
- El nuevo arranque muestra aceleración gradual (70→50ms)

❌ **OBSERVACIÓN**:
- Esperábamos ~60 pulsos en todo el TC3
- Solo registró 12 pulsos en la fase de desaceleración
- **Posible causa**: Secuencia incompleta o test case más corto de lo especificado

**Veredicto**: Comportamiento correcto, detecta paradas graduales perfectamente.

---

## 🔍 Patrones Detectados en Todo el Log

### 1. **Cambios Abruptos Frecuentes**
El gateway detectó múltiples cambios abruptos con estos porcentajes:
- **26.1%** (48→58ms) - seq 809→817
- **44.7%** (47→68ms) - seq 838→847  
- **20.8%** (48→38ms) - seq 914→919
- **123.1%** (26→58ms) - seq 935→941
- **69.2%** (26→44ms) - seq 952→958
- **20.8%** (53→42ms) - seq 1015→1032

**Problema**: Umbral de 20% genera **demasiadas fragmentaciones**.

### 2. **Fragmentos STABLE Muy Cortos**
Muchos eventos STABLE con pocos pulsos:
- 6 pulsos: seq 858-863, 935-940, 952-957, 969-974, 986-991
- 8 pulsos: seq 809-816, 895-902
- 9 pulsos: seq 838-846

**Problema**: Esto genera **muchos mensajes MQTT** innecesarios.

### 3. **Eventos TRANSITION Correctos**
Los eventos TRANSITION tienen buen tamaño:
- 11-23 pulsos por transición
- Promedios de período coherentes (28-77ms)

---

## 📈 Métricas del Sistema

### Publicaciones MQTT
- **Total mensajes**: ~96 (sequenceNumber 74-96 en el log)
- **Tamaño promedio**: 360-450 bytes
- **Tópicos**:
  - `water-consumption`: telemetría de pulsos
  - `healthcheck`: estado del sistema
  - `critical`: eventos críticos (14 pulsos filtrados)

### Memoria
```
Stack watermark: 664 bytes
Heap: 79-84 KB libre
Largest block: 47 KB
```
✅ **Memoria saludable**

### Estadísticas de Pulsos
```
Total pulses: 850 (pin receiver stats)
Debounced pulses in 60s: 14
```

---

## 🎯 Conclusiones y Recomendaciones

### ✅ LO QUE FUNCIONA BIEN
1. ✅ Detección de estados IDLE → TRANSITION → STABLE
2. ✅ Timeouts correctos (~2s)
3. ✅ Cálculo de períodos promedio
4. ✅ Conversión NTP correcta
5. ✅ Publicación MQTT robusta
6. ✅ Memoria estable

### ❌ PROBLEMAS IDENTIFICADOS

#### **P1: Fragmentación excesiva de eventos STABLE**
**Causa**: Umbral de cambio abrupto = 20% es muy sensible
**Impacto**: 
- Muchos fragmentos STABLE de 5-9 pulsos
- Incrementa mensajes MQTT innecesariamente
- Dificulta análisis de flujo continuo

**Solución propuesta**:
```cpp
// En mode_read.cpp, aumentar umbral
const float ABRUPT_CHANGE_THRESHOLD = 0.30; // De 0.20 a 0.30 (30%)
```

#### **P2: Ruido/rebotes en el sensor**
**Causa**: 14 pulsos filtrados en 60s, períodos anómalos (10ms, 18ms, 26ms)
**Impacto**: 
- Períodos muy cortos que generan cambios abruptos falsos
- Sistema compensa con debouncing pero sigue generando ruido

**Solución hardware**:
- Añadir circuito RC al sensor (100nF + 10kΩ)

**Solución software** (opcional):
```cpp
// En common.cpp, aumentar ventana de debouncing
const unsigned long DEBOUNCE_MICROS = 15000; // De 10ms a 15ms
```

#### **P3: Test cases incompletos**
**Causa**: Los TC1 y TC3 mostraron menos pulsos de los esperados
**Impacto**: No valida completamente los escenarios definidos

**Solución**: Verificar que los test cases se ejecuten completamente antes de cambiar a otro.

---

## 📋 Acciones Recomendadas

### Prioridad ALTA
1. **Aumentar umbral de cambio abrupto de 20% a 30%**
   - Reducirá fragmentación en eventos STABLE
   - Archivo: `src/mode_read.cpp`
   
2. **Verificar ejecución completa de test cases**
   - Asegurar que TC1 genere ~50 pulsos
   - Asegurar que TC3 genere ~60 pulsos

### Prioridad MEDIA
3. **Considerar aumentar debounce a 15ms**
   - Solo si persisten períodos <20ms
   - Archivo: `src/common.cpp`

### Prioridad BAJA (Hardware)
4. **Añadir circuito RC al sensor**
   - 100nF capacitor + 10kΩ resistor
   - Reducirá ruido en origen

---

## 🧪 Validación Adicional Requerida

Para confirmar estos análisis, ejecutar:
1. **TC2 completo** (esperado: 75 pulsos en 5 escalones)
2. **TC4** (cambio brusco entre 2 flujos estables)
3. **TC6** (parada gradual de 60 pulsos)

Y verificar:
- Fragmentación de eventos STABLE
- Total de pulsos recibidos vs esperados
- Comportamiento con umbral de 30%
