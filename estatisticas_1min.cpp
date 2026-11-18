#include <Arduino.h>
#include <time.h>

static unsigned long janela_ms = 60000;
static unsigned long inicio_janela_ms = 0;
static int ultimo_minuto_ntp = -1;

// Acumuladores internos
static double soma_freq = 0.0;
static double soma_vrms = 0.0;
static float temp_freq_min = 999.0f, temp_freq_max = 0.0f;
static float temp_vrms_min = 9999.0f, temp_vrms_max = 0.0f;
static uint32_t cont_amostras = 0;

// Guarda a data/hora exata de ABERTURA da janela atual
static String timestamp_abertura_atual = "";

// Variáveis globais prontas com o resultado fechado do último minuto
float stat_freq_med = 0.0f, stat_freq_min = 0.0f, stat_freq_max = 0.0f;
float stat_vrms_med = 0.0f, stat_vrms_min = 0.0f, stat_vrms_max = 0.0f;
uint32_t stat_amostras = 0;
String stat_data_hora = "";

// Função auxiliar para capturar o relógio atual formatado
String capturarRelogioAtual() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
        return "Sem_NTP_" + String(millis() / 1000);
    }
    char texto_data[32];
    strftime(texto_data, sizeof(texto_data), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(texto_data);
}

void Estatisticas_Init(unsigned long tempo_janela_ms) {
    janela_ms = tempo_janela_ms;
    inicio_janela_ms = millis();
    timestamp_abertura_atual = capturarRelogioAtual();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        ultimo_minuto_ntp = timeinfo.tm_min;
    }

    soma_freq = 0.0;
    soma_vrms = 0.0;
    temp_freq_min = 999.0f; temp_freq_max = 0.0f;
    temp_vrms_min = 9999.0f; temp_vrms_max = 0.0f;
    cont_amostras = 0;
}

bool Estatisticas_Update(float freq, float vrms) {
    // 1. Acumula leituras válidas
    if (freq > 10.0f && freq < 120.0f && vrms >= 0.0f) {
        soma_freq += freq;
        soma_vrms += vrms;

        if (freq < temp_freq_min) temp_freq_min = freq;
        if (freq > temp_freq_max) temp_freq_max = freq;

        if (vrms < temp_vrms_min) temp_vrms_min = vrms;
        if (vrms > temp_vrms_max) temp_vrms_max = vrms;

        cont_amostras++;
    }

    // 2. Verifica fechamento alinhado à virada do minuto NTP (ex: :00) ou por millis()
    bool fechou_janela = false;
    struct tm timeinfo;
    bool tem_ntp = getLocalTime(&timeinfo, 5);

    if (janela_ms == 60000 && tem_ntp) {
        if (ultimo_minuto_ntp == -1) {
            ultimo_minuto_ntp = timeinfo.tm_min;
            timestamp_abertura_atual = capturarRelogioAtual();
        } else if (timeinfo.tm_min != ultimo_minuto_ntp) {
            ultimo_minuto_ntp = timeinfo.tm_min;
            fechou_janela = true;
        }
    } else {
        if (millis() - inicio_janela_ms >= janela_ms) {
            inicio_janela_ms = millis();
            fechou_janela = true;
        }
    }

    if (!fechou_janela) return false;
    if (cont_amostras == 0) {
        timestamp_abertura_atual = capturarRelogioAtual();
        return false;
    }

    // 3. Fecha o pacote usando o TIMESTAMP DE ABERTURA da janela encerrada
    stat_data_hora = timestamp_abertura_atual;
    stat_freq_med  = (float)(soma_freq / cont_amostras);
    stat_freq_min  = temp_freq_min;
    stat_freq_max  = temp_freq_max;

    stat_vrms_med  = (float)(soma_vrms / cont_amostras);
    stat_vrms_min  = temp_vrms_min;
    stat_vrms_max  = temp_vrms_max;
    stat_amostras  = cont_amostras;

    // 4. Abre a nova janela marcando o horário atual de início
    timestamp_abertura_atual = capturarRelogioAtual();
    soma_freq = 0.0;
    soma_vrms = 0.0;
    temp_freq_min = 999.0f; temp_freq_max = 0.0f;
    temp_vrms_min = 9999.0f; temp_vrms_max = 0.0f;
    cont_amostras = 0;

    return true;
}
