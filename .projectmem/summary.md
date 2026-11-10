# projectmem - BMSTU-laboratory-workstation

_Last updated: 2026-11-10_

## Project purpose
This is a thin c-api wrapper programmatically generated for the excellent C++ immediate mode gui [Dear ImGui](https://github.com/ocornut/imgui). All imgui.h functions are programmatically wrapped. Generated files are: `cimgui.cpp`, `cimgui.h` for C compilation. Also for helping in bindings…

## Recent issues
- [DONE] #0005 In-app manual text truncated at 2048 wide chars due to fixed stack buffer wbuf[2048] in set_text_u8 [LCARD/main_gui.c:115] (fixed)
- [DONE] #0004 CLI input vulnerabilities: fgets truncation executes leftover line, sscanf %u wraps negative to UINT32_MAX, NaN/out-of-range DAC volts [LCARD/main_cli.c:100] (fixed)
- [DONE] #0003 OOB risk in E502_StandApplyConfig/ReadAdcFrame when lch_count > STAND_MAX_CHANNELS and missing volts_buf bounds check [LCARD/e502_ctrl.c:109] (fixed)
- [DONE] #0002 Early returns in E502_Stand* do not update dev->last_error, making E502_StandGetErrorStr report X502_ERR_OK on failure [LCARD/e502_ctrl.c:76] (fixed)
- [DONE] #0001 Handle and connection leak in E502_StandConnectUSB/IP on reconnect or when E502_StandApplyConfig fails after Open [LCARD/e502_ctrl.c:20] (fixed)

## Decisions
- Defer X502_Set* calls to E502_StandApplyConfig and enforce disconnect-on-fail in ConnectUSB/ConnectIP to keep hnd and struct state atomic [LCARD/e502_ctrl.c]
- Added runtime RU/EN localization via WideChar Win32 API and dual documentation access (in-app manual dialog + printable USER_GUIDE.html/md) [LCARD/main_gui.c]
- Use dynamic Win32 LoadLibraryA bridge in e502_sim.c to load real x502api.dll and e502api.dll when present while keeping E502-SIM-0001/0002 virtual simulator devices available in the same binary [LCARD/e502_sim.c:55]

## Notes
- cpp2ffi: opaque returns assign to static obj
- cpp2ffi: add forced_nonPOD, skip_template, skip_convert_gen (cimnodes_editor work)
- cpp2ffi: 3 scope corrections
- cpp2ffi: split with cpp2ffi_extra.lua
- cpp2ffi: breaking: call_args_old is gone we have call_args and call_args_conv only when needed
- name_conversion clean
- cpp2ffi: don't let the generated-header test link
- Merge pull request #326 from JamesWrigley/header-test
- cpp2ffi: overloaded name without ::
- gotcha: E502_UsbGetSerialList ignores X502_GETDEVS_FLAGS_ONLY_NOT_OPENED and writes total count (>size) to devcnt; X502_AsyncGetAdcFrame internally starts/stops stream [LCARD/e502api.h]

## Key files
- `README.md`
- `cimgui.cpp`
- `cimgui.h`
- `cimgui_impl.h`
- `generator/output/constants.json`
- `generator/output/constants.lua`
- `generator/output/definitions.json`
- `generator/output/definitions.lua`
- `generator/output/impl_definitions.json`
- `generator/output/impl_definitions.lua`
- `generator/output/overloads.txt`
- `generator/output/structs_and_enums.json`
- `generator/output/structs_and_enums.lua`
- `generator/output/typedefs_dict.json`
- `generator/output/typedefs_dict.lua`
- `imgui`
- `1.92.9`
- `generator/cpp2ffi.lua`
- `generator/generator.lua`
- `1.92.9bdock`

## Open questions
- None logged yet.
