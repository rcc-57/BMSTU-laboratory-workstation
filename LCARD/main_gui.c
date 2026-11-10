#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "e502_ctrl.h"
#include "e502_sim.h"

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

#define ID_BTN_SCAN         101
#define ID_CMB_SERIAL       102
#define ID_BTN_CONN_USB     103
#define ID_BTN_CONN_IP      104
#define ID_BTN_DISCONN      105
#define ID_BTN_BLINK        106

#define ID_EDT_CH_CNT       110
#define ID_BTN_CH_CNT       111
#define ID_CMB_LCH          112
#define ID_EDT_PHY          113
#define ID_CMB_MODE         114
#define ID_CMB_RANGE        115
#define ID_EDT_AVG          116
#define ID_BTN_CH_SET       117
#define ID_EDT_PULLUP       118
#define ID_BTN_PULLUP       119
#define ID_BTN_APPLY        120

#define ID_EDT_DAC1         130
#define ID_BTN_DAC1         131
#define ID_EDT_DAC2         132
#define ID_BTN_DAC2         133

#define ID_CMB_SIM_PHY      140
#define ID_CMB_SIM_WAVE     141
#define ID_EDT_SIM_AMP      142
#define ID_EDT_SIM_FREQ     143
#define ID_EDT_SIM_OFFS     144
#define ID_EDT_SIM_NOISE    145
#define ID_BTN_SIM_SET      146

#define ID_BTN_ACQ_RUN      150
#define ID_BTN_ACQ_SINGLE   151
#define ID_BTN_ACQ_CLEAR    152

#define ID_CMB_LANG         160
#define ID_BTN_HELP_DLG     161
#define ID_BTN_HELP_HTML    162

#define ID_HELP_CLOSE       201
#define ID_HELP_OPEN_HTML   202
#define ID_HELP_NAV_LIST    203
#define ID_HELP_PREV        204
#define ID_HELP_NEXT        205
#define ID_HELP_SHOW_ALL    206

#define ID_TIMER_ACQ        1
#define ID_TIMER_SELFTEST   2

#define SCOPE_HISTORY_LEN   240
#define MAX_VISIBLE_TRACES  8

typedef enum {
    LANG_RU = 0,
    LANG_EN = 1
} AppLang;

static E502_StandDevice g_dev;
static AppLang g_lang = LANG_RU;
static bool g_acq_running = false;
static bool g_self_test_mode = false;
static bool g_capture_docs_mode = false;
static bool g_demo_mode = false;
static int  g_self_test_ticks = 0;
static uint32_t g_led_flash_ticks = 0;

/* Кольцевой буфер истории отсчетов для осциллографа */
static double g_trace_buf[STAND_MAX_CHANNELS][SCOPE_HISTORY_LEN];
static double g_last_frame[STAND_MAX_CHANNELS];
static uint32_t g_trace_head = 0;
static uint32_t g_trace_count = 0;

/* Дескрипторы элементов управления */
static HWND hMainWnd = NULL, hHelpWnd = NULL, hHelpList = NULL, hHelpEdit = NULL;
static HWND hHelpBtnPrev = NULL, hHelpBtnNext = NULL, hHelpBtnAll = NULL, hHelpBtnHtml = NULL, hHelpBtnClose = NULL;
static HWND hGrpConn, hGrpLch, hGrpDac, hGrpSim;
static HWND hBtnScan, hCmbSerial, hBtnConnUsb, hBtnConnIp, hBtnDisconn, hBtnBlink;
static HWND hLblStatus, hLblInfo, hLblStatusBar;
static HWND hLblChCnt, hEdtChCnt, hBtnChCnt, hLblLch, hCmbLch, hLblPhy, hEdtPhy, hLblAvg, hEdtAvg;
static HWND hLblMode, hCmbMode, hLblRange, hCmbRange, hBtnChSet, hLblPullup, hEdtPullup, hBtnPullup, hBtnApply;
static HWND hLblDac1, hEdtDac1, hBtnDac1, hLblDac2, hEdtDac2, hBtnDac2;
static HWND hLblSimPhy, hCmbSimPhy, hLblSimWave, hCmbSimWave;
static HWND hLblSimAmp, hEdtSimAmp, hLblSimFreq, hEdtSimFreq, hLblSimOffs, hEdtSimOffs, hLblSimNoise, hEdtSimNoise;
static HWND hBtnSimSet, hLblSimDefaults;
static HWND hBtnAcqRun, hBtnAcqSingle, hBtnAcqClear;
static HWND hLblLang, hCmbLang, hBtnHelpDlg, hBtnHelpHtml;
static HWND hListChannels;
static HFONT hFontUI, hFontMono, hFontBold;
static RECT g_rcScope = {350, 42, 1160, 432};

static const COLORREF g_trace_colors[MAX_VISIBLE_TRACES] = {
    RGB(56, 189, 248),  /* CH0: Голубой */
    RGB(74, 222, 128),  /* CH1: Зеленый */
    RGB(250, 204, 21),  /* CH2: Желтый */
    RGB(248, 113, 113), /* CH3: Красный */
    RGB(192, 132, 252), /* CH4: Фиолетовый */
    RGB(251, 146, 60),  /* CH5: Оранжевый */
    RGB(45, 212, 191),  /* CH6: Бирюзовый */
    RGB(244, 114, 182)  /* CH7: Розовый */
};

/* Выбор строки интерфейса по текущему языку (RU / EN) */
static const char* tr(const char *ru, const char *en) {
    return (g_lang == LANG_RU) ? ru : en;
}

/* Динамическое UTF-8 -> UTF-16 преобразование без усечения длинных текстов */
static void utf8_to_wide(const char *utf8, wchar_t *wbuf, int max_wchars) {
    if (!wbuf || max_wchars <= 0) return;
    if (!utf8) { wbuf[0] = L'\0'; return; }
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf, max_wchars);
    wbuf[max_wchars - 1] = L'\0';
}

static void set_text_u8(HWND hw, const char *utf8) {
    if (!hw) return;
    if (!utf8) {
        SetWindowTextW(hw, L"");
        return;
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (needed <= 0) return;
    if (needed <= 1024) {
        wchar_t sbuf[1024];
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, sbuf, 1024);
        SetWindowTextW(hw, sbuf);
    } else {
        wchar_t *dbuf = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
        if (!dbuf) return;
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, dbuf, needed);
        SetWindowTextW(hw, dbuf);
        free(dbuf);
    }
}

static void cb_add_u8(HWND hw, const char *utf8) {
    wchar_t wbuf[256];
    utf8_to_wide(utf8, wbuf, 256);
    SendMessageW(hw, CB_ADDSTRING, 0, (LPARAM)wbuf);
}

static void lb_add_u8(HWND hw, const char *utf8) {
    wchar_t wbuf[256];
    utf8_to_wide(utf8, wbuf, 256);
    SendMessageW(hw, LB_ADDSTRING, 0, (LPARAM)wbuf);
}

static void text_out_u8(HDC hdc, int x, int y, const char *utf8) {
    wchar_t wbuf[256];
    utf8_to_wide(utf8, wbuf, 256);
    TextOutW(hdc, x, y, wbuf, (int)wcslen(wbuf));
}

static const char* mode_short_str(uint32_t mode) {
    switch (mode) {
        case X502_LCH_MODE_COMM: return "COMM";
        case X502_LCH_MODE_DIFF: return "DIFF";
        case X502_LCH_MODE_ZERO: return "ZERO";
        default:                 return "UNK";
    }
}

static const char* range_short_str(uint32_t range) {
    switch (range) {
        case X502_ADC_RANGE_10: return "+/-10V";
        case X502_ADC_RANGE_5:  return "+/-5V";
        case X502_ADC_RANGE_2:  return "+/-2V";
        case X502_ADC_RANGE_1:  return "+/-1V";
        case X502_ADC_RANGE_05: return "+/-0.5V";
        case X502_ADC_RANGE_02: return "+/-0.2V";
        default:                return "UNK";
    }
}

static void set_status_msg(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    set_text_u8(hLblStatusBar, buf);
}

static void clear_scope_history(void) {
    memset(g_trace_buf, 0, sizeof(g_trace_buf));
    memset(g_last_frame, 0, sizeof(g_last_frame));
    g_trace_head = 0;
    g_trace_count = 0;
}

static void load_lch_fields(uint32_t lch) {
    if (lch >= STAND_MAX_CHANNELS) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", g_dev.lch_cfg[lch].phy_ch);
    SetWindowTextA(hEdtPhy, buf);
    SendMessageW(hCmbMode, CB_SETCURSEL, (WPARAM)g_dev.lch_cfg[lch].mode, 0);
    SendMessageW(hCmbRange, CB_SETCURSEL, (WPARAM)g_dev.lch_cfg[lch].range, 0);
    snprintf(buf, sizeof(buf), "%u", g_dev.lch_cfg[lch].avg);
    SetWindowTextA(hEdtAvg, buf);
}

