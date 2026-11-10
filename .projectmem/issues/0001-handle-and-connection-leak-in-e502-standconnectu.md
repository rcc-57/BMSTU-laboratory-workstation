# #0001 Handle and connection leak in E502_StandConnectUSB/IP on reconnect or when E502_StandApplyConfig fails after Open

- 2026-11-10T18:23:55Z `issue`: Handle and connection leak in E502_StandConnectUSB/IP on reconnect or when E502_StandApplyConfig fails after Open [LCARD/e502_ctrl.c:20]
- 2026-11-10T18:49:13Z `fix`: Guarded reconnect with E502_STAND_MAGIC and added E502_StandDisconnect rollback on GetDevInfo/ApplyConfig failure; verified in test_stand.c [LCARD/e502_ctrl.c:59]
