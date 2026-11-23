#include "factorial.h"

// __uint128_t нужен, чтобы (a * 2) и (result + a) не переполнялись,
// когда mod > 2^63.
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (uint64_t)(((__uint128_t)result + a) % mod);
    a = (uint64_t)(((__uint128_t)a * 2) % mod);
    b /= 2;
  }

  return result % mod;
}

uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1;

  // TODO: your code here
  ans %= args->mod; // случай mod == 1
  if (args->begin > args->end) // пустой диапазон
    return ans;

  for (uint64_t i = args->begin;; i++) {
    ans = MultModulo(ans, i, args->mod);
    if (i == args->end) // проверка внутри, чтобы не зациклиться при end = UINT64_MAX
      break;
  }

  return ans;
}