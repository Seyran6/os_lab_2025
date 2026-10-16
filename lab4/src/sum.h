#ifndef SUM_H
#define SUM_H

struct SumArgs {
  int *array;
  unsigned int begin;      /* включительно */
  unsigned int end;        /* не включительно */
  long long result;        /* сюда поток записывает частичную сумму */
};

/* Сумма элементов array[begin .. end) */
long long Sum(const struct SumArgs *args);

#endif