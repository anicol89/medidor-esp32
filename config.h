#pragma once
// ============================================================================
//  CONFIGURAÇÃO — edite aqui; o .ino não precisa ser alterado para estes ajustes
// ============================================================================

// ------------------------------- Amostragem ---------------------------------
// Período de amostragem em µs (o timer conta em µs, então só valores inteiros).
//   833 µs -> fs = 1e6/833 = 1200,48 Hz  (~20 pontos por ciclo de 60 Hz)
//   Pior caso de tempo disponível por amostra = TICKS µs
//   (cada analogReadMilliVolts leva ~43 µs)
#define TICKS        833

// Core e prioridade da task rápida.
//   Core 1 = mesmo core do loop(): o loop é pausado a cada amostra.
//   O WiFi roda no core 0.
#define CORE_RAPIDA  1

// ------------------------------- Sensores -----------------------------------
// Use SÓ pinos do ADC1: GPIO 32, 33, 34, 35, 36, 39
// (ADC2 não funciona com WiFi ligado)
#define PIN_ZMPT     32       // tensão CA (ZMPT101B)

// ------------------------------- Serial -------------------------------------
#define BAUD_SERIAL  115200
#define PERIODO_PRINT_MS 1000 // intervalo das mensagens de diagnóstico no loop

// ------------------------------- PWM (futuro) -------------------------------
// Saída PWM por hardware (LEDC): roda sozinha, sem interferir na amostragem.
// Para usar, descomente as 3 linhas abaixo e veja "PWM" no .ino.
//   Resolução máxima a 5 kHz = 13 bits (80 MHz / 5 kHz = 16000 > 2^13)
//   Não use GPIO 34..39 (são só entrada).
// #define PWM_PIN       25
// #define PWM_FREQ_HZ   5000
// #define PWM_BITS      10    // duty de 0 a 1023
