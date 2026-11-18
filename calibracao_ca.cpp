#include <Arduino.h>

// Parâmetros internos
static float ganho_escala = 1.0f;
static float offset_dc = 0.0f;
static uint32_t amostras_janela = 400; // Múltiplo exato de ciclos (ex: 400 amostras = 20 ciclos a 1200 Hz)

// Acumuladores internos para atualização contínua
static float soma_dc = 0.0f;
static uint32_t contador_dc = 0;

// 1. Calibra o nível DC inicial (chamar no setup antes de iniciar o timer)
void Calibracao_Init(uint8_t pino_adc, float escala, uint32_t janela) {
    ganho_escala = escala;
    amostras_janela = janela;
    soma_dc = 0.0f;
    contador_dc = 0;

    // Faz uma leitura inicial rápida de 200 amostras espaçadas para achar o repouso DC
    float soma_inicial = 0.0f;
    for (int i = 0; i < 200; i++) {
        soma_inicial += analogReadMilliVolts(pino_adc);
        delayMicroseconds(833); // ~1200 Hz (cobre 10 ciclos de 60 Hz)
    }
    offset_dc = soma_inicial / 200.0f;
}

// 2. Remove o offset continuamente e retorna o sinal CA já escalonado (chamar a cada amostra)
float IRAM_ATTR Calibracao_Aplicar(float leitura_bruta) {
    // Acumula o valor bruto (com DC) para rastrear deriva térmica
    soma_dc += leitura_bruta;
    contador_dc++;

    // Quando fechar a janela exata de ciclos, atualiza o offset suavemente
    if (contador_dc >= amostras_janela) {
        float media_atual = soma_dc / (float)amostras_janela;
        offset_dc = (0.85f * offset_dc) + (0.15f * media_atual); // Filtro suave sem degraus
        soma_dc = 0.0f;
        contador_dc = 0;
    }

    // Subtrai o nível DC e aplica o ganho de calibração da tensão CA
    return (leitura_bruta - offset_dc) * ganho_escala;
}

// Função auxiliar caso queira monitorar o valor atual do nível DC
float Calibracao_GetOffset(void) {
    return offset_dc;
}
