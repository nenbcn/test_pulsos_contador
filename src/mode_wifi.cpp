#include "mode_wifi.h"
#include "display.h"

// Variables específicas del modo WIFI
WiFiNetwork wifi_networks[MAX_WIFI_NETWORKS];
int wifi_count = 0;
unsigned long last_wifi_scan = 0;
bool wifi_scanning = false;
int wifi_page = 0;

void escanearWiFi() {
  if (wifi_scanning) return;
  
  wifi_scanning = true;
  Serial.println("Iniciando escaneo WiFi...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  int network_count = WiFi.scanNetworks(false, false, false, 300, 0);
  wifi_count = min(network_count, MAX_WIFI_NETWORKS);
  
  if (network_count > 0) {
    Serial.printf("Encontradas %d redes:\n", network_count);
    
    for (int i = 0; i < wifi_count; i++) {
      wifi_networks[i].ssid = WiFi.SSID(i);
      wifi_networks[i].rssi = WiFi.RSSI(i);
      wifi_networks[i].encryption = WiFi.encryptionType(i);
      wifi_networks[i].color = getSignalColor(wifi_networks[i].rssi);
      
      Serial.printf("  %d: %s (%d dBm) %s\n", 
                    i + 1, 
                    wifi_networks[i].ssid.c_str(), 
                    wifi_networks[i].rssi,
                    (wifi_networks[i].encryption == WIFI_AUTH_OPEN) ? "[ABIERTA]" : "[PROTEGIDA]");
    }
  } else {
    Serial.println("No se encontraron redes WiFi");
    wifi_count = 0;
  }
  
  WiFi.scanDelete();
  wifi_scanning = false;
  last_wifi_scan = millis();
}

void mostrarListaWiFi() {
  static int last_wifi_count = -1;
  static int last_page = -1;
  
  if (wifi_count != last_wifi_count || wifi_page != last_page) {
    tft.fillRect(0, 22, 240, 113, TFT_BLACK);
    
    tft.fillRect(0, 0, 240, 20, TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(1);
    tft.setTextFont(2);
    tft.drawString("SCANNER WiFi", 5, 2);
    
    tft.setTextColor(TFT_MAGENTA);
    tft.setTextFont(2);
    String page_info = "Pag " + String(wifi_page + 1) + "/3";
    tft.drawString(page_info, 140, 2);
    
    if (wifi_scanning) {
      tft.setTextColor(TFT_YELLOW);
      tft.setTextFont(1);
      tft.drawString("Scan...", 200, 5);
    } else {
      unsigned long next_scan = last_wifi_scan + WIFI_SCAN_INTERVAL_MS;
      unsigned long remaining = (next_scan > millis()) ? (next_scan - millis()) / 1000 : 0;
      if (remaining > 0) {
        tft.setTextColor(TFT_DARKGREY);
        tft.setTextFont(1);
        tft.drawString(String(remaining) + "s", 210, 5);
      }
    }
    
    int start_idx = wifi_page * NETWORKS_PER_PAGE;
    int end_idx = min(start_idx + NETWORKS_PER_PAGE, wifi_count);
    
    for (int i = start_idx; i < end_idx; i++) {
      int y_pos = 25 + (i - start_idx) * 20;
      
      tft.fillCircle(10, y_pos + 10, 6, wifi_networks[i].color);
      tft.drawCircle(10, y_pos + 10, 6, TFT_WHITE);
      
      String ssid = wifi_networks[i].ssid;
      if (ssid.length() > 20) {
        ssid = ssid.substring(0, 20) + "..";
      }
      
      tft.setTextColor(TFT_WHITE);
      tft.setTextFont(2);
      tft.drawString(ssid, 22, y_pos + 4);
      
      tft.setTextColor(wifi_networks[i].color);
      tft.setTextFont(1);
      String rssi_text = String(wifi_networks[i].rssi);
      tft.drawString(rssi_text, 200, y_pos + 8);
    }
    
    if (wifi_count > 0) {
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextFont(1);
      
      for (int p = 0; p < MAX_WIFI_PAGES; p++) {
        int page_start = p * NETWORKS_PER_PAGE;
        if (page_start < wifi_count) {
          uint16_t color = (p == wifi_page) ? TFT_WHITE : TFT_DARKGREY;
          tft.setTextColor(color);
          tft.drawString(String(p + 1), 200 + p * 10, 130);
        }
      }
      
      tft.setTextColor(TFT_DARKGREY);
      tft.drawString("Izq: Pag", 5, 130);
      
      String info = String(wifi_count) + " redes";
      tft.drawString(info, 80, 130);
    }
    
    last_wifi_count = wifi_count;
    last_page = wifi_page;
  }
}

void mostrarPantallaScanningWiFi() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.drawString("SCANNER WiFi", 5, 5);
  
  tft.setTextColor(TFT_YELLOW);
  tft.setTextFont(2);
  tft.drawString("Escaneando...", 5, 35);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextFont(1);
  tft.drawString("Buscando redes disponibles", 5, 55);
  
  mostrarModo();
  mostrarVoltaje();
  
  for (int i = 0; i < 200; i += 20) {
    tft.drawLine(20 + i, 80, 20 + i + 15, 80, TFT_CYAN);
    tft.drawLine(20 + i, 81, 20 + i + 15, 81, TFT_CYAN);
    delay(50);
  }
}

void manejarModoWiFi() {
  unsigned long current_time = millis();
  
  if (!wifi_scanning && (current_time - last_wifi_scan >= WIFI_SCAN_INTERVAL_MS)) {
    escanearWiFi();
  }
  
  mostrarListaWiFi();
}

void manejarBotonIzquierdoWiFi() {
  wifi_page = (wifi_page + 1) % MAX_WIFI_PAGES;
  Serial.println("Cambiando a página WiFi: " + String(wifi_page + 1));
}
