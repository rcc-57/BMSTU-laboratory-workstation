# Graph Report - LCARD  (2026-11-10)

## Corpus Check
- 8 files · ~14,682 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 97 nodes · 135 edges · 5 communities
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 12 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `87bb944b`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- x502api.h
- l502api_compat.h
- lcard_pstdint.h
- e502_ctrl.c
- e502api.h

## God Nodes (most connected - your core abstractions)
1. `main()` - 15 edges
2. `E502_StandApplyConfig()` - 6 edges
3. `E502_StandInitStruct()` - 5 edges
4. `E502_StandConnectUSB()` - 5 edges
5. `print_status()` - 5 edges
6. `E502_StandConnectIP()` - 4 edges
7. `E502_StandReadAdcFrame()` - 4 edges
8. `E502_StandDisconnect()` - 3 edges
9. `E502_StandSetChannelCount()` - 3 edges
10. `E502_StandConfigChannel()` - 3 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `E502_StandListUSB()`  [INFERRED]
  main_cli.c → e502_ctrl.c
- `main()` --calls--> `E502_StandInitStruct()`  [INFERRED]
  main_cli.c → e502_ctrl.c
- `main()` --calls--> `E502_StandConnectUSB()`  [INFERRED]
  main_cli.c → e502_ctrl.c
- `main()` --calls--> `E502_StandDisconnect()`  [INFERRED]
  main_cli.c → e502_ctrl.c
- `main()` --calls--> `E502_StandSetChannelCount()`  [INFERRED]
  main_cli.c → e502_ctrl.c

## Import Cycles
- None detected.

## Communities (5 total, 0 thin omitted)

### Community 0 - "x502api.h"
Cohesion: 0.06
Nodes (30): st_x502, st_x502_devrec_inptr, t_x502_adc_range, t_x502_cbr, t_x502_cbr_coef, t_x502_dac_ch, t_x502_dacout_flags, t_x502_dev_flags (+22 more)

### Community 1 - "l502api_compat.h"
Cohesion: 0.08
Nodes (22): t_l502_adc_range, t_l502_cbr, t_l502_cbr_coef, t_l502_dac_ch, t_l502_dacout_flags, t_l502_dev_flags, t_l502_digout_word_flags, t_l502_dma_ch (+14 more)

### Community 2 - "lcard_pstdint.h"
Cohesion: 0.15
Nodes (4): mode_to_str(), print_help(), print_status(), range_to_str()

### Community 3 - "e502_ctrl.c"
Cohesion: 0.35
Nodes (14): E502_StandApplyConfig(), E502_StandBlinkLed(), E502_StandConfigChannel(), E502_StandConnectIP(), E502_StandConnectUSB(), E502_StandDisconnect(), E502_StandGetErrorStr(), E502_StandInitStruct() (+6 more)

### Community 4 - "e502api.h"
Cohesion: 0.25
Nodes (7): st_e502_eth_config_state, st_e502_eth_svc_browse_context, st_e502_eth_svc_record, t_e502_eth_config_hnd, t_e502_eth_svc_browse_hnd, t_e502_eth_svc_event, t_e502_eth_svc_record_hnd

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `main()` connect `e502_ctrl.c` to `lcard_pstdint.h`?**
  _High betweenness centrality (0.030) - this node is a cross-community bridge._
- **Are the 12 inferred relationships involving `main()` (e.g. with `E502_StandApplyConfig()` and `E502_StandBlinkLed()`) actually correct?**
  _`main()` has 12 INFERRED edges - model-reasoned connections that need verification._
- **Should `x502api.h` be split into smaller, more focused modules?**
  _Cohesion score 0.0625 - nodes in this community are weakly interconnected._
- **Why does `print_status()` connect `lcard_pstdint.h` to `e502_ctrl.c`?**
  _High betweenness centrality (0.024) - this node is a cross-community bridge._
- **Should `l502api_compat.h` be split into smaller, more focused modules?**
  _Cohesion score 0.08333333333333333 - nodes in this community are weakly interconnected._