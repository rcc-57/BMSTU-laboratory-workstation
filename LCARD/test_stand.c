#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "e502_ctrl.h"
#include "e502_sim.h"

static void test_lifecycle_and_leaks(void) {
    E502_SimReset();
    E502_StandDevice dev;
    E502_StandInitStruct(&dev);

    /* 1. Сбой при открытии USB не должен оставлять утечек дескриптора */
    E502_SimInjectFault(X502_ERR_DEVICE_NOT_FOUND, X502_ERR_OK);
    int err = E502_StandConnectUSB(&dev, "E502-SIM-0001");
    assert(err == X502_ERR_DEVICE_NOT_FOUND);
    assert(dev.last_error == X502_ERR_DEVICE_NOT_FOUND);
    assert(!dev.is_connected && dev.hnd == NULL);

    E502_SimStats st;
    E502_SimGetStats(&st);
    assert(st.active_handles == 0 && st.open_connections == 0);

    /* 2. Частичный сбой: успешный OpenUsb, но ошибка в X502_Configure */
    E502_SimInjectFault(X502_ERR_OK, X502_ERR_FPGA_REG_WRITE);
    err = E502_StandConnectUSB(&dev, "E502-SIM-0001");
    assert(err == X502_ERR_FPGA_REG_WRITE);
    assert(dev.last_error == X502_ERR_FPGA_REG_WRITE);
    assert(!dev.is_connected && dev.hnd == NULL);
    E502_SimGetStats(&st);
    assert(st.active_handles == 0 && st.open_connections == 0);

    /* 3. Повторный Connect без предварительного Disconnect не должен давать утечку */
    E502_SimInjectFault(X502_ERR_OK, X502_ERR_OK);
    assert(E502_StandConnectUSB(&dev, "E502-SIM-0001") == X502_ERR_OK);
    E502_SimGetStats(&st);
    assert(st.active_handles == 1 && st.open_connections == 1);

    assert(E502_StandConnectUSB(&dev, "E502-SIM-0002") == X502_ERR_OK);
    E502_SimGetStats(&st);
    assert(st.active_handles == 1 && st.open_connections == 1);
    assert(strcmp(dev.dev_info.serial, "E502-SIM-0002") == 0);

    uint32_t ip = (192U << 24) | (168U << 16) | (1U << 8) | 10U;
    assert(E502_StandConnectIP(&dev, ip, 500) == X502_ERR_OK);
    E502_SimGetStats(&st);
    assert(st.active_handles == 1 && st.open_connections == 1);

    E502_StandDisconnect(&dev);
    E502_SimGetStats(&st);
    assert(st.active_handles == 0 && st.open_connections == 0);
    printf("[PASS] test_lifecycle_and_leaks\n");
}

