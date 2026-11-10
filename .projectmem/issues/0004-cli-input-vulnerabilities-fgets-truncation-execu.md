# #0004 CLI input vulnerabilities: fgets truncation executes leftover line, sscanf %u wraps negative to UINT32_MAX, NaN/out-of-range DAC volts

- 2026-11-10T18:24:36Z `issue`: CLI input vulnerabilities: fgets truncation executes leftover line, sscanf %u wraps negative to UINT32_MAX, NaN/out-of-range DAC volts [LCARD/main_cli.c:100]
- 2026-11-10T18:49:47Z `fix`: Added SIGINT handler, fgets overflow flush, strict strtoul/strtod parsers, and DAC/ADC bounds checks in main_cli.c; verified with CLI & unit tests [LCARD/main_cli.c:100]
