#ifndef NETIO_H
#define NETIO_H

#include <stdbool.h>
#include <stddef.h>

// send/recv могут передать меньше байт, чем запрошено, поэтому
// эти функции докручивают операцию в цикле до конца.

// true - отправлено ровно len байт.
bool SendAll(int fd, const char *buf, size_t len);

// Возвращает: 1 - прочитано ровно len байт,
//             0 - собеседник закрыл соединение до первого байта,
//            -1 - ошибка recv,
//            -2 - соединение закрыто посреди сообщения.
int RecvAll(int fd, char *buf, size_t len);

#endif