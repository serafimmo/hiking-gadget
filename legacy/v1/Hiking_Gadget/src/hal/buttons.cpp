#include "buttons.h"
#include "../config.h"
#include <Arduino.h>

static const uint8_t PIN_MAP[4] = {PIN_K1, PIN_K2, PIN_K3, PIN_K4};

struct BtnState {
    bool     lastRaw;
    bool     debounced;
    uint32_t changeMs;
    bool     pressReported;
};

static BtnState    states[4];
static ButtonEvent queue[8];
static uint8_t     qHead = 0;
static uint8_t     qTail = 0;

static void enqueue(ButtonEvent ev) {
    uint8_t next = (qTail + 1) % 8;
    if (next != qHead) { queue[qTail] = ev; qTail = next; }
}

void Buttons::begin() {
    for (uint8_t i = 0; i < 4; i++) {
        pinMode(PIN_MAP[i], INPUT_PULLUP);
        states[i] = {false, false, 0, false};
    }
}

void Buttons::poll() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < 4; i++) {
        bool raw = (digitalRead(PIN_MAP[i]) == LOW);
        if (raw != states[i].lastRaw) {
            states[i].lastRaw  = raw;
            states[i].changeMs = now;
        }
        if ((now - states[i].changeMs) >= DEBOUNCE_MS) {
            if (raw && !states[i].debounced) {
                states[i].debounced     = true;
                states[i].pressReported = false;
            } else if (!raw && states[i].debounced) {
                states[i].debounced = false;
                if (!states[i].pressReported) {
                    enqueue(static_cast<ButtonEvent>(i + 1)); // K1_SHORT..K4_SHORT
                }
            }
        }
        // Long press: K3 only (action button for decrement in settings)
        if (i == 2 && states[i].debounced && !states[i].pressReported) {
            if ((now - states[i].changeMs) >= LONG_PRESS_MS) {
                enqueue(ButtonEvent::K3_LONG);
                states[i].pressReported = true;
            }
        }
    }
}

bool Buttons::hasEvent()    { return qHead != qTail; }

ButtonEvent Buttons::dequeue() {
    if (qHead == qTail) return ButtonEvent::NONE;
    ButtonEvent ev = queue[qHead];
    qHead = (qHead + 1) % 8;
    return ev;
}
