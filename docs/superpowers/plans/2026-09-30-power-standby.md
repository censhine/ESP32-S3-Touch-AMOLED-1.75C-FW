# Round Badge Power Standby Implementation Plan

> For agentic workers: use subagent-driven-development for component changes and independent review. Working ledger: artifacts/power-optimization-20260930/progress.md.

**Goal:** Stop idle applications safely and put the whole board into explicit light sleep with POWER wake.

**Architecture:** Keep 120 s display blanking. A one-second LVGL timer observes manager state on the existing GUI stack and requests normal STOP at 300 s foreground inactivity or 120 s background residency, and verifies removal. Once dark and no application remains, a separate worker stops audio, quiesce Wi-Fi/status work, pause LVGL and enter GPIO-wake light sleep. Restore the panel/UI/network after wake without rebooting or changing saved Wi-Fi preference.

**Tech Stack:** ESP-IDF 5.5, ESP32-S3, Brookesia, LVGL 9, AXP2101/CO5300.

## Global Constraints
- Foreground idle timeout 300000 ms, background timeout 120000 ms, service sampling 1000 ms.
- No force-deleting worker tasks; cleanup failure retains resources and prevents standby.
- Never erase user NVS, storage, media or partition table; any firmware flash is application-only after fresh verified backup.
- No invented battery capacity or current. Estimates are conditional, measurement limits explicit.
- POWER GPIO3 active high wakes; BOOT behavior stays unchanged; consume wake press so release cannot turn display off again.
- CPU sleep and waiting for status/LVGL tasks run outside the LVGL lock; panel commands remain serialized by it.

## Task 1: Runtime cleanup and timeout policy
- [ ] Extract Xiaozhi heavy runtime start/stop from install/uninstall; ensure close stops workers and reopen restarts.
- [ ] Join SpecAnalyzer worker on close; propagate Gravitysphere/ButtonTest stop timeouts rather than claiming cleanup.
- [ ] Add allocation-free app idle policy and actual manager STOP integration; retry failed closes no faster than 30 s.
- [ ] Test thresholds, user activity, independent background timing, transitions, failed-close backoff, removed/restarted app and uint32 rollover.

## Task 2: Whole-system standby
- [ ] Add system_status::set_standby(bool), temporarily stopping radio/reconnect/monitor work and preserving NVS preference; bounded quiescence acknowledgement.
- [ ] Add power-management worker: dark + empty manager + no audio owner + no media leases + released keys, then pause adapter, close codec, sleep panel and CPU with GPIO wake.
- [ ] Restore panel brightness, full redraw, input and Wi-Fi; unwind failures and rate-limit retries.
- [ ] Enable CPU dynamic frequency scaling (80–240 MHz), automatic light sleep disabled; explicit sleep has no polling timer wake.
- [ ] Test screen wake consumption and actual standby transaction rollback through host seams; build firmware.

## Task 3: Evidence and delivery
- [ ] Run host regressions and full SDK build; independent review, fix material findings.
- [ ] Back up connected device and install only factory app when validation allows; observe standby/wake or clearly report unverified behavior.
- [ ] Document cause, behavior, battery-runtime formula/scenarios and required whole-board current measurement.
