#include "main.h"

// [RECEIVER DATA]
const char* rx_morse_map[] = {
    ".-",    // A
    "-...",  // B
    "-.-.",  // C
    "-..",   // D
    ".",     // E
    "..-.",  // F
    "--.",   // G
    "....",  // H
    "..",    // I
    ".---",  // J
    "-.-",   // K
    ".-..",  // L
    "--",    // M
    "-.",    // N
    "---",   // O
    ".--.",  // P
    "--.-",  // Q
    ".-.",   // R
    "...",   // S
    "-",     // T
    "..-",   // U
    "...-",  // V
    ".--",   // W
    "-..-",  // X
    "-.--",  // Y
    "--.."   // Z
};

// [TRANSMITTER DATA]
// MORSE PATTERNS A–Z
// marking them as "static" so they remain private, main.c doesnt need to know about them
static const uint16_t pattern_a[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};                                          // .-
static const uint16_t pattern_b[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};    // -...
static const uint16_t pattern_c[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};   // -.-.
static const uint16_t pattern_d[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};                       // -..
static const uint16_t pattern_e[] = {TIME_DOT, GAP_CHAR};                                                              // .
static const uint16_t pattern_f[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};    // ..-.
static const uint16_t pattern_g[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};                      // --.
static const uint16_t pattern_h[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};     // ....
static const uint16_t pattern_i[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};                                           // ..
static const uint16_t pattern_j[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};  // .---
static const uint16_t pattern_k[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};                      // -.-
static const uint16_t pattern_l[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};    // .-..
static const uint16_t pattern_m[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};                                         // --
static const uint16_t pattern_n[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};                                          // -.
static const uint16_t pattern_o[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};                     // ---
static const uint16_t pattern_p[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};   // .--.
static const uint16_t pattern_q[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};  // --.-
static const uint16_t pattern_r[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_CHAR};                       // .-.
static const uint16_t pattern_s[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};                        // ...
static const uint16_t pattern_t[] = {TIME_DASH, GAP_CHAR};                                                             // -
static const uint16_t pattern_u[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};                       // ..-
static const uint16_t pattern_v[] = {TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};    // ...-
static const uint16_t pattern_w[] = {TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};                      // .--
static const uint16_t pattern_x[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_CHAR};   // -..-
static const uint16_t pattern_y[] = {TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DASH, GAP_CHAR};  // -.--
static const uint16_t pattern_z[] = {TIME_DASH, GAP_SYM, TIME_DASH, GAP_SYM, TIME_DOT, GAP_SYM, TIME_DOT, GAP_CHAR};   // --..
static const uint16_t pattern_space[] = {GAP_WORD};  

// [LOOKUP TABLE]
const MorseMapping_t morse_lookup_table[] = {
    {'A', pattern_a, sizeof(pattern_a)/sizeof(uint16_t)},
    {'B', pattern_b, sizeof(pattern_b)/sizeof(uint16_t)},
    {'C', pattern_c, sizeof(pattern_c)/sizeof(uint16_t)},
    {'D', pattern_d, sizeof(pattern_d)/sizeof(uint16_t)},
    {'E', pattern_e, sizeof(pattern_e)/sizeof(uint16_t)},
    {'F', pattern_f, sizeof(pattern_f)/sizeof(uint16_t)},
    {'G', pattern_g, sizeof(pattern_g)/sizeof(uint16_t)},
    {'H', pattern_h, sizeof(pattern_h)/sizeof(uint16_t)},
    {'I', pattern_i, sizeof(pattern_i)/sizeof(uint16_t)},
    {'J', pattern_j, sizeof(pattern_j)/sizeof(uint16_t)},
    {'K', pattern_k, sizeof(pattern_k)/sizeof(uint16_t)},
    {'L', pattern_l, sizeof(pattern_l)/sizeof(uint16_t)},
    {'M', pattern_m, sizeof(pattern_m)/sizeof(uint16_t)},
    {'N', pattern_n, sizeof(pattern_n)/sizeof(uint16_t)},
    {'O', pattern_o, sizeof(pattern_o)/sizeof(uint16_t)},
    {'P', pattern_p, sizeof(pattern_p)/sizeof(uint16_t)},
    {'Q', pattern_q, sizeof(pattern_q)/sizeof(uint16_t)},
    {'R', pattern_r, sizeof(pattern_r)/sizeof(uint16_t)},
    {'S', pattern_s, sizeof(pattern_s)/sizeof(uint16_t)},
    {'T', pattern_t, sizeof(pattern_t)/sizeof(uint16_t)},
    {'U', pattern_u, sizeof(pattern_u)/sizeof(uint16_t)},
    {'V', pattern_v, sizeof(pattern_v)/sizeof(uint16_t)},
    {'W', pattern_w, sizeof(pattern_w)/sizeof(uint16_t)},
    {'X', pattern_x, sizeof(pattern_x)/sizeof(uint16_t)},
    {'Y', pattern_y, sizeof(pattern_y)/sizeof(uint16_t)},
    {'Z', pattern_z, sizeof(pattern_z)/sizeof(uint16_t)},
    {' ', pattern_space, sizeof(pattern_space)/sizeof(uint16_t)}
};

// Total size of the lookup table
const size_t morse_lookup_length = sizeof(morse_lookup_table) / sizeof(morse_lookup_table[0]);