static void test_bounds_and_error_integrity(void) {
    E502_SimReset();
    E502_StandDevice dev;
    E502_StandInitStruct(&dev);

    /* Вызовы в состоянии offline обязаны выставлять dev.last_error = X502_ERR_DEVICE_NOT_OPENED */
    double volts[STAND_MAX_CHANNELS];
    assert(E502_StandSetChannelCount(&dev, 2) == X502_ERR_DEVICE_NOT_OPENED);
    assert(dev.last_error == X502_ERR_DEVICE_NOT_OPENED);
    assert(E502_StandReadAdcFrame(&dev, volts, 100) == X502_ERR_DEVICE_NOT_OPENED);
    assert(dev.last_error == X502_ERR_DEVICE_NOT_OPENED);
    assert(E502_StandWriteDac(&dev, X502_DAC_CH1, 1.0) == X502_ERR_DEVICE_NOT_OPENED);
    assert(dev.last_error == X502_ERR_DEVICE_NOT_OPENED);

    assert(E502_StandConnectUSB(&dev, "E502-SIM-0001") == X502_ERR_OK);

    /* Границы количества логических каналов (1..32) */
    assert(E502_StandSetChannelCount(&dev, 0) == X502_ERR_INVALID_LTABLE_SIZE);
    assert(E502_StandSetChannelCount(&dev, 33) == X502_ERR_INVALID_LTABLE_SIZE);
    assert(E502_StandSetChannelCount(&dev, 4) == X502_ERR_OK);

    /* Границы индекса lch и физических каналов (DIFF: 0..15, COMM: 0..31, avg: 0..128) */
    assert(E502_StandConfigChannel(&dev, 4, 0, X502_LCH_MODE_COMM, X502_ADC_RANGE_10, 0) == X502_ERR_INVALID_LCH_NUMBER);
    assert(E502_StandConfigChannel(&dev, 0, 16, X502_LCH_MODE_DIFF, X502_ADC_RANGE_10, 0) == X502_ERR_INVALID_LCH_PHY_NUMBER);
    assert(E502_StandConfigChannel(&dev, 0, 32, X502_LCH_MODE_COMM, X502_ADC_RANGE_10, 0) == X502_ERR_INVALID_LCH_PHY_NUMBER);
    assert(E502_StandConfigChannel(&dev, 0, 0, X502_LCH_MODE_COMM, X502_ADC_RANGE_10, 129) == X502_ERR_INVALID_LCH_AVG_SIZE);

    /* Границы ЦАП (-5.0..+5.0 В, запрет NaN/Inf) */
    assert(E502_StandWriteDac(&dev, X502_DAC_CH1, 5.001) == X502_ERR_INVALID_DAC_CHANNEL);
    assert(E502_StandWriteDac(&dev, X502_DAC_CH1, -5.001) == X502_ERR_INVALID_DAC_CHANNEL);
    assert(E502_StandWriteDac(&dev, X502_DAC_CH1, NAN) == X502_ERR_INVALID_DAC_CHANNEL);
    assert(E502_StandWriteDac(&dev, X502_DAC_CH1, INFINITY) == X502_ERR_INVALID_DAC_CHANNEL);

    /* Защита от OOB при прямом повреждении dev.lch_count */
    dev.lch_count = 64;
    assert(E502_StandApplyConfig(&dev) == X502_ERR_INVALID_LTABLE_SIZE);
    assert(E502_StandReadAdcFrame(&dev, volts, 100) == X502_ERR_INVALID_LTABLE_SIZE);
    dev.lch_count = 4;

    E502_StandDisconnect(&dev);
    printf("[PASS] test_bounds_and_error_integrity\n");
}

