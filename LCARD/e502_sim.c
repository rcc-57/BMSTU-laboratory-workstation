#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "e502_sim.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Динамический мост к реальным библиотекам драйвера L-Card (x502api.dll / e502api.dll) */
typedef t_x502_hnd (APIENTRY *fn_X502_Create)(void);
typedef int32_t    (APIENTRY *fn_X502_Free)(t_x502_hnd);
typedef int32_t    (APIENTRY *fn_X502_Close)(t_x502_hnd);
typedef int32_t    (APIENTRY *fn_E502_UsbGetSerialList)(char serials[][X502_SERIAL_SIZE], uint32_t, uint32_t, uint32_t*);
typedef int32_t    (APIENTRY *fn_E502_OpenUsb)(t_x502_hnd, const char*);
typedef int32_t    (APIENTRY *fn_E502_OpenByIpAddr)(t_x502_hnd, uint32_t, uint32_t, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_GetDevInfo)(t_x502_hnd, t_x502_info*);
typedef int32_t    (APIENTRY *fn_X502_SetMode)(t_x502_hnd, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_SetLChannelCount)(t_x502_hnd, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_SetLChannel)(t_x502_hnd, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_SetDigInPullup)(t_x502_hnd, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_Configure)(t_x502_hnd, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_AsyncGetAdcFrame)(t_x502_hnd, uint32_t, uint32_t, double*);
typedef int32_t    (APIENTRY *fn_X502_AsyncOutDac)(t_x502_hnd, uint32_t, double, uint32_t);
typedef int32_t    (APIENTRY *fn_X502_LedBlink)(t_x502_hnd);
typedef const char*(APIENTRY *fn_X502_GetErrorString)(int32_t);

typedef struct {
    bool loaded;
    bool probed;
    HMODULE hX502;
    HMODULE hE502;
    fn_X502_Create           pX502_Create;
    fn_X502_Free             pX502_Free;
    fn_X502_Close            pX502_Close;
    fn_E502_UsbGetSerialList pE502_UsbGetSerialList;
    fn_E502_OpenUsb          pE502_OpenUsb;
    fn_E502_OpenByIpAddr     pE502_OpenByIpAddr;
    fn_X502_GetDevInfo       pX502_GetDevInfo;
    fn_X502_SetMode          pX502_SetMode;
    fn_X502_SetLChannelCount pX502_SetLChannelCount;
    fn_X502_SetLChannel      pX502_SetLChannel;
    fn_X502_SetDigInPullup   pX502_SetDigInPullup;
    fn_X502_Configure        pX502_Configure;
    fn_X502_AsyncGetAdcFrame pX502_AsyncGetAdcFrame;
    fn_X502_AsyncOutDac      pX502_AsyncOutDac;
    fn_X502_LedBlink         pX502_LedBlink;
    fn_X502_GetErrorString   pX502_GetErrorString;
} E502_RealDllBridge;

static E502_RealDllBridge g_hw = {0};

