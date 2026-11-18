#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

static const char* ssid_wifi;
static const char* senha_wifi;
static String base_url_fb;
static String auth_param_fb;
static String url_atual;
static unsigned long ultima_tentativa_wifi = 0;

const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("\n[WiFi] Falha na conexao. Motivo: %d\n", info.wifi_sta_disconnected.reason);
    }
}

void Firebase_Init(const char* ssid, const char* senha, const char* host_url, const char* auth_token) {
    ssid_wifi = ssid;
    senha_wifi = senha;

    base_url_fb = String(host_url);
    if (!base_url_fb.endsWith("/")) {
        base_url_fb += "/";
    }

    auth_param_fb = "";
    if (auth_token != NULL && strlen(auth_token) > 0) {
        auth_param_fb = "?auth=" + String(auth_token);
    }

    url_atual = base_url_fb + "leituras.json" + auth_param_fb;

    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.begin(ssid_wifi, senha_wifi);
    
    Serial.print("Conectando ao WiFi");
    uint8_t tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nWiFi Conectado! IP: ");
        Serial.println(WiFi.localIP());
        configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "a.st1.ntp.br");
    } else {
        Serial.println("\nNao conectou no setup.");
        ultima_tentativa_wifi = millis();
    }
}

void Firebase_ManterWifi(void) {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - ultima_tentativa_wifi >= 15000) {
            ultima_tentativa_wifi = millis();
            Serial.println("Reiniciando tentativa WiFi...");
            WiFi.disconnect(true, true);
            delay(100);
            WiFi.begin(ssid_wifi, senha_wifi);
        }
    }
}

void Firebase_EnviarResumo(String data_hora, float f_med, float f_min, float f_max, 
                           float v_med, float v_min, float v_max, uint32_t amostras, uint32_t perdidas) {
    if (WiFi.status() != WL_CONNECTED) return;

    // Formata o timestamp para servir como chave cronológica no Firebase (ex: "2026-11-17_18-30-00")
    String chave_tempo = data_hora;
    chave_tempo.replace(" ", "_");
    chave_tempo.replace(":", "-");

    String json = "{";
    json += "\"data_hora\":\"" + data_hora + "\",";
    json += "\"freq_med\":"   + String(f_med, 2) + ",";
    json += "\"freq_min\":"   + String(f_min, 2) + ",";
    json += "\"freq_max\":"   + String(f_max, 2) + ",";
    json += "\"vrms_med\":"   + String(v_med, 2) + ",";
    json += "\"vrms_min\":"   + String(v_min, 2) + ",";
    json += "\"vrms_max\":"   + String(v_max, 2) + ",";
    json += "\"amostras\":"   + String(amostras) + ",";
    json += "\"perdidas\":"   + String(perdidas);
    json += "}";

    HTTPClient http;
    
    // 1. Atualiza o nó 'leituras' (último resumo ao vivo)
    http.begin(url_atual);
    http.addHeader("Content-Type", "application/json");
    http.PUT(json);
    http.end();

    // 2. Salva no nó 'historico' usando a data/hora de abertura como chave
    String url_registro = base_url_fb + "historico/" + chave_tempo + ".json" + auth_param_fb;
    http.begin(url_registro);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.PUT(json);
    
    if (httpCode > 0) {
        Serial.printf("Historico 1min salvo [%s] -> HTTP %d\n", chave_tempo.c_str(), httpCode);
    } else {
        Serial.printf("Erro HTTP: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}
