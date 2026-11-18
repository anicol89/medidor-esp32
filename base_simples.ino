// ============================================================================
//  CÓDIGO BASE SIMPLES — amostragem com período fixo (ESP32, core 3.x)
//
//  timer (a cada 1/fs) -> ISR -> acorda taskRapida -> lê sensores -> calcula
//  loop() -> funções lentas (é pausado a cada amostra e continua de onde parou)
//
//  Parâmetros ajustáveis: config.h
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "segredos.h"

// ----------------------- Variáveis e Constantes de Configuração----------------
const float FS = 1000000.0f / TICKS;  // frequência de amostragem real [Hz]
volatile uint32_t k = 0;               // contador de amostras (t = k / FS)
volatile uint32_t perdidas = 0;        // amostras perdidas (deve ficar 0)
TaskHandle_t hRapida = NULL;
hw_timer_t  *timer   = NULL;

volatile float    v_ant=0., vrms=0., vo=0.;
volatile float    printLL1 = 0., printLL2 = 0., printLL3 = 0.; // média Nc pontos

// ----------------------- Declaração dos Módulos Externos --------------------
// 1. Módulo SOGI-PLL (sogi_pll.cpp)
extern float pll_v_pico, pll_v_rms, pll_theta, pll_freq;
void SOGI_PLL_Init(float fs, float f_nom, float k, float prop_gain, float int_gain);
void SOGI_PLL_Update(float v_in);

// 2. Módulo de Calibração e Nível DC (calibracao_ca.cpp)
void Calibracao_Init(uint8_t pino_adc, float escala, uint32_t janela);
float Calibracao_Aplicar(float leitura_bruta);
float Calibracao_GetOffset(void);

// 3. Módulo de Estatísticas de 1 Minuto (estatisticas_1min.cpp)
extern float stat_freq_med, stat_freq_min, stat_freq_max;
extern float stat_vrms_med, stat_vrms_min, stat_vrms_max;
extern uint32_t stat_amostras;
extern String stat_data_hora;
void Estatisticas_Init(unsigned long tempo_janela_ms);
bool Estatisticas_Update(float freq, float vrms);

// 4. Módulo Wi-Fi e Firebase (firebase_mod.cpp)
void Firebase_Init(const char* ssid, const char* senha, const char* host_url, const char* auth_token);
void Firebase_ManterWifi(void);
void Firebase_EnviarResumo(String data_hora, float f_med, float f_min, float f_max, 
                           float v_med, float v_min, float v_max, uint32_t amostras, uint32_t perdidas);


// ----------------------------- Interrupção ----------------------------------
// Só acorda a task: analogRead não pode ser chamado dentro da ISR no core 3.x
void IRAM_ATTR onTimer() {
  BaseType_t acordou = pdFALSE;
  vTaskNotifyGiveFromISR(hRapida, &acordou);
  if (acordou) portYIELD_FROM_ISR();
}

// --------------------- Cálculos (executa a cada amostra) --------------------
void calcular(float v) {
  //se quiser reduzir o código texto escrito na task rapida

  
}

// --------------------------- Task rápida (1/fs) -----------------------------
void taskRapida(void *) {
  for (;;) {
    uint32_t pend = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // espera o timer
    if (pend > 1) perdidas = perdidas + (pend - 1);
    
    //*****Código a partir daqui ----------------------------------------------
    unsigned long t_inicio=micros();
    float v0 = analogReadMilliVolts(PIN_ZMPT);              // 1) leitura
    float v = Calibracao_Aplicar(v0);

    SOGI_PLL_Update(v);

    printLL1 = pll_freq;
    printLL3 = pll_v_rms;

    
    calcular(v);                                                // 2) cálculos
    k = k + 1;
    unsigned long tempo_gasto = micros() - t_inicio;
    printLL2 = tempo_gasto;
    
    //*****Até aqui -----------------------------------------------------------
  }
}

// --------------------------------- Setup ------------------------------------
void setup() {
  Serial.begin(BAUD_SERIAL);
  delay(500);
  analogReadResolution(12);            // atenuação padrão já é 11 dB (0–3,1 V)

  //Inicializa ganho das funcoes
  Calibracao_Init(PIN_ZMPT, 0.6667f, 400);
  SOGI_PLL_Init(FS, 60.0f, 1.0f, 0.010f, 0.890f);
  Estatisticas_Init(60000);
  Firebase_Init(MEU_WIFI_SSID, MEU_WIFI_SENHA, MEU_FB_URL, MEU_FB_TOKEN);
  
  

#ifdef PWM_PIN
  // PWM: configurado uma vez; o hardware gera o sinal sozinho
  ledcAttach(PWM_PIN, PWM_FREQ_HZ, PWM_BITS);
  ledcWrite(PWM_PIN, 0);               // duty inicial (0 .. 2^PWM_BITS - 1)
#endif

  // Task de prioridade máxima no CORE_RAPIDA -> interrompe o loop
  xTaskCreatePinnedToCore(taskRapida, "rapida", 4096, NULL,
                          configMAX_PRIORITIES - 1, &hRapida, CORE_RAPIDA);

  timer = timerBegin(1000000);         // 1 MHz -> 1 tick = 1 µs (NÃO usar FS aqui)
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, TICKS, true, 0);   // repete a cada TICKS µs

  Serial.printf("fs = %.2f Hz  (Ts = %d us)\n", FS, TICKS);


}

// ---------------------------- Loop (lento) ----------------------------------
void loop() {
// 1. Mantém a conexão Wi-Fi ativa em segundo plano
  Firebase_ManterWifi();

  // 2. Executa a cada 1 segundo (1 Hz -> 1000 ms)
  static uint32_t t_1hz = 0;
  if (millis() - t_1hz >= 1000) {
    t_1hz = millis();

    // Alimenta o acumulador de 1 minuto (1 amostra/segundo = 60 amostras/minuto)
    // Quando fechar a virada do minuto, retorna 'true' e dispara o envio ao Firebase
    if (Estatisticas_Update(pll_freq, pll_v_rms)) {
      Firebase_EnviarResumo(stat_data_hora,
                            stat_freq_med, stat_freq_min, stat_freq_max,
                            stat_vrms_med, stat_vrms_min, stat_vrms_max,
                            stat_amostras, perdidas);
    }

    // Impressão periódica no Monitor Serial para acompanhamento local
    Serial.printf("k=%lu | perd=%lu | freq=%.2f Hz | tempo=%.0f us | Vrms=%.2f V\n",
                  (unsigned long)k, (unsigned long)perdidas, printLL1, printLL2, printLL3);
  }

  // >>> funções lentas aqui <<<
  // PWM: para mudar o duty a qualquer momento ->  ledcWrite(PWM_PIN, valor);
}