static void probe_real_dlls(void) {
    if (g_hw.probed) return;
    g_hw.probed = true;
#ifdef _WIN32
    const char *x502_paths[] = {
        "x502api.dll",
        "C:\\Program Files\\L-Card\\lpcie\\bin\\x502api.dll",
        "C:\\Program Files (x86)\\L-Card\\lpcie\\bin\\x502api.dll"
    };
    const char *e502_paths[] = {
        "e502api.dll",
        "C:\\Program Files\\L-Card\\lpcie\\bin\\e502api.dll",
        "C:\\Program Files (x86)\\L-Card\\lpcie\\bin\\e502api.dll"
    };
    for (int i = 0; i < 3 && !g_hw.hX502; i++) g_hw.hX502 = LoadLibraryA(x502_paths[i]);
    for (int i = 0; i < 3 && !g_hw.hE502; i++) g_hw.hE502 = LoadLibraryA(e502_paths[i]);
    if (!g_hw.hX502 || !g_hw.hE502) return;

    g_hw.pX502_Create           = (fn_X502_Create)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_Create");
    g_hw.pX502_Free             = (fn_X502_Free)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_Free");
    g_hw.pX502_Close            = (fn_X502_Close)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_Close");
    g_hw.pX502_GetDevInfo       = (fn_X502_GetDevInfo)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_GetDevInfo");
    g_hw.pX502_SetMode          = (fn_X502_SetMode)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_SetMode");
    g_hw.pX502_SetLChannelCount = (fn_X502_SetLChannelCount)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_SetLChannelCount");
    g_hw.pX502_SetLChannel      = (fn_X502_SetLChannel)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_SetLChannel");
    g_hw.pX502_SetDigInPullup   = (fn_X502_SetDigInPullup)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_SetDigInPullup");
    g_hw.pX502_Configure        = (fn_X502_Configure)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_Configure");
    g_hw.pX502_AsyncGetAdcFrame = (fn_X502_AsyncGetAdcFrame)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_AsyncGetAdcFrame");
    g_hw.pX502_AsyncOutDac      = (fn_X502_AsyncOutDac)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_AsyncOutDac");
    g_hw.pX502_LedBlink         = (fn_X502_LedBlink)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_LedBlink");
    g_hw.pX502_GetErrorString   = (fn_X502_GetErrorString)(uintptr_t)GetProcAddress(g_hw.hX502, "X502_GetErrorString");
    g_hw.pE502_UsbGetSerialList = (fn_E502_UsbGetSerialList)(uintptr_t)GetProcAddress(g_hw.hE502, "E502_UsbGetSerialList");
    g_hw.pE502_OpenUsb          = (fn_E502_OpenUsb)(uintptr_t)GetProcAddress(g_hw.hE502, "E502_OpenUsb");
    g_hw.pE502_OpenByIpAddr     = (fn_E502_OpenByIpAddr)(uintptr_t)GetProcAddress(g_hw.hE502, "E502_OpenByIpAddr");

    if (g_hw.pX502_Create && g_hw.pX502_Free && g_hw.pX502_Close &&
        g_hw.pE502_UsbGetSerialList && g_hw.pE502_OpenUsb && g_hw.pE502_OpenByIpAddr &&
        g_hw.pX502_GetDevInfo && g_hw.pX502_SetMode && g_hw.pX502_SetLChannelCount &&
        g_hw.pX502_SetLChannel && g_hw.pX502_SetDigInPullup && g_hw.pX502_Configure &&
        g_hw.pX502_AsyncGetAdcFrame && g_hw.pX502_AsyncOutDac && g_hw.pX502_LedBlink) {
        g_hw.loaded = true;
    }
#endif
}

bool E502_SimHasRealDriver(void) {
    probe_real_dlls();
    return g_hw.loaded;
}

bool E502_SimIsSerialSimulated(const char *serial) {
    if (!serial || serial[0] == '\0') return false;
    return (strncmp(serial, "E502-SIM-", 9) == 0 || strncmp(serial, "E502-ETH-", 9) == 0);
}

/* Внутреннее представление описателя модуля st_x502 (гибрид: реальный драйвер или имитатор) */
struct st_x502 {
    bool is_open;
    bool is_real_hw;
    t_x502_hnd real_hnd;
    uint32_t mode;
    uint32_t lch_cnt;
    struct {
        uint32_t phy_ch;
        uint32_t mode;
        uint32_t range;
        uint32_t avg;
    } ltable[X502_LTABLE_MAX_CH_CNT];
    /* Текущая конфигурация, записанная в регистры ПЛИС вызовом X502_Configure */
    bool configured;
    uint32_t hw_lch_cnt;
    struct {
        uint32_t phy_ch;
        uint32_t mode;
        uint32_t range;
        uint32_t avg;
    } hw_ltable[X502_LTABLE_MAX_CH_CNT];
    uint32_t pullups;
    uint32_t hw_pullups;
    double dac_out[X502_DAC_CH_CNT];
    char serial[X502_SERIAL_SIZE];
};

static E502_SimPhyChannel g_phy_signals[X502_ADC_COMM_CH_CNT];
static bool g_sim_initialized = false;
static double g_sim_time_sec = 0.0;
static uint32_t g_rng_state = 0x12345678U;
static E502_SimStats g_stats = {0};
static int32_t g_fault_open_err = X502_ERR_OK;
static int32_t g_fault_config_err = X502_ERR_OK;
static double g_last_dac_volts[X502_DAC_CH_CNT] = {0.0, 0.0};

/* Псевдослучайный генератор Xorshift32 в диапазоне [-1.0, +1.0] */
static double sim_rand_bipolar(void) {
    uint32_t x = g_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng_state = x;
    return ((double)(x & 0xFFFFFFU) / (double)0x800000U) - 1.0;
}

