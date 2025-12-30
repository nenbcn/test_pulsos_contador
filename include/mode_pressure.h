#ifndef MODE_PRESSURE_H
#define MODE_PRESSURE_H

#include "common.h"
#include <Wire.h>

// Variables específicas del modo PRESSURE
extern float pressure_graph_data[GRAPH_WIDTH];
extern int pressure_graph_index;
extern float pressure_min_scale;
extern float pressure_max_scale;
extern float pressure_historical_min;
extern float pressure_historical_max;
extern bool pressure_history_initialized;
extern unsigned long last_pressure_read;
extern bool pressure_auto_scale;

// Funciones del modo PRESSURE
WNK1MA_Reading readWNK1MA();
void inicializarModoPressure();
void manejarModoPressure();
void actualizarHistoricoPresion(float nuevo_valor);
void actualizarGraficoPresion(float nuevo_valor);
void mostrarInfoSensorPressure();

#endif
