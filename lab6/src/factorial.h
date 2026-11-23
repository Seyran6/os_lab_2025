#ifndef FACTORIAL_H
#define FACTORIAL_H

#include <stdint.h>

// Задача для вычисления произведения begin * (begin+1) * ... * end по модулю mod.
struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result; // сюда поток кладёт ответ
};

// (a * b) % mod без переполнения uint64_t.
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

// Произведение чисел begin..end (включительно) по модулю mod.
// Пустой диапазон (begin > end) даёт 1 % mod.
uint64_t Factorial(const struct FactorialArgs *args);

#endif