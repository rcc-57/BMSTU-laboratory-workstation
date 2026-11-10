#ifndef E502_SIM_H
#define E502_SIM_H

#include <stdint.h>
#include <stdbool.h>
#include "x502api.h"
#include "e502api.h"

/* Типы синтетических сигналов для имитации на физических входах АЦП */
typedef enum {
    E502_SIM_WAVE_SINE     = 0, /* Синусоида: offset + amp * sin(2*pi*freq*t + phase) */
    E502_SIM_WAVE_SQUARE   = 1, /* Меандр */
    E502_SIM_WAVE_TRIANGLE = 2, /* Треугольный сигнал */
    E502_SIM_WAVE_SAWTOOTH = 3, /* Пилообразный сигнал */
    E502_SIM_WAVE_DC       = 4, /* Постоянное напряжение (offset) */
    E502_SIM_WAVE_DAC1_LP  = 5, /* Петля обратной связи с выхода ЦАП1 */
    E502_SIM_WAVE_DAC2_LP  = 6  /* Петля обратной связи с выхода ЦАП2 */
} E502_SimWaveType;

typedef struct {
    E502_SimWaveType type;
    double amplitude;   /* Амплитуда, В */
    double freq_hz;     /* Частота, Гц */
    double offset;      /* Постоянное смещение, В */
    double phase_rad;   /* Начальная фаза, рад */
    double noise_amp;   /* Амплитуда белого шума, В */
} E502_SimPhyChannel;

typedef struct {
    uint32_t active_handles;     /* Число созданных и еще не освобожденных t_x502_hnd */
    uint32_t open_connections;   /* Число открытых соединений */
    uint32_t configure_calls;    /* Счетчик вызовов X502_Configure */
    uint32_t adc_frames_read;    /* Счетчик считанных кадров АЦП */
    uint32_t led_blinks;         /* Счетчик миганий светодиодом */
} E502_SimStats;

/* Управление генератором искусственных сигналов и диагностикой имитатора */
void E502_SimReset(void);
void E502_SimSetPhySignal(uint32_t phy_ch, E502_SimWaveType type,
                          double amplitude, double freq_hz, double offset, double noise_amp);
void E502_SimGetPhySignal(uint32_t phy_ch, E502_SimPhyChannel *out_sig);
void E502_SimStepTime(double dt_sec);
double E502_SimGetTime(void);
void E502_SimInjectFault(int32_t open_err, int32_t config_err);
void E502_SimGetStats(E502_SimStats *out_stats);
bool E502_SimHasRealDriver(void);
bool E502_SimIsSerialSimulated(const char *serial);

#endif /* E502_SIM_H */