static void ensure_sim_init(void) {
    if (g_sim_initialized) return;
    g_sim_initialized = true;
    memset(g_phy_signals, 0, sizeof(g_phy_signals));

    /* По умолчанию настраиваем тестовые искусственные сигналы на первых каналах:
       CH0: Синус 2.5 В, 50 Гц
       CH1: Меандр 1.8 В, 25 Гц
       CH2: Треугольник 4.0 В, 10 Гц
       CH3: Пила 3.0 В, 20 Гц
       CH4: Loopback с ЦАП1
       CH5: Loopback с ЦАП2 */
    g_phy_signals[0] = (E502_SimPhyChannel){E502_SIM_WAVE_SINE,     2.5, 50.0,  0.0, 0.0, 0.005};
    g_phy_signals[1] = (E502_SimPhyChannel){E502_SIM_WAVE_SQUARE,   1.8, 25.0,  0.2, 0.0, 0.005};
    g_phy_signals[2] = (E502_SimPhyChannel){E502_SIM_WAVE_TRIANGLE, 4.0, 10.0, -0.5, 0.0, 0.005};
    g_phy_signals[3] = (E502_SimPhyChannel){E502_SIM_WAVE_SAWTOOTH, 3.0, 20.0,  0.0, 0.0, 0.005};
    g_phy_signals[4] = (E502_SimPhyChannel){E502_SIM_WAVE_DAC1_LP,  1.0,  0.0,  0.0, 0.0, 0.001};
    g_phy_signals[5] = (E502_SimPhyChannel){E502_SIM_WAVE_DAC2_LP,  1.0,  0.0,  0.0, 0.0, 0.001};
    for (uint32_t i = 6; i < X502_ADC_COMM_CH_CNT; i++) {
        g_phy_signals[i] = (E502_SimPhyChannel){
            E502_SIM_WAVE_SINE,
            1.0 + 0.2 * (double)(i % 5),
            5.0 * (double)((i % 6) + 1),
            0.0,
            (double)i * 0.4,
            0.002
        };
    }
}

void E502_SimReset(void) {
    g_sim_initialized = false;
    g_sim_time_sec = 0.0;
    g_rng_state = 0x12345678U;
    g_fault_open_err = X502_ERR_OK;
    g_fault_config_err = X502_ERR_OK;
    g_last_dac_volts[0] = 0.0;
    g_last_dac_volts[1] = 0.0;
    memset(&g_stats, 0, sizeof(g_stats));
    ensure_sim_init();
}

void E502_SimSetPhySignal(uint32_t phy_ch, E502_SimWaveType type,
                          double amplitude, double freq_hz, double offset, double noise_amp) {
    ensure_sim_init();
    if (phy_ch >= X502_ADC_COMM_CH_CNT) return;
    g_phy_signals[phy_ch].type      = type;
    g_phy_signals[phy_ch].amplitude = amplitude;
    g_phy_signals[phy_ch].freq_hz   = freq_hz;
    g_phy_signals[phy_ch].offset    = offset;
    g_phy_signals[phy_ch].noise_amp = noise_amp;
}

void E502_SimGetPhySignal(uint32_t phy_ch, E502_SimPhyChannel *out_sig) {
    ensure_sim_init();
    if (phy_ch >= X502_ADC_COMM_CH_CNT || out_sig == NULL) return;
    *out_sig = g_phy_signals[phy_ch];
}

void E502_SimStepTime(double dt_sec) {
    ensure_sim_init();
    g_sim_time_sec += dt_sec;
}

double E502_SimGetTime(void) {
    ensure_sim_init();
    return g_sim_time_sec;
}

void E502_SimInjectFault(int32_t open_err, int32_t config_err) {
    ensure_sim_init();
    g_fault_open_err = open_err;
    g_fault_config_err = config_err;
}

void E502_SimGetStats(E502_SimStats *out_stats) {
    ensure_sim_init();
    if (out_stats != NULL) {
        *out_stats = g_stats;
    }
}

