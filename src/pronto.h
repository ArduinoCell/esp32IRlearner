#pragma once
#include <Arduino.h>

/**
 * rawToProntoHex
 * Converts a raw IR timing buffer (in microseconds) to Pronto HEX format.
 * Pronto HEX is the standard format accepted by Homey IR blasters and
 * universal IR databases (JP1, RemoteCentral, etc.).
 *
 * Pronto HEX structure:
 *   Word 0: 0000  (learned code type)
 *   Word 1: frequency code  (oscillator period / 0.241246)
 *   Word 2: burst pair count – once sequence
 *   Word 3: burst pair count – repeat sequence (0 = none)
 *   Remaining words: mark/space pairs expressed as oscillator cycles
 */
inline String rawToProntoHex(const uint16_t *rawbuf, uint16_t length,
                              uint32_t freqHz = 38000) {
    if (!rawbuf || length < 2) return "";

    // Pronto oscillator period code (1 / (freq * 0.241246e-6))
    uint16_t freqCode = (uint16_t)round(1000000.0 / (freqHz * 0.241246));
    float cycleUs = 1000000.0 / (double)freqHz;

    // Burst pairs (round up so we always have even number of pulse slots)
    uint16_t pairCount = (length + 1) / 2;

    // Build string
    String out;
    out.reserve(pairCount * 10 + 20);

    char buf[12];
    snprintf(buf, sizeof(buf), "0000 %04X ", freqCode);
    out += buf;
    snprintf(buf, sizeof(buf), "%04X 0000", pairCount);
    out += buf;

    for (uint16_t i = 0; i < length; i++) {
        uint16_t cycles = (uint16_t)round((float)rawbuf[i] / cycleUs);
        if (cycles == 0) cycles = 1;
        snprintf(buf, sizeof(buf), " %04X", cycles);
        out += buf;
    }

    // Pad to even number of words with a short space
    if (length % 2 == 1) {
        uint16_t padCycles = (uint16_t)round(1000.0 / cycleUs);
        if (padCycles == 0) padCycles = 1;
        snprintf(buf, sizeof(buf), " %04X", padCycles);
        out += buf;
    }

    return out;
}
