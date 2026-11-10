#include <string.h>
#include <math.h>
#include "e502_ctrl.h"

#define STAND_VALID_PULLUPS_MASK (X502_PULLUPS_DI_H | X502_PULLUPS_DI_L | \
                                  X502_PULLUPS_DI_SYN1 | X502_PULLUPS_DI_SYN2 | \
                                  X502_PULLDOWN_CONV_IN | X502_PULLDOWN_START_IN)

static int validate_channel_params(uint32_t phy_ch, uint32_t mode, uint32_t range, uint32_t avg) {
    if (mode != X502_LCH_MODE_COMM && mode != X502_LCH_MODE_DIFF && mode != X502_LCH_MODE_ZERO) {
        return X502_ERR_INVALID_LCH_MODE;
    }
    if (mode == X502_LCH_MODE_DIFF && phy_ch >= X502_ADC_DIFF_CH_CNT) {
        return X502_ERR_INVALID_LCH_PHY_NUMBER;
    }
    if (mode == X502_LCH_MODE_COMM && phy_ch >= X502_ADC_COMM_CH_CNT) {
        return X502_ERR_INVALID_LCH_PHY_NUMBER;
    }
    if (mode == X502_LCH_MODE_ZERO && phy_ch >= X502_ADC_COMM_CH_CNT) {
        return X502_ERR_INVALID_LCH_PHY_NUMBER;
    }
    if (range > X502_ADC_RANGE_02) {
        return X502_ERR_INVALID_LCH_RANGE;
    }
    if (avg > X502_LCH_AVG_SIZE_MAX) {
        return X502_ERR_INVALID_LCH_AVG_SIZE;
    }
    return X502_ERR_OK;
}

void E502_StandInitStruct(E502_StandDevice *dev) {
    if (dev == NULL) return;
    memset(dev, 0, sizeof(E502_StandDevice));
    dev->lch_count = 1;
    /* Настройка по умолчанию для всех логических каналов: вход i % 32, общая земля, диапазон +/-10В */
    for (uint32_t i = 0; i < STAND_MAX_CHANNELS; i++) {
        dev->lch_cfg[i].phy_ch = i % X502_ADC_COMM_CH_CNT;
        dev->lch_cfg[i].mode   = X502_LCH_MODE_COMM;
        dev->lch_cfg[i].range  = X502_ADC_RANGE_10;
        dev->lch_cfg[i].avg    = 0;
    }
    dev->config_dirty = true;
    dev->last_error   = X502_ERR_OK;
    dev->init_magic   = E502_STAND_MAGIC;
}

int E502_StandListUSB(char serials[][X502_SERIAL_SIZE], uint32_t max_count, uint32_t *found_count) {
    if (serials == NULL && max_count > 0) return X502_ERR_INVALID_POINTER;
    uint32_t total = 0;
    int32_t res = E502_UsbGetSerialList(serials, max_count, 0, &total);
    if (res < 0) {
        if (found_count != NULL) *found_count = 0;
        return res;
    }
    if (found_count != NULL) {
        *found_count = (max_count > 0) ? (uint32_t)res : total;
    }
    return X502_ERR_OK;
}

int E502_StandConnectUSB(E502_StandDevice *dev, const char *serial) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (dev->init_magic == E502_STAND_MAGIC && dev->hnd != NULL) {
        E502_StandDisconnect(dev);
    }
    E502_StandInitStruct(dev);
    dev->hnd = X502_Create();
    if (dev->hnd == NULL) {
        dev->last_error = X502_ERR_MEMORY_ALLOC;
        return dev->last_error;
    }

    dev->last_error = E502_OpenUsb(dev->hnd, (serial && serial[0] != '\0') ? serial : NULL);
    if (dev->last_error != X502_ERR_OK) {
        /* Согласно документации, при ряде ошибок (например, X502_ERR_FPGA_NOT_LOADED)
           соединение может остаться открытым, поэтому закрываем его перед освобождением */
        X502_Close(dev->hnd);
        X502_Free(dev->hnd);
        dev->hnd = NULL;
        return dev->last_error;
    }

    dev->is_connected = true;
    dev->last_error = X502_GetDevInfo(dev->hnd, &dev->dev_info);
    if (dev->last_error == X502_ERR_OK) {
        dev->last_error = E502_StandApplyConfig(dev);
    }
    if (dev->last_error != X502_ERR_OK) {
        int32_t err = dev->last_error;
        E502_StandDisconnect(dev);
        dev->last_error = err;
        return err;
    }
    return X502_ERR_OK;
}