static double eval_phy_channel_sample(uint32_t phy_ch, double t) {
    if (phy_ch >= X502_ADC_COMM_CH_CNT) return 0.0;
    const E502_SimPhyChannel *sig = &g_phy_signals[phy_ch];
    double val = sig->offset;
    double phase = sig->freq_hz * t + (sig->phase_rad / (2.0 * M_PI));
    double frac = phase - floor(phase);

    switch (sig->type) {
        case E502_SIM_WAVE_SINE:
            val += sig->amplitude * sin(2.0 * M_PI * phase);
            break;
        case E502_SIM_WAVE_SQUARE:
            val += (frac < 0.5) ? sig->amplitude : -sig->amplitude;
            break;
        case E502_SIM_WAVE_TRIANGLE:
            val += sig->amplitude * (1.0 - 4.0 * fabs(frac - 0.5));
            break;
        case E502_SIM_WAVE_SAWTOOTH:
            val += sig->amplitude * (2.0 * frac - 1.0);
            break;
        case E502_SIM_WAVE_DC:
            break;
        case E502_SIM_WAVE_DAC1_LP:
            val += g_last_dac_volts[0];
            break;
        case E502_SIM_WAVE_DAC2_LP:
            val += g_last_dac_volts[1];
            break;
        default:
            break;
    }
    if (sig->noise_amp > 0.0) {
        val += sig->noise_amp * sim_rand_bipolar();
    }
    return val;
}

static double range_limit_volts(uint32_t range) {
    switch (range) {
        case X502_ADC_RANGE_10: return 10.0;
        case X502_ADC_RANGE_5:  return 5.0;
        case X502_ADC_RANGE_2:  return 2.0;
        case X502_ADC_RANGE_1:  return 1.0;
        case X502_ADC_RANGE_05: return 0.5;
        case X502_ADC_RANGE_02: return 0.2;
        default:                return 10.0;
    }
}

/* ==================== Реализация функций L-Card X502 / E502 API ==================== */

X502_EXPORT(t_x502_hnd) X502_Create(void) {
    ensure_sim_init();
    probe_real_dlls();
    t_x502_hnd hnd = (t_x502_hnd)calloc(1, sizeof(struct st_x502));
    if (hnd != NULL) {
        hnd->lch_cnt = 1;
        hnd->mode = X502_MODE_FPGA;
        g_stats.active_handles++;
    }
    return hnd;
}

X502_EXPORT(int32_t) X502_Free(t_x502_hnd hnd) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (hnd->is_open) {
        X502_Close(hnd);
    }
    if (hnd->real_hnd != NULL && g_hw.loaded) {
        g_hw.pX502_Free(hnd->real_hnd);
        hnd->real_hnd = NULL;
    }
    if (g_stats.active_handles > 0) {
        g_stats.active_handles--;
    }
    free(hnd);
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_Close(t_x502_hnd hnd) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (hnd->is_open) {
        if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
            g_hw.pX502_Close(hnd->real_hnd);
            g_hw.pX502_Free(hnd->real_hnd);
            hnd->real_hnd = NULL;
            hnd->is_real_hw = false;
        }
        hnd->is_open = false;
        if (g_stats.open_connections > 0) {
            g_stats.open_connections--;
        }
    }
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) E502_UsbGetSerialList(char serials[][X502_SERIAL_SIZE], uint32_t size,
                                           uint32_t flags, uint32_t *devcnt) {
    ensure_sim_init();
    probe_real_dlls();

    uint32_t hw_total = 0;
    uint32_t written = 0;
    if (g_hw.loaded) {
        int32_t hw_res = g_hw.pE502_UsbGetSerialList(serials, size, flags, &hw_total);
        if (hw_res > 0) {
            written = (uint32_t)hw_res;
        }
    }

    const char *sim_devs[2] = {"E502-SIM-0001", "E502-SIM-0002"};
    if (devcnt != NULL) {
        *devcnt = hw_total + 2U;
    }
    if (serials == NULL || size == 0) {
        return 0;
    }
    for (uint32_t i = 0; i < 2U && written < size; i++) {
        memset(serials[written], 0, X502_SERIAL_SIZE);
        strncpy(serials[written], sim_devs[i], X502_SERIAL_SIZE - 1);
        written++;
    }
    return (int32_t)written;
}

