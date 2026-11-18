#include <Arduino.h> // Necessário para reconhecer o IRAM_ATTR
#include <math.h>

#define TWO_PI 6.28318530718f

// Parâmetros internos de configuração
static float Ts = 0.0f;
static float w_nom = 0.0f;
static float k_sogi = 0.0f;
static float kp = 0.0f;
static float ki = 0.0f;

// Estados anteriores do SOGI
static float v_in_1 = 0.0f, v_in_2 = 0.0f;
static float v_alpha_1 = 0.0f, v_alpha_2 = 0.0f;
static float v_beta_1 = 0.0f, v_beta_2 = 0.0f;

// Estados internos do PLL
static float v_q_1 = 0.0f;
static float dw_1 = 0.0f;
static float w = 0.0f;
static float theta = 0.0f;

// Saídas globais prontas para leitura
float pll_v_pico = 0.0f;
float pll_v_rms  = 0.0f;
float pll_theta  = 0.0f;
float pll_freq   = 0.0f;

void SOGI_PLL_Init(float fs, float f_nom, float k, float prop_gain, float int_gain) {
    Ts = 1.0f / fs;
    w_nom = TWO_PI * f_nom;
    k_sogi = k;
    kp = prop_gain;
    ki = int_gain;

    v_in_1 = v_in_2 = 0.0f;
    v_alpha_1 = v_alpha_2 = 0.0f;
    v_beta_1 = v_beta_2 = 0.0f;

    v_q_1 = 0.0f;
    dw_1 = 0.0f;
    w = w_nom;
    theta = 0.0f;

    pll_v_pico = 0.0f;
    pll_v_rms  = 0.0f;
    pll_theta  = 0.0f;
    pll_freq   = f_nom;
}

void IRAM_ATTR SOGI_PLL_Update(float v_in) {
    float w_ts = w * Ts;
    float x = 2.0f * k_sogi * w_ts;
    float y = w_ts * w_ts;
    float inv_denom = 1.0f / (x + y + 4.0f);

    // Coeficientes do filtro SOGI (Tustin)
    float b0 = x * inv_denom;
    float k_y_denom = (k_sogi * y) * inv_denom;
    float a1 = 2.0f * (4.0f - y) * inv_denom;
    float a2 = (x - y - 4.0f) * inv_denom;

    // 1. Sinais em quadratura (v_alpha e v_beta)
    float v_alpha = b0 * (v_in - v_in_2) + a1 * v_alpha_1 + a2 * v_alpha_2;
    float v_beta  = k_y_denom * (v_in + 2.0f * v_in_1 + v_in_2) + a1 * v_beta_1 + a2 * v_beta_2;

    // Atualiza históricos
    v_in_2 = v_in_1;
    v_in_1 = v_in;
    v_alpha_2 = v_alpha_1;
    v_alpha_1 = v_alpha;
    v_beta_2 = v_beta_1;
    v_beta_1 = v_beta;

    // 2. Transformada de Park
    float sin_theta = sinf(theta);
    float cos_theta = cosf(theta);
    float v_q = -v_alpha * sin_theta + v_beta * cos_theta;

    // 3. Controlador PI
    float dw = dw_1 + kp * (v_q - v_q_1) + (ki * Ts) * v_q;
    
    // Limite de segurança (+/- 10 Hz)
    if (dw > 62.83f) dw = 62.83f;
    if (dw < -62.83f) dw = -62.83f;

    v_q_1 = v_q;
    dw_1 = dw;

    // 4. Integração da fase
    w = w_nom + dw;
    theta += w * Ts;
    if (theta >= TWO_PI) {
        theta -= TWO_PI;
    }

    // 5. Atualiza as saídas globais
    pll_v_pico = sqrtf(v_alpha * v_alpha + v_beta * v_beta);
    pll_v_rms  = pll_v_pico * 0.70710678f;
    pll_theta  = theta;
    pll_freq   = w * 0.15915494f;
}
