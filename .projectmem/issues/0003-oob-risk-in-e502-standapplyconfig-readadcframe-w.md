# #0003 OOB risk in E502_StandApplyConfig/ReadAdcFrame when lch_count > STAND_MAX_CHANNELS and missing volts_buf bounds check

- 2026-11-10T18:24:25Z `issue`: OOB risk in E502_StandApplyConfig/ReadAdcFrame when lch_count > STAND_MAX_CHANNELS and missing volts_buf bounds check [LCARD/e502_ctrl.c:109]
- 2026-11-10T18:49:37Z `fix`: Enforced lch_count <= STAND_MAX_CHANNELS, lch < dev->lch_count, NULL checks, and channel/DAC parameter bounds in e502_ctrl.c; verified in test_stand.c [LCARD/e502_ctrl.c:201]
