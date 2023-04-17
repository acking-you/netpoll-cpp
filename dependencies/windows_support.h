#pragma once

struct iovec
{
   void *iov_base; /* Starting address */
   int   iov_len;  /* Number of bytes */
};

int readv(int fd, const struct iovec *vector, int count);