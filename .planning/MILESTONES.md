# Milestones

## v1.0 MVP (Shipped: 2026-03-08)

**Phases completed:** 5 phases, 15 plans
**Timeline:** 4 days (2026-03-05 → 2026-03-08), 73 commits
**Codebase:** 2,408 LOC source + 2,532 LOC tests (C++)
**Tests:** 1,810 assertions passing

**Delivered:** A fully functional stereo 8-tap delay plugin with feedback routing matrix, tempo sync, and custom GUI — the sonic foundation of the Zeitraum product.

**Key accomplishments:**
- Stereo 8-tap delay engine with shared serial delay line, per-tap positioning, and analog character
- 12-source feedback routing matrix (8 taps + 4 preset mixes) with tanh saturation and RMS energy limiting
- Tempo sync with host BPM, 6 note divisions, and 120 BPM fallback
- Custom dark/teal GUI with draggable tap position bars, level faders, and feedback matrix editor
- Full DAW integration: 38 automatable parameters in organized groups, XML state persistence (v3)

---

