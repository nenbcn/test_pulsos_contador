#ifndef MODE_RECIRCULATOR_H
#define MODE_RECIRCULATOR_H

#include "common.h"

// Variables específicas del modo RECIRCULATOR
extern bool recirculator_power_state;
extern unsigned long recirculator_start_time;
extern float recirculator_temp;
extern float recirculator_max_temp;

// Funciones del modo RECIRCULATOR
void inicializarRecirculador();
void setRecirculatorPower(bool state);
void leerTemperaturaRecirculador();
void controlarRecirculadorAutomatico();
void mostrarPantallaRecirculador();
void manejarModoRecirculador();
void manejarBotonIzquierdoRecirculator();

#endif