int E502_StandConnectIP(E502_StandDevice *dev, uint32_t ip_addr, uint32_t tout_ms) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (dev->init_magic == E502_STAND_MAGIC && dev->hnd != NULL) {
        E502_StandDisconnect(dev);
    }
    E502_StandInitStruct(dev);
    dev->hnd = X502_Create();
    if (dev->hnd == NULL) {
        dev->last_error = X502_ERR_MEMORY_ALLOC;
        return dev->last_error;
    }

    dev->last_error = E502_OpenByIpAddr(dev->hnd, ip_addr, 0, tout_ms);
    if (dev->last_error != X502_ERR_OK) {
        X502_Close(dev->hnd);
        X502_Free(dev->hnd);
        dev->hnd = NULL;
        return dev->last_error;
    }

    dev->is_connected = true;
    dev->last_error = X502_GetDevInfo(dev->hnd, &dev->dev_info);
    if (dev->last_error == X502_ERR_OK) {
        dev->last_error = E502_StandApplyConfig(dev);
    }
    if (dev->last_error != X502_ERR_OK) {
        int32_t err = dev->last_error;
        E502_StandDisconnect(dev);
        dev->last_error = err;
        return err;
    }
    return X502_ERR_OK;
}

void E502_StandDisconnect(E502_StandDevice *dev) {
    if (dev == NULL) return;
    if (dev->hnd != NULL) {
        if (dev->is_connected) {
            X502_Close(dev->hnd);
        }
        X502_Free(dev->hnd);
        dev->hnd = NULL;
    }
    dev->is_connected = false;
}

int E502_StandSetChannelCount(E502_StandDevice *dev, uint32_t count) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    if (count == 0 || count > STAND_MAX_CHANNELS) {
        dev->last_error = X502_ERR_INVALID_LTABLE_SIZE;
        return dev->last_error;
    }
    dev->last_error = X502_SetLChannelCount(dev->hnd, count);
    if (dev->last_error == X502_ERR_OK) {
        dev->lch_count = count;
        dev->config_dirty = true;
    }
    return dev->last_error;
}

int E502_StandConfigChannel(E502_StandDevice *dev, uint32_t lch, uint32_t phy_ch,
                            uint32_t mode, uint32_t range, uint32_t avg) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    if (lch >= STAND_MAX_CHANNELS || lch >= dev->lch_count) {
        dev->last_error = X502_ERR_INVALID_LCH_NUMBER;
        return dev->last_error;
    }
    int v_err = validate_channel_params(phy_ch, mode, range, avg);
    if (v_err != X502_ERR_OK) {
        dev->last_error = v_err;
        return dev->last_error;
    }
    dev->last_error = X502_SetLChannel(dev->hnd, lch, phy_ch, mode, range, avg);
    if (dev->last_error == X502_ERR_OK) {
        dev->lch_cfg[lch].phy_ch = phy_ch;
        dev->lch_cfg[lch].mode   = mode;
        dev->lch_cfg[lch].range  = range;
        dev->lch_cfg[lch].avg    = avg;
        dev->config_dirty = true;
    }
    return dev->last_error;
}

int E502_StandSetPullups(E502_StandDevice *dev, uint32_t pullups_mask) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    if ((pullups_mask & ~STAND_VALID_PULLUPS_MASK) != 0) {
        dev->last_error = X502_ERR_INVALID_OP_FOR_IFACE;
        return dev->last_error;
    }
    dev->last_error = X502_SetDigInPullup(dev->hnd, pullups_mask);
    if (dev->last_error == X502_ERR_OK) {
        dev->pullups_mask = pullups_mask;
        dev->config_dirty = true;
    }
    return dev->last_error;
}

