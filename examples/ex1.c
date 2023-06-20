#include "utils.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


char buf[16];


int main()
{
    int dht = open("/dev/dht22", O_RDONLY);

    if (dht == -1)
    {
        printf("Open failed. Errno is %d\n", errno);
        return 1;
    }

    ssize_t nbytes = read(dht, (void*) buf, sizeof(buf) - 1);

    if (nbytes < 0)
    {
        printf("Read failed. Errno is %d\n", errno);
        return 1;
    }

    if (close(dht) == -1)
    {
        printf("Close failed. Errno is %d\n", errno);
        return 1;
    }

    if (nbytes > 0)
    {
        printf("Read %d bytes: \"%s\"\n", nbytes, buf);
        pprint_data(buf);
    } 
    else 
    {
        printf("No data available\n");
    }

    return 0;
}
