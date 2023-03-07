#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


char buf[16];


void pprint_data()
{   
    float humidity = 0.F;
    float temperature = 0.F;
    char* tok = NULL;

    tok = strtok(buf, ",");
    humidity = atof(tok);

    tok = strtok(NULL, ",");
    temperature = atof(tok);

    printf("\n\t%.1f RH\n\t%.1f °C\n\n", humidity, temperature);
}


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
        pprint_data();
    } 
    else 
    {
        printf("No data available\n");
    }

    return 0;
}