X502_EXPORT(int32_t) E502_OpenUsb(t_x502_hnd hnd, const char *serial) {
    ensure_sim_init();
    probe_real_dlls();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (hnd->is_open) return X502_ERR_ALREADY_OPENED;
    if (g_fault_open_err != X502_ERR_OK) return g_fault_open_err;

    bool want_sim = (serial != NULL && (strcmp(serial, "E502-SIM-0001") == 0 ||
                                        strcmp(serial, "E502-SIM-0002") == 0));

    /* Если выбран реальный серийный номер (не E502-SIM-*) и загружен драйвер L-Card */
    if (!want_sim && g_hw.loaded && serial != NULL && serial[0] != '\0') {
        t_x502_hnd rh = g_hw.pX502_Create();
        if (rh == NULL) return X502_ERR_MEMORY_ALLOC;
        int32_t err = g_hw.pE502_OpenUsb(rh, serial);
        if (err != X502_ERR_OK) {
            g_hw.pX502_Free(rh);
            return err;
        }
        hnd->real_hnd = rh;
        hnd->is_real_hw = true;
        strncpy(hnd->serial, serial, X502_SERIAL_SIZE - 1);
        hnd->serial[X502_SERIAL_SIZE - 1] = '\0';
        hnd->is_open = true;
        g_stats.open_connections++;
        return X502_ERR_OK;
    }

    if (serial != NULL && serial[0] != '\0') {
        if (!want_sim) {
            return X502_ERR_DEVICE_NOT_FOUND;
        }
        strncpy(hnd->serial, serial, X502_SERIAL_SIZE - 1);
    } else {
        strncpy(hnd->serial, "E502-SIM-0001", X502_SERIAL_SIZE - 1);
    }
    hnd->serial[X502_SERIAL_SIZE - 1] = '\0';
    hnd->is_real_hw = false;
    hnd->is_open = true;
    g_stats.open_connections++;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) E502_OpenByIpAddr(t_x502_hnd hnd, uint32_t ip_addr,
                                       uint32_t flags, uint32_t tout) {
    ensure_sim_init();
    probe_real_dlls();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (hnd->is_open) return X502_ERR_ALREADY_OPENED;
    if (g_fault_open_err != X502_ERR_OK) return g_fault_open_err;
    if (ip_addr == 0) return X502_ERR_HOST_UNREACHABLE;

    if (g_hw.loaded) {
        t_x502_hnd rh = g_hw.pX502_Create();
        if (rh != NULL) {
            int32_t err = g_hw.pE502_OpenByIpAddr(rh, ip_addr, flags, tout);
            if (err == X502_ERR_OK) {
                hnd->real_hnd = rh;
                hnd->is_real_hw = true;
                snprintf(hnd->serial, X502_SERIAL_SIZE, "E502-IP-%u.%u",
                         (ip_addr >> 8) & 0xFFU, ip_addr & 0xFFU);
                hnd->is_open = true;
                g_stats.open_connections++;
                return X502_ERR_OK;
            }
            g_hw.pX502_Free(rh);
        }
    }

    snprintf(hnd->serial, X502_SERIAL_SIZE, "E502-ETH-%u.%u",
             (ip_addr >> 8) & 0xFFU, ip_addr & 0xFFU);
    hnd->is_real_hw = false;
    hnd->is_open = true;
    g_stats.open_connections++;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_GetDevInfo(t_x502_hnd hnd, t_x502_info *info) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (info == NULL) return X502_ERR_INVALID_POINTER;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        return g_hw.pX502_GetDevInfo(hnd->real_hnd, info);
    }

    memset(info, 0, sizeof(t_x502_info));
    memcpy(info->name, "E502", 5);
    memcpy(info->serial, hnd->serial, X502_SERIAL_SIZE);
    info->serial[X502_SERIAL_SIZE - 1] = '\0';
    info->devflags = X502_DEVFLAGS_DAC_PRESENT | X502_DEVFLAGS_GAL_PRESENT |
                     X502_DEVFLAGS_IFACE_SUPPORT_USB | X502_DEVFLAGS_IFACE_SUPPORT_ETH;
    info->fpga_ver = (1U << 8) | 12U;
    info->plda_ver = 2U;
    info->board_rev = 3U;
    info->mcu_firmware_ver = (1U << 24) | (4U << 16) | (2U << 8) | 0U;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_SetMode(t_x502_hnd hnd, uint32_t mode) {
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        return g_hw.pX502_SetMode(hnd->real_hnd, mode);
    }
    if (mode != X502_MODE_FPGA && mode != X502_MODE_DSP && mode != X502_MODE_DEBUG) {
        return X502_ERR_INVALID_MODE;
    }
    hnd->mode = mode;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_SetLChannelCount(t_x502_hnd hnd, uint32_t lch_cnt) {
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        return g_hw.pX502_SetLChannelCount(hnd->real_hnd, lch_cnt);
    }
    if (lch_cnt == 0 || lch_cnt > X502_LTABLE_MAX_CH_CNT) {
        return X502_ERR_INVALID_LTABLE_SIZE;
    }
    hnd->lch_cnt = lch_cnt;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_SetLChannel(t_x502_hnd hnd, uint32_t lch, uint32_t phy_ch,
                                      uint32_t mode, uint32_t range, uint32_t avg) {
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        return g_hw.pX502_SetLChannel(hnd->real_hnd, lch, phy_ch, mode, range, avg);
    }
    if (lch >= X502_LTABLE_MAX_CH_CNT) return X502_ERR_INVALID_LCH_NUMBER;
    if (mode != X502_LCH_MODE_COMM && mode != X502_LCH_MODE_DIFF && mode != X502_LCH_MODE_ZERO) {
        return X502_ERR_INVALID_LCH_MODE;
    }
    if ((mode == X502_LCH_MODE_DIFF && phy_ch >= X502_ADC_DIFF_CH_CNT) ||
        (mode != X502_LCH_MODE_DIFF && phy_ch >= X502_ADC_COMM_CH_CNT)) {
        return X502_ERR_INVALID_LCH_PHY_NUMBER;
    }
    if (range > X502_ADC_RANGE_02) return X502_ERR_INVALID_LCH_RANGE;
    if (avg > X502_LCH_AVG_SIZE_MAX) return X502_ERR_INVALID_LCH_AVG_SIZE;

    hnd->ltable[lch].phy_ch = phy_ch;
    hnd->ltable[lch].mode   = mode;
    hnd->ltable[lch].range  = range;
    hnd->ltable[lch].avg    = avg;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_SetDigInPullup(t_x502_hnd hnd, uint32_t pullups) {
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        return g_hw.pX502_SetDigInPullup(hnd->real_hnd, pullups);
    }
    hnd->pullups = pullups;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_Configure(t_x502_hnd hnd, uint32_t flags) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (g_fault_config_err != X502_ERR_OK) return g_fault_config_err;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        int32_t err = g_hw.pX502_Configure(hnd->real_hnd, flags);
        if (err == X502_ERR_OK) g_stats.configure_calls++;
        return err;
    }

    hnd->hw_lch_cnt = hnd->lch_cnt;
    memcpy(hnd->hw_ltable, hnd->ltable, sizeof(hnd->ltable));
    hnd->hw_pullups = hnd->pullups;
    hnd->configured = true;
    g_stats.configure_calls++;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_AsyncGetAdcFrame(t_x502_hnd hnd, uint32_t flags,
                                           uint32_t tout, double *data) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (data == NULL) return X502_ERR_INVALID_POINTER;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        int32_t err = g_hw.pX502_AsyncGetAdcFrame(hnd->real_hnd, flags, tout, data);
        if (err == X502_ERR_OK) g_stats.adc_frames_read++;
        return err;
    }
    if (!hnd->configured || hnd->hw_lch_cnt == 0) return X502_ERR_INVALID_LTABLE_SIZE;

    /* Шаг времени между вызовами кадра (например, 0.5 мс = 2 кГц кадровая развертка) */
    const double sample_dt = 1.0 / 2000000.0; /* 2 МГц тактовая АЦП */
    for (uint32_t i = 0; i < hnd->hw_lch_cnt; i++) {
        uint32_t phy   = hnd->hw_ltable[i].phy_ch;
        uint32_t mode  = hnd->hw_ltable[i].mode;
        uint32_t range = hnd->hw_ltable[i].range;
        uint32_t avg   = hnd->hw_ltable[i].avg;
        if (avg == 0) avg = 1;

        double acc = 0.0;
        for (uint32_t k = 0; k < avg; k++) {
            double t_sample = g_sim_time_sec + (double)(i * avg + k) * sample_dt;
            double v = 0.0;
            if (mode == X502_LCH_MODE_ZERO) {
                v = 0.0001 * sim_rand_bipolar();
            } else if (mode == X502_LCH_MODE_DIFF) {
                uint32_t p_pos = phy % X502_ADC_DIFF_CH_CNT;
                uint32_t p_neg = p_pos + X502_ADC_DIFF_CH_CNT;
                v = eval_phy_channel_sample(p_pos, t_sample) - eval_phy_channel_sample(p_neg, t_sample);
            } else {
                v = eval_phy_channel_sample(phy % X502_ADC_COMM_CH_CNT, t_sample);
            }
            acc += v;
        }
        double measured = acc / (double)avg;
        double limit = range_limit_volts(range);
        if (measured > limit) measured = limit;
        if (measured < -limit) measured = -limit;
        data[i] = measured;
    }

    g_sim_time_sec += 0.001; /* Продвигаем модельное время на 1 мс за кадр */
    g_stats.adc_frames_read++;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_AsyncOutDac(t_x502_hnd hnd, uint32_t ch, double data, uint32_t flags) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (ch != X502_DAC_CH1 && ch != X502_DAC_CH2) return X502_ERR_INVALID_DAC_CHANNEL;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        return g_hw.pX502_AsyncOutDac(hnd->real_hnd, ch, data, flags);
    }

    double clamped = data;
    if (clamped > X502_DAC_RANGE) clamped = X502_DAC_RANGE;
    if (clamped < -X502_DAC_RANGE) clamped = -X502_DAC_RANGE;
    hnd->dac_out[ch] = clamped;
    g_last_dac_volts[ch] = clamped;
    return X502_ERR_OK;
}

