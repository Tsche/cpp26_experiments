#pragma once

#define $stamp4(Offset, Fnc) Fnc(Offset) Fnc((Offset) + 1) Fnc((Offset) + 2) Fnc((Offset) + 3)
#define $stamp16(Offset, Fnc)  \
    $stamp4(Offset, Fnc)       \
    $stamp4((Offset) + 4, Fnc) \
    $stamp4((Offset) + 8, Fnc) \
    $stamp4((Offset) + 12, Fnc)
#define $stamp64(Offset, Fnc)    \
    $stamp16(Offset, Fnc)        \
    $stamp16((Offset) + 16, Fnc) \
    $stamp16((Offset) + 32, Fnc) \
    $stamp16((Offset) + 48, Fnc)
#define $stamp256(Offset, Fnc)    \
    $stamp64(Offset, Fnc)         \
    $stamp64((Offset) + 64, Fnc)  \
    $stamp64((Offset) + 128, Fnc) \
    $stamp64((Offset) + 192, Fnc)

#define $stamp(Count) $stamp##Count