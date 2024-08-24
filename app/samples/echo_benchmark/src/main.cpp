#include "liburing.h"

int main(int , char *[])
{
    io_uring ring;
    return io_uring_queue_init(1, &ring, 0);
}