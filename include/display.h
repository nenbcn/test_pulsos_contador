#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

// Funciones de pantalla comunes
void mostrarVoltaje();
void mostrarModo();
void dibujarMarcoGrafico(bool es_presion);
void dibujarLineasReferencia();
void inicializarGrafico();
void actualizarGrafico(float nueva_frecuencia);
void actualizarGraficoGenerico(float* data, int* index, float nuevo_valor, 
                                float min_scale, float max_scale, 
                                uint16_t color_fill, uint16_t color_line,
                                bool auto_scale = false);

// Funciones auxiliares
void playTone(int frequency, int duration_ms);
void stopTone();
uint16_t getSignalColor(int32_t rssi);

#endif
