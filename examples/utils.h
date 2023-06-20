#ifndef DHT22_EXAMPLE_UTILS_H
#define DHT22_EXAMPLE_UTILS_H


#include <stdio.h>
#include <string.h>
#include <stdlib.h>


void pprint_data(char* buf) 
{
    float humidity = 0.0f;
    float temperature = 0.0f;
    sscanf(buf, "%f,%f", &humidity, &temperature);
    printf("\n\t%.1f RH\n\t%.1f °C\n\n", humidity, temperature);
}


#endif /* DHT22_EXAMPLE_UTILS_H */