static void test_synthetic_signals_and_dsp(void) {
    E502_SimReset();
    E502_StandDevice dev;
    assert(E502_StandConnectUSB(&dev, "E502-SIM-0001") == X502_ERR_OK);
    assert(E502_StandSetChannelCount(&dev, 6) == X502_ERR_OK);

    /* Настраиваем искусственные сигналы без шума для точной проверки:
       Phy 0: Синус A=4.0В, F=250Гц (период 4 мс)
       Phy 1: Постоянное +3.5В (для DIFF +)
       Phy 17: Постоянное +1.2В (для DIFF - канала 1 -> ожидаем +2.3В)
       Phy 2: Постоянное +2.5В, но на LCH2 диапазон +/-0.5В (ожидаем насыщение +0.5В)
       Phy 4: Loopback с ЦАП1
       Phy 5: Loopback с ЦАП2 */
    E502_SimSetPhySignal(0,  E502_SIM_WAVE_SINE, 4.0, 250.0, 0.0, 0.0);
    E502_SimSetPhySignal(1,  E502_SIM_WAVE_DC,   0.0,   0.0, 3.5, 0.0);
    E502_SimSetPhySignal(17, E502_SIM_WAVE_DC,   0.0,   0.0, 1.2, 0.0);
    E502_SimSetPhySignal(2,  E502_SIM_WAVE_DC,   0.0,   0.0, 2.5, 0.0);
    E502_SimSetPhySignal(4,  E502_SIM_WAVE_DAC1_LP, 1.0, 0.0, 0.0, 0.0);
    E502_SimSetPhySignal(5,  E502_SIM_WAVE_DAC2_LP, 1.0, 0.0, 0.0, 0.0);

    assert(E502_StandConfigChannel(&dev, 0, 0, X502_LCH_MODE_COMM, X502_ADC_RANGE_10, 1) == X502_ERR_OK);
    assert(E502_StandConfigChannel(&dev, 1, 1, X502_LCH_MODE_DIFF, X502_ADC_RANGE_10, 1) == X502_ERR_OK);
    assert(E502_StandConfigChannel(&dev, 2, 2, X502_LCH_MODE_COMM, X502_ADC_RANGE_05, 1) == X502_ERR_OK);
    assert(E502_StandConfigChannel(&dev, 3, 0, X502_LCH_MODE_ZERO, X502_ADC_RANGE_10, 1) == X502_ERR_OK);
    assert(E502_StandConfigChannel(&dev, 4, 4, X502_LCH_MODE_COMM, X502_ADC_RANGE_5,  1) == X502_ERR_OK);
    assert(E502_StandConfigChannel(&dev, 5, 5, X502_LCH_MODE_COMM, X502_ADC_RANGE_5,  1) == X502_ERR_OK);

    assert(E502_StandWriteDac(&dev, X502_DAC_CH1,  3.25) == X502_ERR_OK);
    assert(E502_StandWriteDac(&dev, X502_DAC_CH2, -4.10) == X502_ERR_OK);

    double v[STAND_MAX_CHANNELS] = {0};
    /* Кадр 0 (t = 0.000 с): sin(0) = 0.0 В */
    assert(E502_StandReadAdcFrame(&dev, v, 500) == X502_ERR_OK);
    assert(fabs(v[0] - 0.0) < 0.01);
    assert(fabs(v[1] - 2.3) < 0.001);   /* DIFF: 3.5 - 1.2 = 2.3 В */
    assert(fabs(v[2] - 0.5) < 0.001);   /* Насыщение диапазона +/-0.5 В */
    assert(fabs(v[3]) < 0.001);         /* Режим ZERO ~ 0.0 В */
    assert(fabs(v[4] - 3.25) < 0.001);  /* Петля ЦАП1 -> АЦП */
    assert(fabs(v[5] + 4.10) < 0.001);  /* Петля ЦАП2 -> АЦП */

    /* Кадр 1 (t = 0.001 с = T/4 для 250 Гц): sin(pi/2) = +4.0 В */
    assert(E502_StandReadAdcFrame(&dev, v, 500) == X502_ERR_OK);
    assert(fabs(v[0] - 4.0) < 0.05);

    /* Кадр 2 (t = 0.002 с = T/2): sin(pi) = 0.0 В */
    assert(E502_StandReadAdcFrame(&dev, v, 500) == X502_ERR_OK);
    assert(fabs(v[0] - 0.0) < 0.05);

    /* Кадр 3 (t = 0.003 с = 3T/4): sin(3pi/2) = -4.0 В */
    assert(E502_StandReadAdcFrame(&dev, v, 500) == X502_ERR_OK);
    assert(fabs(v[0] + 4.0) < 0.05);

    /* Проверка аппаратного усреднения (avg=1 vs avg=128) на зашумленном сигнале */
    E502_SimSetPhySignal(6, E502_SIM_WAVE_DC, 0.0, 0.0, 1.5, 0.40);
    assert(E502_StandConfigChannel(&dev, 0, 6, X502_LCH_MODE_COMM, X502_ADC_RANGE_10, 1) == X502_ERR_OK);
    assert(E502_StandConfigChannel(&dev, 1, 6, X502_LCH_MODE_COMM, X502_ADC_RANGE_10, 128) == X502_ERR_OK);
    assert(E502_StandApplyConfig(&dev) == X502_ERR_OK);

    double err_no_avg = 0.0, err_avg128 = 0.0;
    for (int k = 0; k < 50; k++) {
        assert(E502_StandReadAdcFrame(&dev, v, 500) == X502_ERR_OK);
        err_no_avg += (v[0] - 1.5) * (v[0] - 1.5);
        err_avg128 += (v[1] - 1.5) * (v[1] - 1.5);
    }
    assert(err_avg128 < err_no_avg * 0.1); /* Дисперсия шума с avg=128 на порядок ниже */

    E502_StandDisconnect(&dev);
    printf("[PASS] test_synthetic_signals_and_dsp (RMS error avg=1: %.4f V, avg=128: %.4f V)\n",
           sqrt(err_no_avg / 50.0), sqrt(err_avg128 / 50.0));
}

int main(void) {
    printf("=== Running E-502 Stand Unit & Synthetic Signal Tests ===\n");
    test_lifecycle_and_leaks();
    test_bounds_and_error_integrity();
    test_synthetic_signals_and_dsp();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}