int E502_StandApplyConfig(E502_StandDevice *dev) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    if (dev->lch_count == 0 || dev->lch_count > STAND_MAX_CHANNELS) {
        dev->last_error = X502_ERR_INVALID_LTABLE_SIZE;
        return dev->last_error;
    }

    /* Явно переводим модуль в штатный режим работы с ПЛИС (согласно п. 3.1.5 руководства) */
    dev->last_error = X502_SetMode(dev->hnd, X502_MODE_FPGA);
    if (dev->last_error != X502_ERR_OK) return dev->last_error;

    /* Записываем количество каналов и конфигурацию каждого канала в описатель */
    dev->last_error = X502_SetLChannelCount(dev->hnd, dev->lch_count);
    if (dev->last_error != X502_ERR_OK) return dev->last_error;

    for (uint32_t i = 0; i < dev->lch_count; i++) {
        int v_err = validate_channel_params(dev->lch_cfg[i].phy_ch,
                                            dev->lch_cfg[i].mode,
                                            dev->lch_cfg[i].range,
                                            dev->lch_cfg[i].avg);
        if (v_err != X502_ERR_OK) {
            dev->last_error = v_err;
            return dev->last_error;
        }
        dev->last_error = X502_SetLChannel(dev->hnd, i,
                                           dev->lch_cfg[i].phy_ch,
                                           dev->lch_cfg[i].mode,
                                           dev->lch_cfg[i].range,
                                           dev->lch_cfg[i].avg);
        if (dev->last_error != X502_ERR_OK) return dev->last_error;
    }

    dev->last_error = X502_SetDigInPullup(dev->hnd, dev->pullups_mask);
    if (dev->last_error != X502_ERR_OK) return dev->last_error;

    /* Передаем все установленные настройки непосредственно в регистры модуля */
    dev->last_error = X502_Configure(dev->hnd, 0);
    if (dev->last_error == X502_ERR_OK) {
        dev->config_dirty = false;
    }
    return dev->last_error;
}

int E502_StandReadAdcFrame(E502_StandDevice *dev, double *volts_buf, uint32_t tout_ms) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    if (volts_buf == NULL) {
        dev->last_error = X502_ERR_INVALID_POINTER;
        return dev->last_error;
    }
    if (dev->lch_count == 0 || dev->lch_count > STAND_MAX_CHANNELS) {
        dev->last_error = X502_ERR_INVALID_LTABLE_SIZE;
        return dev->last_error;
    }
    /* Автоматически применяем настройки, если они менялись до чтения */
    if (dev->config_dirty) {
        int err = E502_StandApplyConfig(dev);
        if (err != X502_ERR_OK) return err;
    }
    dev->last_error = X502_AsyncGetAdcFrame(dev->hnd, X502_PROC_FLAGS_VOLT, tout_ms, volts_buf);
    return dev->last_error;
}

int E502_StandWriteDac(E502_StandDevice *dev, uint32_t dac_ch, double volts) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    if (dac_ch != X502_DAC_CH1 && dac_ch != X502_DAC_CH2) {
        dev->last_error = X502_ERR_INVALID_DAC_CHANNEL;
        return dev->last_error;
    }
    if (!isfinite(volts) || volts < -X502_DAC_RANGE || volts > X502_DAC_RANGE) {
        dev->last_error = X502_ERR_INVALID_DAC_CHANNEL;
        return dev->last_error;
    }
    uint32_t flags = X502_DAC_FLAGS_VOLT | X502_DAC_FLAGS_CALIBR;
    dev->last_error = X502_AsyncOutDac(dev->hnd, dac_ch, volts, flags);
    if (dev->last_error == X502_ERR_OK) {
        if (dac_ch == X502_DAC_CH1) dev->dac_volts[0] = volts;
        if (dac_ch == X502_DAC_CH2) dev->dac_volts[1] = volts;
    }
    return dev->last_error;
}

int E502_StandBlinkLed(E502_StandDevice *dev) {
    if (dev == NULL) return X502_ERR_INVALID_POINTER;
    if (!dev->is_connected || dev->hnd == NULL) {
        dev->last_error = X502_ERR_DEVICE_NOT_OPENED;
        return dev->last_error;
    }
    dev->last_error = X502_LedBlink(dev->hnd);
    return dev->last_error;
}

const char* E502_StandGetErrorStr(const E502_StandDevice *dev) {
    if (dev == NULL) return "Invalid device pointer (NULL)";
    return X502_GetErrorString(dev->last_error);
}
