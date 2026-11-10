# #0005 In-app manual text truncated at 2048 wide chars due to fixed stack buffer wbuf[2048] in set_text_u8

- 2026-11-10T19:15:00Z `issue`: In-app manual text truncated at 2048 wide chars due to fixed stack buffer wbuf[2048] in set_text_u8 [LCARD/main_gui.c:115]
- 2026-11-10T19:27:24Z `fix`: Dynamically allocate wide buffer in set_text_u8 and set EM_SETLIMITTEXT(0) on hHelpEdit; verified full help rendering via --capture-docs screenshots [LCARD/main_gui.c:127]