static void load_sim_fields(uint32_t phy_ch) {
    E502_SimPhyChannel sig;
    E502_SimGetPhySignal(phy_ch, &sig);
    SendMessageW(hCmbSimWave, CB_SETCURSEL, (WPARAM)sig.type, 0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", sig.amplitude);
    SetWindowTextA(hEdtSimAmp, buf);
    snprintf(buf, sizeof(buf), "%.1f", sig.freq_hz);
    SetWindowTextA(hEdtSimFreq, buf);
    snprintf(buf, sizeof(buf), "%.2f", sig.offset);
    SetWindowTextA(hEdtSimOffs, buf);
    snprintf(buf, sizeof(buf), "%.4f", sig.noise_amp);
    SetWindowTextA(hEdtSimNoise, buf);
}

/* ==================== СТРУКТУРИРОВАННАЯ БАЗА СПРАВОЧНИКА (ТОЛЬКО РУССКИЙ ЯЗЫК) ==================== */

typedef struct {
    const char *nav_title;
    const char *content;
} HelpTopic;

static const HelpTopic g_help_topics[] = {
    {
        "Глава 1. Назначение и архитектура стенда",
        "================================================================================\r\n"
        "  ГЛАВА 1. НАЗНАЧЕНИЕ И АРХИТЕКТУРА ПРОГРАММНОГО КОМПЛЕКСА E-502\r\n"
        "================================================================================\r\n\r\n"
        "1.1. ОБЩИЕ СВЕДЕНИЯ\r\n"
        "Программный комплекс предназначен для автоматизации лабораторного стенда на базе\r\n"
        "16-разрядного модуля сбора данных L-Card E-502 (интерфейсы USB 2.0 и Ethernet).\r\n"
        "Комплекс обеспечивает управление логической таблицей АЦП (до 32 каналов на стенде),\r\n"
        "формирование аналоговых воздействий по двум каналам ЦАП (-5.0 .. +5.0 В),\r\n"
        "программную имитацию сигналов и цифровую осциллографию в реальном времени.\r\n\r\n"
        "1.2. СОСТАВ МОДУЛЕЙ КАТАЛОГА LCARD/\r\n"
        "  * e502_ctrl.h / e502_ctrl.c — библиотека управления модулем. Инкапсулирует\r\n"
        "    работу с дескриптором t_x502_hnd, контролирует границы параметров и выполняет\r\n"
        "    транзакционную запись конфигурации в регистры ПЛИС (FPGA).\r\n"
        "  * e502_sim.h / e502_sim.c   — математический имитатор аппаратуры E-502.\r\n"
        "    Генерирует синтетические сигналы (синус, меандр, треугольник, пила, DC,\r\n"
        "    петли ЦАП1/ЦАП2), моделирует дифференциальные входы и аппаратное усреднение.\r\n"
        "  * main_gui.c (stand_gui.exe)— графическая рабочая станция оператора.\r\n"
        "  * main_cli.c (stand_cli.exe)— интерактивный консольный терминал.\r\n"
        "  * test_stand.c              — модульные и интеграционные автотесты.\r\n\r\n"
        "1.3. ЖИЗНЕННЫЙ ЦИКЛ ДЕСКРИПТОРА И ЗАЩИТА РЕСУРСОВ\r\n"
        "  1) Инициализация структуры E502_StandInitStruct(&dev) выставляет сигнатуру\r\n"
        "     E502_STAND_MAGIC (0x45353032) и исходную таблицу каналов.\r\n"
        "  2) При вызове E502_StandConnectUSB() / E502_StandConnectIP():\r\n"
        "     - если предыдущий дескриптор открыт, автоматически вызывается Disconnect;\r\n"
        "     - создается дескриптор X502_Create() и открывается связь с устройством;\r\n"
        "     - считывается паспорт X502_GetDevInfo() и вызывается E502_StandApplyConfig();\r\n"
        "     - при любом сбое инициализации выполняется гарантированный откат\r\n"
        "       (X502_Close + X502_Free), исключающий утечку памяти и блокировку USB.\r\n"
    },
    {
        "Глава 2. Описание кнопок и элементов GUI",
        "================================================================================\r\n"
        "  ГЛАВА 2. ОПИСАНИЕ ФУНКЦИОНАЛЬНОСТИ ГРАФИЧЕСКОГО ИНТЕРФЕЙСА (UI)\r\n"
        "================================================================================\r\n\r\n"
        "ВЕРХНЯЯ ПАНЕЛЬ ИНСТРУМЕНТОВ:\r\n"
        "  * [Язык / Lang]               — переключение языка подписей окна (RU / EN).\r\n"
        "  * [? Руководство (в окне)]    — открытие данного интерактивного справочника.\r\n"
        "  * [Открыть USER_GUIDE.html]   — открытие полного иллюстрированного руководства\r\n"
        "                                  в браузере (с фотографиями окон и калькулятором).\r\n\r\n"
        "БЛОК 1. ПОДКЛЮЧЕНИЕ И СТАТУС МОДУЛЯ:\r\n"
        "  * [Поиск USB]                 — вызывает E502_StandListUSB(), сканирует шину USB\r\n"
        "                                  и заполняет выпадающий список серийных номеров.\r\n"
        "  * Поле [Серийный номер]       — выбор найденного устройства (напр. E502-SIM-0001).\r\n"
        "  * [Подкл. USB]                — вызывает E502_StandConnectUSB(), открывает модуль,\r\n"
        "                                  активирует 6 логических каналов и синхронизирует ПЛИС.\r\n"
        "  * [Подкл. IP (Sim)]           — вызывает E502_StandConnectIP() по адресу 192.168.1.100.\r\n"
        "  * [Отключить]                 — останавливает опрос и вызывает E502_StandDisconnect().\r\n"
        "  * [Тест LED1]                 — вызывает E502_StandBlinkLed(), мигает индикатором\r\n"
        "                                  LED1 на лицевой панели модуля и в углу осциллографа.\r\n\r\n"
        "БЛОК 2. ЛОГИЧЕСКАЯ ТАБЛИЦА АЦП И ПОДТЯЖКА:\r\n"
        "  * Поле [Активных каналов] + кнопка [Задать число]:\r\n"
        "    Вызывает E502_StandSetChannelCount(dev, count) (допустимо 1..32). Переводит\r\n"
        "    статус ПЛИС в состояние 'ИЗМЕНЕНО (нужен Apply)'.\r\n"
        "  * Списки [Канал LCH], [Физ.вх], [Уср], [Режим], [Диапаз.] + кнопка\r\n"
        "    [Применить настройки к каналу LCH]:\r\n"
        "    Вызывает E502_StandConfigChannel(). Проверяет совместимость номера физического\r\n"
        "    канала с режимом (0..15 для DIFF, 0..31 для COMM) и усреднение (0..128).\r\n"
        "  * Поле [Подтяжка Hex] + кнопка [Задать маску]:\r\n"
        "    Вызывает E502_StandSetPullups() для включения резисторов подтяжки (0x00..0x3F).\r\n"
        "  * [Записать конфигурацию в ПЛИС (Apply)]:\r\n"
        "    Вызывает E502_StandApplyConfig() -> переносит всю таблицу в регистры ПЛИС.\r\n\r\n"
        "БЛОК 3. АНАЛОГОВЫЕ ВЫХОДЫ ЦАП (-5.0 .. +5.0 В):\r\n"
        "  * Поля [ЦАП1 (В)] / [ЦАП2 (В)] + кнопки [Выдать на ЦАП1] / [Выдать на ЦАП2]:\r\n"
        "    Вызывают E502_StandWriteDac() с калибровкой X502_DAC_FLAGS_CALIBR.\r\n\r\n"
        "БЛОК 4. ГЕНЕРАТОР ТЕСТОВЫХ СИГНАЛОВ (ИМИТАТОР):\r\n"
        "  * Позволяет для любого физического входа (Phy 0..31) задать форму сигнала\r\n"
        "    (Синус, Меандр, Треугольник, Пила, DC, Петля с ЦАП1/ЦАП2), амплитуду,\r\n"
        "    частоту, постоянное смещение и уровень шума кнопкой [Обновить сигнал].\r\n\r\n"
        "ПРАВАЯ ПАНЕЛЬ (ОСЦИЛЛОГРАФ И ТАБЛИЦА ТЕЛЕМЕТРИИ):\r\n"
        "  * [Старт опроса (20 Гц)]      — запуск/останов периодического чтения кадров АЦП.\r\n"
        "  * [Одиночный кадр]            — однократный вызов E502_StandReadAdcFrame().\r\n"
        "  * [Очистить экран]            — сброс кольцевого буфера осциллограмм.\r\n"
    },
    {
        "Глава 3. Конвейер ПЛИС, режимы и формулы",
        "================================================================================\r\n"
        "  ГЛАВА 3. КОНВЕЙЕР КОНФИГУРИРОВАНИЯ ПЛИС, РЕЖИМЫ АЦП И РАСЧЕТНЫЕ СООТНОШЕНИЯ\r\n"
        "================================================================================\r\n\r\n"
        "3.1. ПОСЛЕДОВАТЕЛЬНОСТЬ ЗАПИСИ НАСТРОЕК В МОДУЛЬ\r\n"
        "Модуль E-502 требует строгой очередности вызовов API перед чтением данных:\r\n"
        "  1. X502_SetMode(hnd, X502_MODE_FPGA)          — перевод в режим ПЛИС;\r\n"
        "  2. X502_SetLChannelCount(hnd, lch_count)      — размер таблицы (1..32);\r\n"
        "  3. X502_SetLChannel(hnd, i, phy, mode, r, avg)— запись параметров слотов;\r\n"
        "  4. X502_SetDigInPullup(hnd, pullups_mask)     — настройка подтяжки линий;\r\n"
        "  5. X502_Configure(hnd, 0)                     — транзакция в регистры ПЛИС.\r\n\r\n"
        "3.2. РЕЖИМЫ ИЗМЕРЕНИЯ КАНАЛОВ АЦП\r\n"
        "  * X502_LCH_MODE_COMM (0) — измерение относительно общей земли AGND.\r\n"
        "    Допустимые физические каналы: phy_ch = 0 .. 31.\r\n"
        "  * X502_LCH_MODE_DIFF (1) — дифференциальное измерение между линиями X и Y.\r\n"
        "    Допустимые физические каналы: phy_ch = 0 .. 15.\r\n"
        "    Измеряемое напряжение: U_diff = U(phy_ch) - U(phy_ch + 16).\r\n"
        "  * X502_LCH_MODE_ZERO (2) — подключение входа АЦП к общей точке внутри модуля\r\n"
        "    для контроля смещения нуля.\r\n\r\n"
        "3.3. ПОДДИАПАЗОНЫ ВХОДНОГО УСИЛИТЕЛЯ (PGA)\r\n"
        "  * Код 0 (X502_ADC_RANGE_10): +/- 10.0 В\r\n"
        "  * Код 1 (X502_ADC_RANGE_5) : +/-  5.0 В\r\n"
        "  * Код 2 (X502_ADC_RANGE_2) : +/-  2.0 В\r\n"
        "  * Код 3 (X502_ADC_RANGE_1) : +/-  1.0 В\r\n"
        "  * Код 4 (X502_ADC_RANGE_05): +/-  0.5 В\r\n"
        "  * Код 5 (X502_ADC_RANGE_02): +/-  0.2 В\r\n\r\n"
        "3.4. АППАРАТНОЕ УСРЕДНЕНИЕ В ПЛИС И РАСЧЕТ СКЗ (VRMS)\r\n"
        "При задании коэффициента усреднения avg = N (1..128) ПЛИС вычисляет среднее:\r\n"
        "  U_avg = (1 / N) * Sum_{k=1..N} U_k\r\n"
        "При этом среднеквадратическое отклонение белого шума уменьшается в sqrt(N) раз.\r\n"
        "В таблице телеметрии GUI для каждого канала по окну из M = 240 кадров вычисляется\r\n"
        "среднеквадратическое значение (СКЗ / Vrms):\r\n"
        "  V_rms = sqrt( (1 / M) * Sum_{j=1..M} (U_j)^2 )\r\n"
    },
    {
        "Глава 4. Методика лабораторной работы",
        "================================================================================\r\n"
        "  ГЛАВА 4. МЕТОДИКА ПРОВЕДЕНИЯ ЛАБОРАТОРНЫХ ИСПЫТАНИЙ\r\n"
        "================================================================================\r\n\r\n"
        "ОПЫТ 1. ПОДКЛЮЧЕНИЕ И МНОГОКАНАЛЬНОЕ ОСЦИЛЛОГРАФИРОВАНИЕ\r\n"
        "  1. В Блоке 1 нажмите [Поиск USB], выберите 'E502-SIM-0001' -> [Подкл. USB].\r\n"
        "  2. Убедитесь, что статус изменился на 'ПОДКЛЮЧЕНО', а ПЛИС — 'СИНХРОНИЗИРОВАНО'.\r\n"
        "  3. Нажмите [Старт опроса (20 Гц)] под экраном осциллографа.\r\n"
        "  4. Наблюдайте на экране и в таблице 6 каналов: LCH0 (Синус 2.5 В), LCH1 (Меандр),\r\n"
        "     LCH2 (Треугольник), LCH3 (Пила), LCH4 (Петля ЦАП1), LCH5 (Петля ЦАП2).\r\n\r\n"
        "ОПЫТ 2. ПРОВЕРКА ПЕТЛИ ОБРАТНОЙ СВЯЗИ ЦАП -> АЦП\r\n"
        "  1. В Блоке 3 введите в поле [ЦАП1 (В)] значение 3.250 и нажмите [Выдать на ЦАП1].\r\n"
        "  2. В поле [ЦАП2 (В)] введите -4.100 и нажмите [Выдать на ЦАП2].\r\n"
        "  3. По таблице телеметрии убедитесь, что на канале LCH4 (Физ.вх 4) установилось\r\n"
        "     напряжение +3.2500 В, а на канале LCH5 (Физ.вх 5) — напряжение -4.1000 В.\r\n\r\n"
        "ОПЫТ 3. ИССЛЕДОВАНИЕ ДИФФЕРЕНЦИАЛЬНОГО ВХОДА (DIFF)\r\n"
        "  1. В Блоке 4 выберите 'Phy 0', Форма = 'Постоянное (DC)', Смещ = 3.50 В ->\r\n"
        "     нажмите [Обновить сигнал на физическом входе].\r\n"
        "  2. Выберите 'Phy 16', Форма = 'Постоянное (DC)', Смещ = 1.20 В -> [Обновить].\r\n"
        "  3. В Блоке 2 выберите 'LCH 0', Физ.вх = 0, Режим = '1: DIFF' -> нажмите\r\n"
        "     [Применить настройки к каналу LCH], затем [Записать конфигурацию в ПЛИС].\r\n"
        "  4. Убедитесь, что канал LCH0 измеряет разность: 3.50 - 1.20 = +2.3000 В.\r\n\r\n"
        "ОПЫТ 4. ПОДАВЛЕНИЕ ШУМА АППАРАТНЫМ УСРЕДНЕНИЕМ (AVG)\r\n"
        "  1. В Блоке 4 для 'Phy 0' установите Шум = 0.40 В и нажмите [Обновить сигнал].\r\n"
        "  2. Сравните размах колебаний (МАКС - МИН) в таблице при Уср = 1 и при Уср = 128\r\n"
        "     (после изменения Уср нажимайте [Применить к каналу] и [Записать в ПЛИС]).\r\n"
    },
    {
        "Глава 5. Команды консольного режима (CLI)",
        "================================================================================\r\n"
        "  ГЛАВА 5. СПРАВОЧНИК КОМАНД КОНСОЛЬНОГО ИНТЕРФЕЙСА (STAND_CLI.EXE)\r\n"
        "================================================================================\r\n\r\n"
        "  scan                           - Поиск модулей E-502 на шине USB\r\n"
        "  open [serial]                  - Подключение по USB (первый доступный или S/N)\r\n"
        "  info (или status)              - Вывод паспорта модуля и таблицы каналов\r\n"
        "  ch_cnt <N>                     - Установка числа активных каналов (1..32)\r\n"
        "  ch_set <lch> <phy> <m> <r> <a> - Настройка логического канала <lch>:\r\n"
        "                                   <lch>: номер слота (0 .. lch_count-1)\r\n"
        "                                   <phy>: физ. вход (0..31 COMM, 0..15 DIFF)\r\n"
        "                                   <m>  : режим (0=COMM, 1=DIFF, 2=ZERO)\r\n"
        "                                   <r>  : диапазон (0=10V, 1=5V, 2=2V, 3=1V,\r\n"
        "                                                    4=0.5V, 5=0.2V)\r\n"
        "                                   <a>  : усреднение (0=авто, 1..128)\r\n"
        "  pullup <hex_mask>              - Маска резисторов подтяжки (0x00..0x3F)\r\n"
        "  apply                          - Запись настроек в регистры ПЛИС\r\n"
        "  adc [count]                    - Чтение 1 или [count] (1..10000) кадров АЦП\r\n"
        "  dac <1|2> <volts>              - Выдача напряжения (-5.0..+5.0 В) на ЦАП1/2\r\n"
        "  blink                          - Индикация светодиодом LED1\r\n"
        "  close                          - Отключение от модуля и освобождение hnd\r\n"
        "  help                           - Вывод справки по командам\r\n"
        "  exit (или quit)                - Безопасное отключение и выход\r\n"
    },
    {
        "Функция: E502_StandInitStruct",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandInitStruct\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  void E502_StandInitStruct(E502_StandDevice *dev);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Выполняет начальную инициализацию контекста стенда, обнуляет поля структуры,\r\n"
        "  устанавливает 1 активный логический канал и формирует настройки по умолчанию\r\n"
        "  для всех 32 слотов таблицы (режим COMM, диапазон +/-10 В, avg = 0).\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev [out] (E502_StandDevice*) — указатель на инициализируемую структуру\r\n"
        "    контекста устройства. Если передан NULL, функция безопасно завершает работу.\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  Нет (void). Поле dev->config_dirty устанавливается в true, dev->init_magic =\r\n"
        "  E502_STAND_MAGIC (0x45353032U), dev->last_error = X502_ERR_OK (0).\r\n"
    },
    {
        "Функция: E502_StandListUSB",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandListUSB\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandListUSB(char serials[][X502_SERIAL_SIZE],\r\n"
        "                        uint32_t max_count, uint32_t *found_count);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Опрашивает шину USB через E502_UsbGetSerialList() и заполняет массив серийных\r\n"
        "  номеров обнаруженных модулей E-502.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * serials     [out] (char[][32]) — двумерный буфер для записи строк серийных\r\n"
        "                                     номеров (может быть NULL при max_count == 0).\r\n"
        "  * max_count   [in]  (uint32_t)   — максимальная вместимость массива serials.\r\n"
        "  * found_count [out] (uint32_t*)  — указатель, куда записывается число реально\r\n"
        "                                     сохраненных номеров (гарантированно <= max_count).\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0)               — успешное выполнение;\r\n"
        "  * X502_ERR_INVALID_POINTER (-7) — если serials == NULL при max_count > 0;\r\n"
        "  * Отрицательный код t_x502_errs — при аппаратной ошибке драйвера USB.\r\n"
    },
    {
        "Функция: E502_StandConnectUSB",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandConnectUSB\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandConnectUSB(E502_StandDevice *dev, const char *serial);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Создает описатель X502_Create(), устанавливает эксклюзивное USB-соединение\r\n"
        "  через E502_OpenUsb(), считывает паспорт модуля X502_GetDevInfo() и применяет\r\n"
        "  начальную конфигурацию ПЛИС вызовом E502_StandApplyConfig().\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev    [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n"
        "  * serial [in]     (const char*)       — строка с серийным номером (до 31 симв.)\r\n"
        "                                          либо NULL / \"\" для подключения к первому\r\n"
        "                                          доступному модулю на шине USB.\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0)                — связь установлена и ПЛИС сконфигурирована;\r\n"
        "  * X502_ERR_INVALID_POINTER (-7)  — dev == NULL;\r\n"
        "  * X502_ERR_MEMORY_ALLOC (-2)     — ошибка выделения памяти в X502_Create();\r\n"
        "  * X502_ERR_DEVICE_NOT_FOUND (-4) — модуль с указанным S/N не найден.\r\n"
        "  При любой ошибке дескриптор автоматически закрывается и освобождается.\r\n"
    },
    {
        "Функция: E502_StandConnectIP",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandConnectIP\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandConnectIP(E502_StandDevice *dev, uint32_t ip_addr, uint32_t tout_ms);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Устанавливает соединение с модулем E-502 по сети Ethernet (TCP/IP) через\r\n"
        "  функцию E502_OpenByIpAddr(), загружает информацию о плате и синхронизирует ПЛИС.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev     [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n"
        "  * ip_addr [in]     (uint32_t)          — IPv4-адрес в хостовом порядке байт:\r\n"
        "                                           (a << 24) | (b << 16) | (c << 8) | d.\r\n"
        "  * tout_ms [in]     (uint32_t)          — таймаут ожидания подключения в мс.\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0) или отрицательный код ошибки из перечисления t_x502_errs.\r\n"
    },
    {
        "Функция: E502_StandDisconnect",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandDisconnect\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  void E502_StandDisconnect(E502_StandDevice *dev);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Безопасно закрывает активное соединение с модулем (X502_Close), освобождает\r\n"
        "  память описателя (X502_Free), обнуляет указатель dev->hnd = NULL и сбрасывает\r\n"
        "  флаг dev->is_connected = false.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  Нет (void). Повторный вызов для уже отключенного устройства безопасен (no-op).\r\n"
    },
    {
        "Функция: E502_StandSetChannelCount",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandSetChannelCount\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandSetChannelCount(E502_StandDevice *dev, uint32_t count);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Задает количество активных логических каналов в таблице опроса АЦП и взводит\r\n"
        "  флаг необходимости перезаписи ПЛИС (dev->config_dirty = true).\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev   [in/out] (E502_StandDevice*) — указатель на открытый контекст модуля.\r\n"
        "  * count [in]     (uint32_t)          — число каналов в диапазоне 1 .. 32\r\n"
        "                                         (STAND_MAX_CHANNELS).\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0)                   — количество каналов успешно обновлено;\r\n"
        "  * X502_ERR_DEVICE_NOT_OPENED (-31)  — модуль не подключен;\r\n"
        "  * X502_ERR_INVALID_LTABLE_SIZE(-102)— count == 0 или count > 32.\r\n"
    },
    {
        "Функция: E502_StandConfigChannel",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandConfigChannel\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandConfigChannel(E502_StandDevice *dev, uint32_t lch,\r\n"
        "                              uint32_t phy_ch, uint32_t mode,\r\n"
        "                              uint32_t range, uint32_t avg);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Проверяет допустимость параметров и настраивает один слот логической таблицы АЦП.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev    [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n"
        "  * lch    [in]     (uint32_t)          — индекс логического канала (0 .. lch_count-1).\r\n"
        "  * phy_ch [in]     (uint32_t)          — номер физического входа АЦП:\r\n"
        "                                          0..31 для режима COMM, 0..15 для DIFF.\r\n"
        "  * mode   [in]     (uint32_t)          — режим из t_x502_lch_mode:\r\n"
        "                                          0 (COMM), 1 (DIFF), 2 (ZERO).\r\n"
        "  * range  [in]     (uint32_t)          — диапазон из t_x502_adc_range (0..5):\r\n"
        "                                          0=10В, 1=5В, 2=2В, 3=1В, 4=0.5В, 5=0.2В.\r\n"
        "  * avg    [in]     (uint32_t)          — коэффициент усреднения (0 или 1..128).\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0)                    — успешная настройка слота;\r\n"
        "  * X502_ERR_INVALID_LCH_NUMBER (-103) — lch >= dev->lch_count или lch >= 32;\r\n"
        "  * X502_ERR_INVALID_LCH_PHY_NUMBER(-106) — выход phy_ch за границы режима;\r\n"
        "  * X502_ERR_INVALID_LCH_MODE (-105)   — неизвестный режим;\r\n"
        "  * X502_ERR_INVALID_LCH_RANGE (-104)  — неизвестный код диапазона;\r\n"
        "  * X502_ERR_INVALID_LCH_AVG_SIZE(-107)— avg > 128.\r\n"
    },
    {
        "Функция: E502_StandSetPullups",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandSetPullups\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandSetPullups(E502_StandDevice *dev, uint32_t pullups_mask);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Задает битовую маску подтягивающих резисторов цифровых и синхровходов.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev          [in/out] (E502_StandDevice*) — указатель на контекст модуля.\r\n"
        "  * pullups_mask [in]     (uint32_t)          — побитовая комбинация флагов\r\n"
        "                                                t_x502_pullups (0x00 .. 0x3F:\r\n"
        "                                                DI_H=0x01, DI_L=0x02, SYN1=0x04,\r\n"
        "                                                SYN2=0x08, CONV_IN=0x10, START_IN=0x20).\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0), X502_ERR_DEVICE_NOT_OPENED (-31) или\r\n"
        "    X502_ERR_INVALID_OP_FOR_IFACE (-32) при недопустимых битах маски.\r\n"
    },
    {
        "Функция: E502_StandApplyConfig",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandApplyConfig\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandApplyConfig(E502_StandDevice *dev);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Выполняет полный цикл загрузки параметров из структуры dev в аппаратные\r\n"
        "  регистры ПЛИС: X502_SetMode(FPGA) -> X502_SetLChannelCount -> цикл\r\n"
        "  X502_SetLChannel -> X502_SetDigInPullup -> X502_Configure(hnd, 0).\r\n"
        "  При успехе сбрасывает флаг dev->config_dirty = false.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0) или код ошибки ПЛИС/валидации.\r\n"
    },
    {
        "Функция: E502_StandReadAdcFrame",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandReadAdcFrame\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandReadAdcFrame(E502_StandDevice *dev, double *volts_buf, uint32_t tout_ms);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Считывает один полный кадр отсчетов АЦП (в Вольтах) по всем активным логическим\r\n"
        "  каналам через X502_AsyncGetAdcFrame(). Если настройки менялись до чтения\r\n"
        "  (dev->config_dirty == true), автоматически вызывает E502_StandApplyConfig().\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev       [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n"
        "  * volts_buf [out]    (double*)           — выходной буфер емкостью не менее\r\n"
        "                                             dev->lch_count элементов типа double.\r\n"
        "  * tout_ms   [in]     (uint32_t)          — таймаут ожидания кадра в мс.\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0), X502_ERR_INVALID_POINTER (-7), X502_ERR_DEVICE_NOT_OPENED (-31).\r\n"
    },
    {
        "Функция: E502_StandWriteDac",
        "================================================================================\r\n"
        "  ФУНКЦИЯ: E502_StandWriteDac\r\n"
        "================================================================================\r\n\r\n"
        "ПРОТОТИП (e502_ctrl.h):\r\n"
        "  int E502_StandWriteDac(E502_StandDevice *dev, uint32_t dac_ch, double volts);\r\n\r\n"
        "НАЗНАЧЕНИЕ:\r\n"
        "  Устанавливает выходное напряжение на указанном канале ЦАП через функцию\r\n"
        "  X502_AsyncOutDac() с флагами X502_DAC_FLAGS_VOLT | X502_DAC_FLAGS_CALIBR.\r\n\r\n"
        "АРГУМЕНТЫ:\r\n"
        "  * dev    [in/out] (E502_StandDevice*) — указатель на контекст устройства.\r\n"
        "  * dac_ch [in]     (uint32_t)          — канал ЦАП: X502_DAC_CH1 (0) или\r\n"
        "                                          X502_DAC_CH2 (1).\r\n"
        "  * volts  [in]     (double)            — конечное напряжение в диапазоне\r\n"
        "                                          от -5.000 В до +5.000 В (не NaN/Inf).\r\n\r\n"
        "ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:\r\n"
        "  * X502_ERR_OK (0)                   — напряжение успешно выставлено;\r\n"
        "  * X502_ERR_INVALID_DAC_CHANNEL(-111)— неверный канал или выход за +/-5.0 В.\r\n"
    },
    {
        "Функция: E502_StandBlinkLed / GetErrorStr",
        "================================================================================\r\n"
        "  ФУНКЦИИ: E502_StandBlinkLed и E502_StandGetErrorStr\r\n"
        "================================================================================\r\n\r\n"
        "1. ПРОТОТИП:\r\n"
        "  int E502_StandBlinkLed(E502_StandDevice *dev);\r\n"
        "  * Назначение: Вызывает X502_LedBlink(dev->hnd) для визуальной идентификации\r\n"
        "    модуля миганием светодиода LED1 на передней панели.\r\n"
        "  * Аргумент: dev [in/out] — контекст открытого устройства.\r\n"
        "  * Возвращает: X502_ERR_OK (0) или X502_ERR_DEVICE_NOT_OPENED (-31).\r\n\r\n"
        "2. ПРОТОТИП:\r\n"
        "  const char* E502_StandGetErrorStr(const E502_StandDevice *dev);\r\n"
        "  * Назначение: Возвращает текстовое описание последнего кода ошибки\r\n"
        "    dev->last_error через X502_GetErrorString().\r\n"
        "  * Аргумент: dev [in] — константный указатель на контекст устройства.\r\n"
        "  * Возвращает: ненулевой указатель на строку с описанием ошибки.\r\n"
    },
    {
        "Функции имитатора: E502_Sim*",
        "================================================================================\r\n"
        "  ФУНКЦИИ ИМИТАТОРА ИСКУССТВЕННЫХ СИГНАЛОВ (E502_SIM.H)\r\n"
        "================================================================================\r\n\r\n"
        "1. void E502_SimReset(void);\r\n"
        "   Сбрасывает модельное время, генератор шума, счетчики дескрипторов и\r\n"
        "   восстанавливает исходные тестовые сигналы на физических каналах 0..31.\r\n\r\n"
        "2. void E502_SimSetPhySignal(uint32_t phy_ch, E502_SimWaveType type,\r\n"
        "                             double amplitude, double freq_hz,\r\n"
        "                             double offset, double noise_amp);\r\n"
        "   Аргументы:\r\n"
        "   * phy_ch    [in] (0..31) — номер физического канала аналогового входа;\r\n"
        "   * type      [in]         — тип сигнала (SINE=0, SQUARE=1, TRIANGLE=2,\r\n"
        "                              SAWTOOTH=3, DC=4, DAC1_LP=5, DAC2_LP=6);\r\n"
        "   * amplitude [in] (double)— амплитуда переменной составляющей, В;\r\n"
        "   * freq_hz   [in] (double)— частота сигнала, Гц;\r\n"
        "   * offset    [in] (double)— постоянное смещение (DC), В;\r\n"
        "   * noise_amp [in] (double)— амплитуда равномерного белого шума, В.\r\n\r\n"
        "3. void E502_SimGetPhySignal(uint32_t phy_ch, E502_SimPhyChannel *out_sig);\r\n"
        "   Считывает текущие настройки генератора для физического входа phy_ch (0..31).\r\n\r\n"
        "4. void E502_SimInjectFault(int32_t open_err, int32_t config_err);\r\n"
        "   Инжектирует искусственные коды ошибок в E502_OpenUsb/IP и X502_Configure\r\n"
        "   для тестирования устойчивости к сбоям и отсутствия утечек дескрипторов.\r\n\r\n"
        "5. void E502_SimGetStats(E502_SimStats *out_stats);\r\n"
        "   Возвращает статистику: active_handles, open_connections, configure_calls,\r\n"
        "   adc_frames_read, led_blinks.\r\n"
    }
};

#define HELP_TOPIC_COUNT ((int)(sizeof(g_help_topics) / sizeof(g_help_topics[0])))

static void show_help_topic(int idx) {
    if (!hHelpEdit || !hHelpList) return;
    if (idx < 0 || idx >= HELP_TOPIC_COUNT) {
        /* Показать все главы единым полным документом без обрывов */
        size_t total_len = 128;
        for (int i = 0; i < HELP_TOPIC_COUNT; i++) {
            total_len += strlen(g_help_topics[i].content) + 8;
        }
        char *all_buf = (char*)malloc(total_len);
        if (!all_buf) return;
        all_buf[0] = '\0';
        for (int i = 0; i < HELP_TOPIC_COUNT; i++) {
            strcat(all_buf, g_help_topics[i].content);
            strcat(all_buf, "\r\n\r\n");
        }
        set_text_u8(hHelpEdit, all_buf);
        SendMessageW(hHelpEdit, EM_SETSEL, 0, 0);
        SendMessageW(hHelpEdit, EM_SCROLLCARET, 0, 0);
        free(all_buf);
        return;
    }
    SendMessageW(hHelpList, LB_SETCURSEL, (WPARAM)idx, 0);
    set_text_u8(hHelpEdit, g_help_topics[idx].content);
    SendMessageW(hHelpEdit, EM_SETSEL, 0, 0);
    SendMessageW(hHelpEdit, EM_SCROLLCARET, 0, 0);
}

static void draw_oscilloscope(HDC hdc, const RECT *rc);

static void save_hwnd_bmp(HWND hw, bool client_only, const char *filename) {
    if (!hw) return;
    RECT rc;
    if (client_only) GetClientRect(hw, &rc);
    else GetWindowRect(hw, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hbm && bits) {
        HBITMAP old = (HBITMAP)SelectObject(hdcMem, hbm);
        RedrawWindow(hw, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASENOW);
        BOOL ok = PrintWindow(hw, hdcMem, client_only ? PW_CLIENTONLY : 0);
        if (!ok) {
            SendMessageW(hw, WM_PRINT, (WPARAM)hdcMem,
                         PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND | (client_only ? 0 : PRF_NONCLIENT));
        }
        if (hw == hMainWnd) {
            RECT scopeRc = g_rcScope;
            if (!client_only) {
                POINT pt = {0, 0};
                ClientToScreen(hw, &pt);
                int dx = pt.x - rc.left;
                int dy = pt.y - rc.top;
                OffsetRect(&scopeRc, dx, dy);
            }
            draw_oscilloscope(hdcMem, &scopeRc);
        }
        uint32_t stride = ((uint32_t)w * 3U + 3U) & ~3U;
        uint32_t img_size = stride * (uint32_t)h;
        BITMAPFILEHEADER bfh;
        memset(&bfh, 0, sizeof(bfh));
        bfh.bfType    = 0x4D42;
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bfh.bfSize    = bfh.bfOffBits + img_size;
        FILE *f = fopen(filename, "wb");
        if (f) {
            fwrite(&bfh, sizeof(bfh), 1, f);
            fwrite(&bmi.bmiHeader, sizeof(bmi.bmiHeader), 1, f);
            fwrite(bits, 1, img_size, f);
            fclose(f);
        }
        SelectObject(hdcMem, old);
        DeleteObject(hbm);
    }
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

static void open_html_manual(HWND owner) {
    wchar_t exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    wchar_t *last_slash = wcsrchr(exe_path, L'\\');
    if (last_slash) *(last_slash + 1) = L'\0';

    wchar_t html_path[MAX_PATH] = {0};
    wcsncpy(html_path, exe_path, MAX_PATH - 1);
    wcsncat(html_path, L"USER_GUIDE.html", MAX_PATH - wcslen(html_path) - 1);

    HINSTANCE res = ShellExecuteW(owner, L"open", html_path, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)res <= 32) {
        ShellExecuteW(owner, L"open", L"USER_GUIDE.html", NULL, NULL, SW_SHOWNORMAL);
    }
}

static LRESULT CALLBACK HelpWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HWND hLblNav = CreateWindowExW(0, L"STATIC", L"Оглавление и функции:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                12, 10, 285, 18, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hLblNav, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            hHelpList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                12, 32, 285, 505, hwnd, (HMENU)(intptr_t)ID_HELP_NAV_LIST, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpList, WM_SETFONT, (WPARAM)hFontUI, TRUE);

            for (int i = 0; i < HELP_TOPIC_COUNT; i++) {
                lb_add_u8(hHelpList, g_help_topics[i].nav_title);
            }

            hHelpBtnPrev = CreateWindowExW(0, L"BUTTON", L"<< Пред. раздел",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                308, 6, 130, 24, hwnd, (HMENU)(intptr_t)ID_HELP_PREV, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpBtnPrev, WM_SETFONT, (WPARAM)hFontUI, TRUE);

            hHelpBtnNext = CreateWindowExW(0, L"BUTTON", L"След. раздел >>",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                444, 6, 130, 24, hwnd, (HMENU)(intptr_t)ID_HELP_NEXT, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpBtnNext, WM_SETFONT, (WPARAM)hFontUI, TRUE);

            hHelpBtnAll = CreateWindowExW(0, L"BUTTON", L"Показать все разделы целиком",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                580, 6, 220, 24, hwnd, (HMENU)(intptr_t)ID_HELP_SHOW_ALL, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpBtnAll, WM_SETFONT, (WPARAM)hFontUI, TRUE);

            hHelpEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                308, 32, 660, 505, hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpEdit, WM_SETFONT, (WPARAM)hFontMono, TRUE);
            /* Снимаем стандартный лимит длины текста для поля EDIT */
            SendMessageW(hHelpEdit, EM_SETLIMITTEXT, 0, 0);

            hHelpBtnHtml = CreateWindowExW(0, L"BUTTON",
                L"Открыть полное интерактивное руководство в браузере (USER_GUIDE.html)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                12, 546, 560, 30, hwnd, (HMENU)(intptr_t)ID_HELP_OPEN_HTML, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpBtnHtml, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            hHelpBtnClose = CreateWindowExW(0, L"BUTTON", L"Закрыть окно справки",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                808, 546, 160, 30, hwnd, (HMENU)(intptr_t)ID_HELP_CLOSE, GetModuleHandle(NULL), NULL);
            SendMessageW(hHelpBtnClose, WM_SETFONT, (WPARAM)hFontUI, TRUE);

            show_help_topic(0);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == ID_HELP_NAV_LIST && code == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(hHelpList, LB_GETCURSEL, 0, 0);
                if (sel >= 0) show_help_topic(sel);
            } else if (id == ID_HELP_PREV) {
                int sel = (int)SendMessageW(hHelpList, LB_GETCURSEL, 0, 0);
                if (sel > 0) show_help_topic(sel - 1);
                else show_help_topic(0);
            } else if (id == ID_HELP_NEXT) {
                int sel = (int)SendMessageW(hHelpList, LB_GETCURSEL, 0, 0);
                if (sel + 1 < HELP_TOPIC_COUNT) show_help_topic(sel + 1);
            } else if (id == ID_HELP_SHOW_ALL) {
                SendMessageW(hHelpList, LB_SETCURSEL, (WPARAM)-1, 0);
                show_help_topic(-1);
            } else if (id == ID_HELP_OPEN_HTML) {
                open_html_manual(hwnd);
            } else if (id == ID_HELP_CLOSE) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_DESTROY:
            hHelpWnd = NULL;
            hHelpList = NULL;
            hHelpEdit = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void show_help_dialog(HWND parent) {
    if (hHelpWnd != NULL && IsWindow(hHelpWnd)) {
        SetForegroundWindow(hHelpWnd);
        return;
    }
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc = {0};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = HelpWndProc;
        wc.hInstance     = GetModuleHandle(NULL);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"E502HelpManualWnd";
        RegisterClassExW(&wc);
        reg = true;
    }
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    RECT rc = {0, 0, 980, 588};
    AdjustWindowRect(&rc, style, FALSE);
    hHelpWnd = CreateWindowExW(0, L"E502HelpManualWnd",
                               L"Интерактивное справочное руководство — АПК L-Card E-502",
                               style, CW_USEDEFAULT, CW_USEDEFAULT,
                               rc.right - rc.left, rc.bottom - rc.top,
                               parent, NULL, GetModuleHandle(NULL), NULL);
    ShowWindow(hHelpWnd, SW_SHOW);
    UpdateWindow(hHelpWnd);
}

static void update_ui_state(HWND hwnd) {
    char buf[320];
    if (g_dev.is_connected) {
        snprintf(buf, sizeof(buf),
                 tr("СТАТУС: ПОДКЛЮЧЕНО | ПЛИС: %s | Подт: 0x%02X",
                    "STATUS: ONLINE | FPGA: %s | Pull-ups: 0x%02X"),
                 g_dev.config_dirty
                     ? tr("ИЗМЕНЕНО", "DIRTY")
                     : tr("СИНХРОН.", "SYNCED"),
                 g_dev.pullups_mask);
        set_text_u8(hLblStatus, buf);

        bool is_sim = E502_SimIsSerialSimulated(g_dev.dev_info.serial);
        snprintf(buf, sizeof(buf),
                 tr("Тип: %s | S/N: %.15s | ПЛИС: v%u.%u",
                    "Mode: %s | S/N: %.15s | FPGA: v%u.%u"),
                 is_sim ? tr("ИМИТАТОР", "SIMULATOR") : tr("РЕАЛЬНЫЙ E502", "REAL E502"),
                 g_dev.dev_info.serial,
                 (g_dev.dev_info.fpga_ver >> 8) & 0xFF, g_dev.dev_info.fpga_ver & 0xFF);
        set_text_u8(hLblInfo, buf);
    } else {
        set_text_u8(hLblStatus, tr("СТАТУС: ОТКЛЮЧЕНО (Нет активного соединения)",
                                   "STATUS: OFFLINE (Disconnected)"));
        set_text_u8(hLblInfo, tr("Модель: --  |  S/N: --  |  ПЛИС: --  |  Рев.: --",
                                 "Model: --  |  S/N: --  |  FPGA: --  |  Rev: --"));
    }

    SendMessageW(hListChannels, LB_RESETCONTENT, 0, 0);
    lb_add_u8(hListChannels,
              tr(" LCH | ВХ. | РЕЖ. | ДИАПАЗ. | УСР |   ТЕКУЩЕЕ (В) |     МИН (В) |    МАКС (В) |    СКЗ Vrms",
                 " LCH | PHY | MODE | RANGE   | AVG |   CURRENT (V) |     MIN (V) |     MAX (V) |    VRMS (V)"));
    lb_add_u8(hListChannels,
              "-----+-----+------+---------+-----+---------------+-------------+-------------+------------");

    uint32_t ch_limit = (g_dev.lch_count <= STAND_MAX_CHANNELS) ? g_dev.lch_count : STAND_MAX_CHANNELS;
    for (uint32_t c = 0; c < ch_limit; c++) {
        double vmin = g_last_frame[c], vmax = g_last_frame[c], sum_sq = 0.0;
        if (g_trace_count > 0) {
            vmin = 1e9;
            vmax = -1e9;
            for (uint32_t k = 0; k < g_trace_count; k++) {
                uint32_t idx = (g_trace_head + SCOPE_HISTORY_LEN - g_trace_count + k) % SCOPE_HISTORY_LEN;
                double v = g_trace_buf[c][idx];
                if (v < vmin) vmin = v;
                if (v > vmax) vmax = v;
                sum_sq += v * v;
            }
            sum_sq = sqrt(sum_sq / (double)g_trace_count);
        }
        char row[160];
        snprintf(row, sizeof(row),
                 "  %2u |  %2u | %-4s | %-7s | %3u |   %+10.4f  |  %+9.4f  |  %+9.4f  |  %9.4f",
                 c,
                 g_dev.lch_cfg[c].phy_ch,
                 mode_short_str(g_dev.lch_cfg[c].mode),
                 range_short_str(g_dev.lch_cfg[c].range),
                 g_dev.lch_cfg[c].avg,
                 g_last_frame[c], vmin, vmax, sum_sq);
        lb_add_u8(hListChannels, row);
    }

    InvalidateRect(hwnd, &g_rcScope, FALSE);
}

static void apply_language(HWND hwnd) {
    set_text_u8(hwnd, tr("МГТУ им. Н.Э. Баумана — Лабораторный стенд L-Card E-502 (Управление и Имитатор)",
                         "BMSTU — L-Card E-502 Laboratory Workstation (Control & Signal Simulator)"));

    set_text_u8(hLblLang, tr("Язык / Lang:", "Language:"));
    set_text_u8(hBtnHelpDlg, tr("? Руководство (в окне)", "? In-App Manual (RU)"));
    set_text_u8(hBtnHelpHtml, tr("Открыть USER_GUIDE.html", "Open USER_GUIDE.html"));

    set_text_u8(hGrpConn, tr("1. Подключение и статус модуля", "1. Device Connection & Status"));
    set_text_u8(hBtnScan, tr("Поиск USB", "Scan USB"));
    set_text_u8(hBtnConnUsb, tr("Подкл. USB", "Connect USB"));
    set_text_u8(hBtnConnIp, tr("Подкл. IP (Sim)", "Connect IP (Sim)"));
    set_text_u8(hBtnDisconn, tr("Отключить", "Disconnect"));
    set_text_u8(hBtnBlink, tr("Тест LED1", "Blink LED1"));

    set_text_u8(hGrpLch, tr("2. Логическая таблица АЦП и подтяжка", "2. Logical Channel Table & Pull-ups"));
    set_text_u8(hLblChCnt, tr("Активных каналов (1..32):", "Active Channels (1..32):"));
    set_text_u8(hBtnChCnt, tr("Задать число", "Set Count"));
    set_text_u8(hLblLch, tr("Канал:", "LCH #:"));
    set_text_u8(hLblPhy, tr("Физ.вх(0..31):", "Phy (0..31):"));
    set_text_u8(hLblAvg, tr("Уср:", "Avg:"));
    set_text_u8(hLblMode, tr("Режим:", "Mode:"));
    set_text_u8(hLblRange, tr("Диапаз.:", "Range:"));

    int cur_mode = (int)SendMessageW(hCmbMode, CB_GETCURSEL, 0, 0);
    if (cur_mode < 0) cur_mode = 0;
    SendMessageW(hCmbMode, CB_RESETCONTENT, 0, 0);
    cb_add_u8(hCmbMode, tr("0: COMM (Общ.земля)", "0: COMM (Gnd)"));
    cb_add_u8(hCmbMode, tr("1: DIFF (Дифференц.)", "1: DIFF"));
    cb_add_u8(hCmbMode, tr("2: ZERO (Калибр. 0)", "2: ZERO"));
    SendMessageW(hCmbMode, CB_SETCURSEL, (WPARAM)cur_mode, 0);

    set_text_u8(hBtnChSet, tr("Применить настройки к каналу LCH", "Set Channel Config"));
    set_text_u8(hLblPullup, tr("Подтяжка Hex (0..3F):", "Pull-up Hex (0..3F):"));
    set_text_u8(hBtnPullup, tr("Задать маску", "Set Pull-ups"));
    set_text_u8(hBtnApply, tr("Записать конфигурацию в ПЛИС (Apply)", "Apply Configuration to FPGA"));

    set_text_u8(hGrpDac, tr("3. Аналоговые выходы ЦАП (-5.0 .. +5.0 В)", "3. Analog Outputs (DAC +/-5.0 V)"));
    set_text_u8(hLblDac1, tr("ЦАП1 (В):", "DAC1 (V):"));
    set_text_u8(hBtnDac1, tr("Выдать на ЦАП1", "Write DAC1"));
    set_text_u8(hLblDac2, tr("ЦАП2 (В):", "DAC2 (V):"));
    set_text_u8(hBtnDac2, tr("Выдать на ЦАП2", "Write DAC2"));

    set_text_u8(hGrpSim, tr("4. Генератор тестовых сигналов (Имитатор)", "4. Synthetic Signal Generator (Simulator)"));
    set_text_u8(hLblSimPhy, tr("Физ.вх:", "Phy In:"));
    set_text_u8(hLblSimWave, tr("Форма:", "Wave:"));

    int cur_wave = (int)SendMessageW(hCmbSimWave, CB_GETCURSEL, 0, 0);
    if (cur_wave < 0) cur_wave = 0;
    SendMessageW(hCmbSimWave, CB_RESETCONTENT, 0, 0);
    cb_add_u8(hCmbSimWave, tr("Синусоида (Sine)", "Sine Wave"));
    cb_add_u8(hCmbSimWave, tr("Меандр (Square)", "Square Wave"));
    cb_add_u8(hCmbSimWave, tr("Треугольник (Triangle)", "Triangle Wave"));
    cb_add_u8(hCmbSimWave, tr("Пила (Sawtooth)", "Sawtooth Wave"));
    cb_add_u8(hCmbSimWave, tr("Постоянное (DC)", "DC Voltage"));
    cb_add_u8(hCmbSimWave, tr("Петля с ЦАП1 (Loopback)", "DAC1 Loopback"));
    cb_add_u8(hCmbSimWave, tr("Петля с ЦАП2 (Loopback)", "DAC2 Loopback"));
    SendMessageW(hCmbSimWave, CB_SETCURSEL, (WPARAM)cur_wave, 0);

    set_text_u8(hLblSimAmp, tr("Ампл (В):", "Amp (V):"));
    set_text_u8(hLblSimFreq, tr("Част (Гц):", "Freq (Hz):"));
    set_text_u8(hLblSimOffs, tr("Смещ (В):", "Offset (V):"));
    set_text_u8(hLblSimNoise, tr("Шум (В):", "Noise (V):"));
    set_text_u8(hBtnSimSet, tr("Обновить сигнал на физическом входе", "Update Synthetic Signal on Phy Input"));
    set_text_u8(hLblSimDefaults, tr("Вх0=Синус, Вх1=Меандр, Вх2=Треуг, Вх4/5=ЦАП1/2",
                                    "Defaults: Phy0=Sine, Phy1=Square, Phy4/5=DAC1/2"));

    set_text_u8(hBtnAcqRun, g_acq_running
        ? tr("Стоп опроса", "Stop Polling")
        : tr("Старт опроса (20 Гц)", "Start Polling (20 Hz)"));
    set_text_u8(hBtnAcqSingle, tr("Одиночный кадр", "Single Frame"));
    set_text_u8(hBtnAcqClear, tr("Очистить экран", "Clear Oscilloscope"));

    update_ui_state(hwnd);
}

static void acquire_single_frame(HWND hwnd) {
    if (!g_dev.is_connected) {
        set_status_msg(tr("[ОШИБКА] Модуль не подключен.", "[ERROR] Device is not opened."));
        return;
    }
    double frame[STAND_MAX_CHANNELS] = {0};
    int err = E502_StandReadAdcFrame(&g_dev, frame, 500);
    if (err != X502_ERR_OK) {
        g_acq_running = false;
        set_text_u8(hBtnAcqRun, tr("Старт опроса (20 Гц)", "Start Polling (20 Hz)"));
        set_status_msg(tr("[ОШИБКА] Сбой чтения АЦП: %s (код %d)", "[ERROR] ADC Read failed: %s (code %d)"),
                       E502_StandGetErrorStr(&g_dev), err);
        return;
    }
    for (uint32_t c = 0; c < g_dev.lch_count && c < STAND_MAX_CHANNELS; c++) {
        g_last_frame[c] = frame[c];
        g_trace_buf[c][g_trace_head] = frame[c];
    }
    g_trace_head = (g_trace_head + 1) % SCOPE_HISTORY_LEN;
    if (g_trace_count < SCOPE_HISTORY_LEN) g_trace_count++;
    update_ui_state(hwnd);
}

static void draw_oscilloscope(HDC hdc, const RECT *rc) {
    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;
    if (w <= 20 || h <= 20) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    RECT localRc = {0, 0, w, h};
    HBRUSH bgBrush = CreateSolidBrush(RGB(18, 22, 28));
    FillRect(memDC, &localRc, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(65, 75, 90));
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, 0, 0, w, h);

    int padL = 52, padR = 14, padT = 28, padB = 24;
    int plotW = w - padL - padR;
    int plotH = h - padT - padB;

    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(38, 46, 58));
    HPEN zeroPen = CreatePen(PS_SOLID, 1, RGB(75, 90, 110));
    SetBkMode(memDC, TRANSPARENT);
    SelectObject(memDC, hFontMono);
    SetTextColor(memDC, RGB(148, 163, 184));

    for (int i = 0; i <= 8; i++) {
        int y = padT + (plotH * i) / 8;
        SelectObject(memDC, (i == 4) ? zeroPen : gridPen);
        MoveToEx(memDC, padL, y, NULL);
        LineTo(memDC, padL + plotW, y);

        double v_label = 10.0 - 2.5 * i;
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%+5.1fV", v_label);
        text_out_u8(memDC, 4, y - 7, lbl);
    }

    SelectObject(memDC, gridPen);
    for (int j = 0; j <= 10; j++) {
        int x = padL + (plotW * j) / 10;
        MoveToEx(memDC, x, padT, NULL);
        LineTo(memDC, x, padT + plotH);
    }

    SetTextColor(memDC, RGB(226, 232, 240));
    text_out_u8(memDC, padL, 6,
                tr("ОСЦИЛЛОГРАММА СИГНАЛОВ АЦП (-10 В .. +10 В)",
                   "ADC WAVEFORM MONITOR (-10V .. +10V)"));

    COLORREF ledCol = !g_dev.is_connected ? RGB(70, 70, 75) :
                      (g_led_flash_ticks > 0 ? RGB(239, 68, 68) : RGB(34, 197, 94));
    HBRUSH ledBrush = CreateSolidBrush(ledCol);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, ledBrush);
    Ellipse(memDC, w - 85, 7, w - 71, 21);
    SelectObject(memDC, oldBrush);
    DeleteObject(ledBrush);
    text_out_u8(memDC, w - 66, 6, "LED1");

    if (g_trace_count >= 2 && g_dev.is_connected) {
        uint32_t ch_draw = g_dev.lch_count;
        if (ch_draw > MAX_VISIBLE_TRACES) ch_draw = MAX_VISIBLE_TRACES;

        for (uint32_t c = 0; c < ch_draw; c++) {
            HPEN tracePen = CreatePen(PS_SOLID, 2, g_trace_colors[c]);
            SelectObject(memDC, tracePen);

            for (uint32_t k = 0; k < g_trace_count; k++) {
                uint32_t idx = (g_trace_head + SCOPE_HISTORY_LEN - g_trace_count + k) % SCOPE_HISTORY_LEN;
                double v = g_trace_buf[c][idx];
                if (v > 10.0) v = 10.0;
                if (v < -10.0) v = -10.0;

                int x = padL + (int)((k * (uint32_t)plotW) / (SCOPE_HISTORY_LEN - 1));
                int y = padT + (int)(((10.0 - v) / 20.0) * (double)plotH);
                if (k == 0) MoveToEx(memDC, x, y, NULL);
                else        LineTo(memDC, x, y);
            }
            DeleteObject(tracePen);

            SetTextColor(memDC, g_trace_colors[c]);
            char leg[32];
            snprintf(leg, sizeof(leg), "LCH%u:%+6.2fV", c, g_last_frame[c]);
            text_out_u8(memDC, padL + (int)c * 92, padT + plotH + 4, leg);
        }
    } else {
        SetTextColor(memDC, RGB(148, 163, 184));
        const char *hint = g_dev.is_connected
            ? tr("Нажмите «Старт опроса (20 Гц)» или «Одиночный кадр» для запуска измерений",
                 "Press 'Start Polling' or 'Single Frame' to acquire ADC signals")
            : tr("Подключите модуль E-502 (USB или IP) в разделе 1 для начала работы",
                 "Connect to E502 USB/IP module in Section 1 to start acquisition");
        text_out_u8(memDC, padL + 20, padT + plotH / 2 - 8, hint);
    }

    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    DeleteObject(gridPen);
    DeleteObject(zeroPen);

    BitBlt(hdc, rc->left, rc->top, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

static HWND create_ctrl(HWND parent, const wchar_t *cls, const char *txt_u8, DWORD style,
                        int x, int y, int w, int h, int id, HFONT font) {
    wchar_t wtxt[256];
    utf8_to_wide(txt_u8, wtxt, 256);
    HWND hw = CreateWindowExW(0, cls, wtxt, WS_CHILD | WS_VISIBLE | style,
                              x, y, w, h, parent, (HMENU)(intptr_t)id, GetModuleHandle(NULL), NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    return hw;
}

#define LCARD_DRIVER_URL "https://www.lcard.ru/download/lpcie_setup.exe"

static void prompt_driver_download(HWND hwnd) {
    if (g_self_test_mode || E502_SimHasRealDriver()) return;
    const char *msg_u8 = tr(
        "Системные драйверы L-Card (x502api.dll / e502api.dll) не обнаружены на этом ПК.\n"
        "Сейчас программа работает в режиме виртуального имитатора (E502-SIM-0001).\n\n"
        "Для подключения реального аппаратного модуля L-Card E-502 скачайте и установите официальный пакет драйверов:\n"
        LCARD_DRIVER_URL "\n\n"
        "Перейти по ссылке для скачивания драйвера прямо сейчас?",
        "L-Card system drivers (x502api.dll / e502api.dll) were not found on this PC.\n"
        "The application is currently running in virtual simulator mode (E502-SIM-0001).\n\n"
        "To connect a real L-Card E-502 hardware unit, please download and install the official driver package:\n"
        LCARD_DRIVER_URL "\n\n"
        "Open the download link in your browser now?"
    );
    const char *title_u8 = tr(
        "Драйвер L-Card не найден — Установка lpcie_setup.exe",
        "L-Card Driver Not Found — Install lpcie_setup.exe"
    );
    wchar_t wmsg[1024], wtitle[128];
    utf8_to_wide(msg_u8, wmsg, 1024);
    utf8_to_wide(title_u8, wtitle, 128);
    if (MessageBoxW(hwnd, wmsg, wtitle, MB_YESNO | MB_ICONWARNING) == IDYES) {
        ShellExecuteW(hwnd, L"open", L"https://www.lcard.ru/download/lpcie_setup.exe",
                      NULL, NULL, SW_SHOWNORMAL);
    }
}

static void populate_usb_list(void) {
    SendMessageW(hCmbSerial, CB_RESETCONTENT, 0, 0);
    char serials[8][X502_SERIAL_SIZE];
    uint32_t found = 0;
    if (E502_StandListUSB(serials, 8, &found) == X502_ERR_OK && found > 0) {
        uint32_t real_cnt = 0;
        for (uint32_t i = 0; i < found && i < 8; i++) {
            cb_add_u8(hCmbSerial, serials[i]);
            if (!E502_SimIsSerialSimulated(serials[i])) real_cnt++;
        }
        SendMessageW(hCmbSerial, CB_SETCURSEL, 0, 0);
        if (E502_SimHasRealDriver()) {
            set_status_msg(tr("[OK] Поиск USB: физических плат E-502: %u, имитаторов: %u (драйвер L-Card загружен).",
                              "[OK] USB scan: physical E-502 boards: %u, simulators: %u (L-Card driver loaded)."),
                           real_cnt, found - real_cnt);
        } else {
            set_status_msg(tr("[ВНИМАНИЕ] Драйвер L-Card не найден (режим имитации). Скачать драйвер: %s",
                              "[WARN] L-Card driver not found (simulator mode). Download driver: %s"),
                           LCARD_DRIVER_URL);
        }
    } else {
        cb_add_u8(hCmbSerial, "E502-SIM-0001");
        SendMessageW(hCmbSerial, CB_SETCURSEL, 0, 0);
        set_status_msg(tr("[ВНИМАНИЕ] Модули USB не найдены. Скачать драйвер: %s",
                          "[WARN] No USB devices found. Download driver: %s"),
                       LCARD_DRIVER_URL);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hFontUI   = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontBold = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontMono = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    FIXED_PITCH | FF_MODERN, L"Consolas");

            /* Верхняя панель: выбор языка и открытие документации */
            hLblLang = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 350, 14, 90, 20, 0, hFontBold);
            hCmbLang = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWNLIST, 442, 10, 155, 100, ID_CMB_LANG, hFontUI);
            cb_add_u8(hCmbLang, "Русский (RU)");
            cb_add_u8(hCmbLang, "English (EN)");
            SendMessageW(hCmbLang, CB_SETCURSEL, 0, 0);

            hBtnHelpDlg  = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 760, 9, 195, 26, ID_BTN_HELP_DLG, hFontBold);
            hBtnHelpHtml = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 965, 9, 195, 26, ID_BTN_HELP_HTML, hFontUI);

            /* 1. Блок подключения */
            hGrpConn    = create_ctrl(hwnd, L"BUTTON", "", BS_GROUPBOX, 10, 8, 330, 142, 0, hFontBold);
            hBtnScan    = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 20, 28, 78, 24, ID_BTN_SCAN, hFontUI);
            hCmbSerial  = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWN | WS_VSCROLL, 102, 28, 140, 120, ID_CMB_SERIAL, hFontUI);
            hBtnConnUsb = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 246, 28, 84, 24, ID_BTN_CONN_USB, hFontBold);

            hBtnConnIp  = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 20, 58, 105, 24, ID_BTN_CONN_IP, hFontUI);
            hBtnDisconn = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 130, 58, 95, 24, ID_BTN_DISCONN, hFontUI);
            hBtnBlink   = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 230, 58, 100, 24, ID_BTN_BLINK, hFontUI);

            hLblStatus = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 90, 310, 18, 0, hFontBold);
            hLblInfo   = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 112, 310, 32, 0, hFontUI);

            /* 2. Блок настройки логической таблицы АЦП */
            hGrpLch   = create_ctrl(hwnd, L"BUTTON", "", BS_GROUPBOX, 10, 156, 330, 214, 0, hFontBold);
            hLblChCnt = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 178, 145, 18, 0, hFontUI);
            hEdtChCnt = create_ctrl(hwnd, L"EDIT", "4", WS_BORDER | ES_NUMBER, 168, 175, 45, 22, ID_EDT_CH_CNT, hFontUI);
            hBtnChCnt = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 220, 174, 110, 24, ID_BTN_CH_CNT, hFontUI);

            hLblLch = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 208, 45, 18, 0, hFontUI);
            hCmbLch = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 68, 205, 68, 200, ID_CMB_LCH, hFontUI);
            for (int i = 0; i < STAND_MAX_CHANNELS; i++) {
                char s[16];
                snprintf(s, sizeof(s), "LCH %d", i);
                cb_add_u8(hCmbLch, s);
            }
            SendMessageW(hCmbLch, CB_SETCURSEL, 0, 0);

            hLblPhy = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 142, 208, 78, 18, 0, hFontUI);
            hEdtPhy = create_ctrl(hwnd, L"EDIT", "0", WS_BORDER | ES_NUMBER, 222, 205, 42, 22, ID_EDT_PHY, hFontUI);

            hLblAvg = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 268, 208, 30, 18, 0, hFontUI);
            hEdtAvg = create_ctrl(hwnd, L"EDIT", "0", WS_BORDER | ES_NUMBER, 300, 205, 30, 22, ID_EDT_AVG, hFontUI);

            hLblMode = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 238, 45, 18, 0, hFontUI);
            hCmbMode = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWNLIST, 68, 235, 110, 100, ID_CMB_MODE, hFontUI);

            hLblRange = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 182, 238, 48, 18, 0, hFontUI);
            hCmbRange = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWNLIST, 232, 235, 98, 160, ID_CMB_RANGE, hFontUI);
            cb_add_u8(hCmbRange, "+/- 10 V");
            cb_add_u8(hCmbRange, "+/- 5 V");
            cb_add_u8(hCmbRange, "+/- 2 V");
            cb_add_u8(hCmbRange, "+/- 1 V");
            cb_add_u8(hCmbRange, "+/- 0.5 V");
            cb_add_u8(hCmbRange, "+/- 0.2 V");
            SendMessageW(hCmbRange, CB_SETCURSEL, 0, 0);

            hBtnChSet  = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 20, 266, 310, 24, ID_BTN_CH_SET, hFontUI);
            hLblPullup = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 300, 125, 18, 0, hFontUI);
            hEdtPullup = create_ctrl(hwnd, L"EDIT", "04", WS_BORDER, 148, 297, 48, 22, ID_EDT_PULLUP, hFontUI);
            hBtnPullup = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 205, 296, 125, 24, ID_BTN_PULLUP, hFontUI);
            hBtnApply  = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 20, 332, 310, 28, ID_BTN_APPLY, hFontBold);

            /* 3. Блок управления ЦАП */
            hGrpDac  = create_ctrl(hwnd, L"BUTTON", "", BS_GROUPBOX, 10, 376, 330, 86, 0, hFontBold);
            hLblDac1 = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 400, 60, 18, 0, hFontUI);
            hEdtDac1 = create_ctrl(hwnd, L"EDIT", "1.500", WS_BORDER, 82, 397, 65, 22, ID_EDT_DAC1, hFontUI);
            hBtnDac1 = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 155, 396, 175, 24, ID_BTN_DAC1, hFontUI);

            hLblDac2 = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 430, 60, 18, 0, hFontUI);
            hEdtDac2 = create_ctrl(hwnd, L"EDIT", "-2.250", WS_BORDER, 82, 427, 65, 22, ID_EDT_DAC2, hFontUI);
            hBtnDac2 = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 155, 426, 175, 24, ID_BTN_DAC2, hFontUI);

            /* 4. Генератор искусственных сигналов (Имитатор) */
            hGrpSim    = create_ctrl(hwnd, L"BUTTON", "", BS_GROUPBOX, 10, 468, 330, 172, 0, hFontBold);
            hLblSimPhy = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 492, 48, 18, 0, hFontUI);
            hCmbSimPhy = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 68, 489, 75, 200, ID_CMB_SIM_PHY, hFontUI);
            for (int i = 0; i < X502_ADC_COMM_CH_CNT; i++) {
                char s[16];
                snprintf(s, sizeof(s), "Phy %d", i);
                cb_add_u8(hCmbSimPhy, s);
            }
            SendMessageW(hCmbSimPhy, CB_SETCURSEL, 0, 0);

            hLblSimWave = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 148, 492, 42, 18, 0, hFontUI);
            hCmbSimWave = create_ctrl(hwnd, L"COMBOBOX", "", CBS_DROPDOWNLIST, 192, 489, 138, 180, ID_CMB_SIM_WAVE, hFontUI);

            hLblSimAmp   = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 524, 58, 18, 0, hFontUI);
            hEdtSimAmp   = create_ctrl(hwnd, L"EDIT", "2.50", WS_BORDER, 80, 521, 55, 22, ID_EDT_SIM_AMP, hFontUI);
            hLblSimFreq  = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 145, 524, 64, 18, 0, hFontUI);
            hEdtSimFreq  = create_ctrl(hwnd, L"EDIT", "50.0", WS_BORDER, 212, 521, 60, 22, ID_EDT_SIM_FREQ, hFontUI);

            hLblSimOffs  = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 554, 58, 18, 0, hFontUI);
            hEdtSimOffs  = create_ctrl(hwnd, L"EDIT", "0.00", WS_BORDER, 80, 551, 55, 22, ID_EDT_SIM_OFFS, hFontUI);
            hLblSimNoise = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 145, 554, 64, 18, 0, hFontUI);
            hEdtSimNoise = create_ctrl(hwnd, L"EDIT", "0.0050", WS_BORDER, 212, 551, 60, 22, ID_EDT_SIM_NOISE, hFontUI);

            hBtnSimSet      = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 20, 584, 310, 26, ID_BTN_SIM_SET, hFontUI);
            hLblSimDefaults = create_ctrl(hwnd, L"STATIC", "", SS_LEFT, 20, 616, 310, 18, 0, hFontUI);

            /* Правая панель: управление сбором и таблица каналов */
            hBtnAcqRun    = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 350, 438, 170, 28, ID_BTN_ACQ_RUN, hFontBold);
            hBtnAcqSingle = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 530, 438, 140, 28, ID_BTN_ACQ_SINGLE, hFontUI);
            hBtnAcqClear  = create_ctrl(hwnd, L"BUTTON", "", BS_PUSHBUTTON, 680, 438, 150, 28, ID_BTN_ACQ_CLEAR, hFontUI);

            hListChannels = create_ctrl(hwnd, L"LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                        350, 474, 810, 166, 0, hFontMono);

            hLblStatusBar = create_ctrl(hwnd, L"STATIC", "", WS_BORDER | SS_LEFT, 10, 648, 1150, 22, 0, hFontUI);

            E502_StandInitStruct(&g_dev);
            apply_language(hwnd);
            populate_usb_list();
            load_lch_fields(0);
            load_sim_fields(0);
            update_ui_state(hwnd);

            SetTimer(hwnd, ID_TIMER_ACQ, 50, NULL);
            if (g_demo_mode) {
                SendMessageW(hwnd, WM_COMMAND, ID_BTN_CONN_USB, 0);
                SendMessageW(hwnd, WM_COMMAND, ID_BTN_DAC1, 0);
                SendMessageW(hwnd, WM_COMMAND, ID_BTN_DAC2, 0);
                SendMessageW(hwnd, WM_COMMAND, ID_BTN_ACQ_RUN, 0);
            }
            if (g_self_test_mode) {
                SetTimer(hwnd, ID_TIMER_SELFTEST, 60, NULL);
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == ID_TIMER_ACQ) {
                if (g_led_flash_ticks > 0) {
                    g_led_flash_ticks--;
                    if (g_led_flash_ticks == 0) InvalidateRect(hwnd, &g_rcScope, FALSE);
                }
                if (g_acq_running && g_dev.is_connected) {
                    acquire_single_frame(hwnd);
                }
            } else if (wParam == ID_TIMER_SELFTEST) {
                g_self_test_ticks++;
                if (g_self_test_ticks == 1) {
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_CONN_USB, 0);
                    SetWindowTextA(hEdtChCnt, "6");
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_CH_CNT, 0);
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_APPLY, 0);
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_DAC1, 0);
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_DAC2, 0);
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_ACQ_RUN, 0);
                } else if (g_self_test_ticks == 4) {
                    if (g_capture_docs_mode) {
                        for (int k = 0; k < 180; k++) {
                            acquire_single_frame(hwnd);
                        }
                    }
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_HELP_DLG, 0);
                    if (hHelpWnd) {
                        SendMessageW(hHelpWnd, WM_COMMAND, ID_HELP_NEXT, 0);
                        SendMessageW(hHelpWnd, WM_COMMAND, ID_HELP_SHOW_ALL, 0);
                    }
                } else if (g_self_test_ticks == 7) {
                    if (g_capture_docs_mode) {
                        UpdateWindow(hwnd);
                        save_hwnd_bmp(hwnd, false, "docs_assets/raw_main_full.bmp");
                        save_hwnd_bmp(hwnd, true,  "docs_assets/raw_main_client.bmp");
                        if (hHelpWnd) {
                            show_help_topic(1);
                            UpdateWindow(hHelpWnd);
                            save_hwnd_bmp(hHelpWnd, false, "docs_assets/raw_help_ch2.bmp");
                            show_help_topic(11);
                            UpdateWindow(hHelpWnd);
                            save_hwnd_bmp(hHelpWnd, false, "docs_assets/raw_help_func.bmp");
                        }
                    }
                    if (hHelpWnd) DestroyWindow(hHelpWnd);
                } else if (g_self_test_ticks >= 10) {
                    SendMessageW(hwnd, WM_COMMAND, ID_BTN_DISCONN, 0);
                    DestroyWindow(hwnd);
                }
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (id == ID_CMB_LANG && code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(hCmbLang, CB_GETCURSEL, 0, 0);
                g_lang = (sel == 1) ? LANG_EN : LANG_RU;
                apply_language(hwnd);
                set_status_msg(tr("[OK] Язык интерфейса переключен на Русский.",
                                  "[OK] Interface language switched to English."));
                return 0;
            }
            if (id == ID_CMB_LCH && code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(hCmbLch, CB_GETCURSEL, 0, 0);
                if (sel >= 0) load_lch_fields((uint32_t)sel);
                return 0;
            }
            if (id == ID_CMB_SIM_PHY && code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(hCmbSimPhy, CB_GETCURSEL, 0, 0);
                if (sel >= 0) load_sim_fields((uint32_t)sel);
                return 0;
            }

            switch (id) {
                case ID_BTN_HELP_DLG:
                    show_help_dialog(hwnd);
                    break;

                case ID_BTN_HELP_HTML:
                    open_html_manual(hwnd);
                    set_status_msg(tr("[OK] Открыто руководство пользователя USER_GUIDE.html.",
                                      "[OK] Opened USER_GUIDE.html in system browser."));
                    break;

                case ID_BTN_SCAN:
                    populate_usb_list();
                    prompt_driver_download(hwnd);
                    break;

                case ID_BTN_CONN_USB: {
                    char serial[X502_SERIAL_SIZE] = {0};
                    GetWindowTextA(hCmbSerial, serial, sizeof(serial));
                    clear_scope_history();
                    if (E502_StandConnectUSB(&g_dev, serial) == X502_ERR_OK) {
                        E502_StandSetChannelCount(&g_dev, 6);
                        E502_StandApplyConfig(&g_dev);
                        SetWindowTextA(hEdtChCnt, "6");
                        if (!E502_SimHasRealDriver()) {
                            set_status_msg(tr("[OK] Подключено к имитатору %s. Драйвер для реальной платы: %s",
                                              "[OK] Connected to simulator %s. Real board driver: %s"),
                                           g_dev.dev_info.serial, LCARD_DRIVER_URL);
                        } else {
                            set_status_msg(tr("[OK] Подключено к модулю %s (S/N: %s).",
                                              "[OK] Connected to %s (S/N: %s)."),
                                           g_dev.dev_info.name, g_dev.dev_info.serial);
                        }
                    } else {
                        set_status_msg(tr("[ОШИБКА] Сбой подключения USB: %s (драйвер: %s)",
                                          "[ERROR] Connect USB failed: %s (driver: %s)"),
                                       E502_StandGetErrorStr(&g_dev), LCARD_DRIVER_URL);
                        prompt_driver_download(hwnd);
                    }
                    update_ui_state(hwnd);
                    break;
                }

                case ID_BTN_CONN_IP: {
                    clear_scope_history();
                    uint32_t ip = (192U << 24) | (168U << 16) | (1U << 8) | 100U;
                    if (E502_StandConnectIP(&g_dev, ip, 1000) == X502_ERR_OK) {
                        E502_StandSetChannelCount(&g_dev, 6);
                        E502_StandApplyConfig(&g_dev);
                        SetWindowTextA(hEdtChCnt, "6");
                        set_status_msg(tr("[OK] Подключено по Ethernet (192.168.1.100) к %s.",
                                          "[OK] Connected via Ethernet (192.168.1.100) to %s."),
                                       g_dev.dev_info.serial);
                    } else {
                        set_status_msg(tr("[ОШИБКА] Сбой подключения IP: %s",
                                          "[ERROR] Connect IP failed: %s"),
                                       E502_StandGetErrorStr(&g_dev));
                    }
                    update_ui_state(hwnd);
                    break;
                }

                case ID_BTN_DISCONN:
                    g_acq_running = false;
                    set_text_u8(hBtnAcqRun, tr("Старт опроса (20 Гц)", "Start Polling (20 Hz)"));
                    E502_StandDisconnect(&g_dev);
                    set_status_msg(tr("[OK] Соединение закрыто.", "[OK] Disconnected."));
                    update_ui_state(hwnd);
                    break;

                case ID_BTN_BLINK:
                    if (E502_StandBlinkLed(&g_dev) == X502_ERR_OK) {
                        g_led_flash_ticks = 6;
                        set_status_msg(tr("[OK] Команда мигания светодиодом LED1 выполнена.",
                                          "[OK] Front panel LED1 blink triggered."));
                        InvalidateRect(hwnd, &g_rcScope, FALSE);
                    } else {
                        set_status_msg(tr("[ОШИБКА] Сбой команды LED1: %s",
                                          "[ERROR] LED blink failed: %s"),
                                       E502_StandGetErrorStr(&g_dev));
                    }
                    break;

                case ID_BTN_CH_CNT: {
                    char buf[32] = {0};
                    GetWindowTextA(hEdtChCnt, buf, sizeof(buf));
                    uint32_t cnt = (uint32_t)strtoul(buf, NULL, 10);
                    if (E502_StandSetChannelCount(&g_dev, cnt) == X502_ERR_OK) {
                        set_status_msg(tr("[OK] Число каналов установлено: %u (нажмите Apply для записи в ПЛИС).",
                                          "[OK] Logical channel count set to %u (unapplied)."), cnt);
                    } else {
                        set_status_msg(tr("[ОШИБКА] Неверное число каналов: %s",
                                          "[ERROR] Set channel count failed: %s"),
                                       E502_StandGetErrorStr(&g_dev));
                    }
                    update_ui_state(hwnd);
                    break;
                }

                case ID_BTN_CH_SET: {
                    int lch = (int)SendMessageW(hCmbLch, CB_GETCURSEL, 0, 0);
                    int mode_idx = (int)SendMessageW(hCmbMode, CB_GETCURSEL, 0, 0);
                    int range_idx = (int)SendMessageW(hCmbRange, CB_GETCURSEL, 0, 0);
                    char b_phy[32] = {0}, b_avg[32] = {0};
                    GetWindowTextA(hEdtPhy, b_phy, sizeof(b_phy));
                    GetWindowTextA(hEdtAvg, b_avg, sizeof(b_avg));
                    uint32_t phy = (uint32_t)strtoul(b_phy, NULL, 10);
                    uint32_t avg = (uint32_t)strtoul(b_avg, NULL, 10);

                    if (E502_StandConfigChannel(&g_dev, (uint32_t)lch, phy,
                                                (uint32_t)mode_idx, (uint32_t)range_idx, avg) == X502_ERR_OK) {
                        set_status_msg(tr("[OK] Канал LCH[%d] -> Физ.вход %u настроен (нужен Apply).",
                                          "[OK] LCH[%d] -> Phy %u configured (unapplied)."), lch, phy);
                    } else {
                        set_status_msg(tr("[ОШИБКА] Настройка LCH[%d] отклонена: %s",
                                          "[ERROR] LCH[%d] config failed: %s"),
                                       lch, E502_StandGetErrorStr(&g_dev));
                    }
                    update_ui_state(hwnd);
                    break;
                }

                case ID_BTN_PULLUP: {
                    char buf[32] = {0};
                    GetWindowTextA(hEdtPullup, buf, sizeof(buf));
                    uint32_t mask = (uint32_t)strtoul(buf, NULL, 16);
                    if (E502_StandSetPullups(&g_dev, mask) == X502_ERR_OK) {
                        set_status_msg(tr("[OK] Маска подтяжки 0x%02X установлена (нужен Apply).",
                                          "[OK] Pull-up mask set to 0x%02X (unapplied)."), mask);
                    } else {
                        set_status_msg(tr("[ОШИБКА] Неверная маска подтяжки: %s",
                                          "[ERROR] Pull-up mask failed: %s"),
                                       E502_StandGetErrorStr(&g_dev));
                    }
                    update_ui_state(hwnd);
                    break;
                }

                case ID_BTN_APPLY:
                    if (E502_StandApplyConfig(&g_dev) == X502_ERR_OK) {
                        set_status_msg(tr("[УСПЕХ] Конфигурация записана в регистры ПЛИС.",
                                          "[SUCCESS] Configuration committed to FPGA registers."));
                    } else {
                        set_status_msg(tr("[ОШИБКА] Сбой записи в ПЛИС: %s",
                                          "[ERROR] Apply failed: %s"),
                                       E502_StandGetErrorStr(&g_dev));
                    }
                    update_ui_state(hwnd);
                    break;

                case ID_BTN_DAC1:
                case ID_BTN_DAC2: {
                    bool is_dac1 = (id == ID_BTN_DAC1);
                    char buf[32] = {0};
                    GetWindowTextA(is_dac1 ? hEdtDac1 : hEdtDac2, buf, sizeof(buf));
                    char *end = NULL;
                    double v = strtod(buf, &end);
                    if (end == buf || !isfinite(v)) {
                        set_status_msg(tr("[ОШИБКА] Некорректный формат напряжения ЦАП.",
                                          "[ERROR] Invalid DAC voltage format."));
                        break;
                    }
                    if (E502_StandWriteDac(&g_dev, is_dac1 ? X502_DAC_CH1 : X502_DAC_CH2, v) == X502_ERR_OK) {
                        set_status_msg(tr("[OK] На выход %s установлено напряжение %+.3f В.",
                                          "[OK] %s output set to %+.3f V."),
                                       is_dac1 ? "DAC1" : "DAC2", v);
                    } else {
                        set_status_msg(tr("[ОШИБКА] Запись в ЦАП отклонена: %s",
                                          "[ERROR] DAC write failed: %s"),
                                       E502_StandGetErrorStr(&g_dev));
                    }
                    update_ui_state(hwnd);
                    break;
                }

                case ID_BTN_SIM_SET: {
                    int phy = (int)SendMessageW(hCmbSimPhy, CB_GETCURSEL, 0, 0);
                    int wave = (int)SendMessageW(hCmbSimWave, CB_GETCURSEL, 0, 0);
                    char b_amp[32], b_freq[32], b_offs[32], b_noise[32];
                    GetWindowTextA(hEdtSimAmp, b_amp, sizeof(b_amp));
                    GetWindowTextA(hEdtSimFreq, b_freq, sizeof(b_freq));
                    GetWindowTextA(hEdtSimOffs, b_offs, sizeof(b_offs));
                    GetWindowTextA(hEdtSimNoise, b_noise, sizeof(b_noise));
                    E502_SimSetPhySignal((uint32_t)phy, (E502_SimWaveType)wave,
                                         strtod(b_amp, NULL), strtod(b_freq, NULL),
                                         strtod(b_offs, NULL), strtod(b_noise, NULL));
                    set_status_msg(tr("[ИМИТАТОР] Обновлены параметры сигнала на физическом входе #%d.",
                                      "[SIM] Updated synthetic signal on Physical Input #%d."), phy);
                    break;
                }

                case ID_BTN_ACQ_RUN:
                    if (!g_dev.is_connected) {
                        set_status_msg(tr("[ОШИБКА] Сначала подключите устройство в разделе 1.",
                                          "[ERROR] Connect to device before starting polling."));
                        break;
                    }
                    g_acq_running = !g_acq_running;
                    set_text_u8(hBtnAcqRun, g_acq_running
                        ? tr("Стоп опроса", "Stop Polling")
                        : tr("Старт опроса (20 Гц)", "Start Polling (20 Hz)"));
                    set_status_msg(g_acq_running
                        ? tr("[ОПРОС] Непрерывный опрос АЦП запущен.", "[ACQ] Continuous ADC polling started.")
                        : tr("[ОПРОС] Опрос АЦП приостановлен.", "[ACQ] Polling paused."));
                    break;

                case ID_BTN_ACQ_SINGLE:
                    acquire_single_frame(hwnd);
                    break;

                case ID_BTN_ACQ_CLEAR:
                    clear_scope_history();
                    update_ui_state(hwnd);
                    set_status_msg(tr("[OK] Буфер осциллографа очищен.",
                                      "[OK] Oscilloscope trace buffer cleared."));
                    break;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            draw_oscilloscope(hdc, &g_rcScope);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, ID_TIMER_ACQ);
            E502_StandDisconnect(&g_dev);
            if (hFontUI) DeleteObject(hFontUI);
            if (hFontBold) DeleteObject(hFontBold);
            if (hFontMono) DeleteObject(hFontMono);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd) {
    (void)hPrev;
    if (lpCmdLine && strstr(lpCmdLine, "--self-test") != NULL) {
        g_self_test_mode = true;
    }
    if (lpCmdLine && strstr(lpCmdLine, "--capture-docs") != NULL) {
        g_self_test_mode = true;
        g_capture_docs_mode = true;
    }
    if (lpCmdLine && strstr(lpCmdLine, "--demo") != NULL) {
        g_demo_mode = true;
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"E502LabWorkstationWnd";
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = {0, 0, 1175, 680};
    AdjustWindowRect(&rc, style, FALSE);

    hMainWnd = CreateWindowExW(0, wc.lpszClassName, L"E-502 Workstation",
                               style, CW_USEDEFAULT, CW_USEDEFAULT,
                               rc.right - rc.left, rc.bottom - rc.top,
                               NULL, NULL, hInst, NULL);
    if (!hMainWnd) return 1;

    ShowWindow(hMainWnd, g_self_test_mode ? SW_SHOWNOACTIVATE : nShowCmd);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
