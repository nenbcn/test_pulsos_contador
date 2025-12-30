#ifndef MODE_WIFI_H
#define MODE_WIFI_H

#include "common.h"

// Variables específicas del modo WIFI
extern WiFiNetwork wifi_networks[MAX_WIFI_NETWORKS];
extern int wifi_count;
extern unsigned long last_wifi_scan;
extern bool wifi_scanning;
extern int wifi_page;

// Funciones del modo WIFI
void escanearWiFi();
void mostrarListaWiFi();
void mostrarPantallaScanningWiFi();
void manejarModoWiFi();
void manejarBotonIzquierdoWiFi();

#endif