X502_EXPORT(int32_t) X502_LedBlink(t_x502_hnd hnd) {
    ensure_sim_init();
    if (hnd == NULL) return X502_ERR_INVALID_HANDLE;
    if (!hnd->is_open) return X502_ERR_DEVICE_NOT_OPENED;
    if (hnd->is_real_hw && hnd->real_hnd != NULL && g_hw.loaded) {
        int32_t err = g_hw.pX502_LedBlink(hnd->real_hnd);
        if (err == X502_ERR_OK) g_stats.led_blinks++;
        return err;
    }
    g_stats.led_blinks++;
    return X502_ERR_OK;
}

X502_EXPORT(const char*) X502_GetErrorString(int32_t err) {
    if (g_hw.loaded && g_hw.pX502_GetErrorString != NULL) {
        const char *s = g_hw.pX502_GetErrorString(err);
        if (s != NULL) return s;
    }
    switch (err) {
        case X502_ERR_OK:                        return "OK (No error)";
        case X502_ERR_INVALID_HANDLE:            return "Invalid device handle";
        case X502_ERR_MEMORY_ALLOC:              return "Memory allocation failed";
        case X502_ERR_ALREADY_OPENED:            return "Device is already opened";
        case X502_ERR_DEVICE_NOT_FOUND:          return "Device not found";
        case X502_ERR_DEVICE_ACCESS_DENIED:      return "Device access denied";
        case X502_ERR_INVALID_POINTER:           return "Invalid pointer (NULL)";
        case X502_ERR_STREAM_IS_RUNNING:         return "Synchronous stream is running";
        case X502_ERR_FPGA_REG_WRITE:            return "FPGA register write error";
        case X502_ERR_DEVICE_NOT_OPENED:         return "Device is not opened";
        case X502_ERR_INVALID_OP_FOR_IFACE:      return "Invalid operation/flags for interface";
        case X502_ERR_INVALID_LTABLE_SIZE:       return "Invalid logical table size (1..32)";
        case X502_ERR_INVALID_LCH_NUMBER:        return "Invalid logical channel index";
        case X502_ERR_INVALID_LCH_RANGE:         return "Invalid ADC voltage range";
        case X502_ERR_INVALID_LCH_MODE:          return "Invalid logical channel mode";
        case X502_ERR_INVALID_LCH_PHY_NUMBER:    return "Invalid physical channel number";
        case X502_ERR_INVALID_LCH_AVG_SIZE:      return "Invalid channel averaging factor (0..128)";
        case X502_ERR_INVALID_MODE:              return "Invalid module operating mode";
        case X502_ERR_INVALID_DAC_CHANNEL:       return "Invalid DAC channel or voltage range (-5..+5 V)";
        case X502_ERR_HOST_UNREACHABLE:          return "Host unreachable";
        default:                                 return "Unknown X502 error code";
    }
}
