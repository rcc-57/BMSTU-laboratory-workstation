# #0002 Early returns in E502_Stand* do not update dev->last_error, making E502_StandGetErrorStr report X502_ERR_OK on failure

- 2026-11-10T18:24:09Z `issue`: Early returns in E502_Stand* do not update dev->last_error, making E502_StandGetErrorStr report X502_ERR_OK on failure [LCARD/e502_ctrl.c:76]
- 2026-11-10T18:49:24Z `fix`: Updated all E502_Stand* early returns to set dev->last_error to specific X502_ERR_* codes; verified in test_stand.c [LCARD/e502_ctrl.c:139]
