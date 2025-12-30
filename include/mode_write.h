#ifndef MODE_WRITE_H
#define MODE_WRITE_H

#include "common.h"

// Variables específicas del modo WRITE
extern bool generating_pulse;
extern unsigned long next_pulse_time;
extern float pulse_interval;
extern float target_frequency;
extern float current_gen_frequency;
extern bool pulse_state;
extern unsigned long pulse_count_generated;
extern TestCase current_test;
extern unsigned long test_start_time;
extern PulsePattern pulse_pattern;
extern int current_pulse_index;
extern bool pattern_ready;

// Arrays de fases para cada test case
extern PulsePhase test1_phases[];
extern PulsePhase test2_phases[];
extern PulsePhase test3_phases[];
extern PulsePhase test4_phases[];
extern PulsePhase test5_phases[];

// Funciones del modo WRITE
void inicializarGenerador();
void preGenerarPatron(TestCase tc);
void generarPulsos();
void manejarModoWrite();
void manejarBotonIzquierdoWrite();

// Funciones auxiliares
float aplicarJitter(float tempo, float jitter_percent, unsigned long pulse_number);
PulsePhase* getTestPhases(TestCase tc, int* phase_count);

#endif
