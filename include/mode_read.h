#ifndef MODE_READ_H
#define MODE_READ_H

#include "common.h"

// Variables específicas del modo READ
extern volatile unsigned long pulse_count;
extern unsigned long last_pulse_count;
extern unsigned long last_pulse_time;
extern float pulse_frequency;

// Funciones del modo READ
void IRAM_ATTR pulseInterrupt();
void inicializarModoRead();
void manejarModoRead();
void mostrarInfoSensorRead();

#endif
