#include "utils.h"
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>


#define UPDATE_INTERVAL 5


void* sensor_thread(void* arg) 
{
    char buf[16];
    int dht = (int) arg;
    time_t current_time = 0;

    while (1) 
    {
        printf("\033c"); /* Clear console */
        current_time = time(NULL);
        printf("%s", ctime(&current_time));

        memset(buf, 0, sizeof(buf));

        lseek(dht, 0, SEEK_SET);
        if (read(dht, buf, sizeof(buf) - 1) < 0) 
        {
            printf("Error while reading. Try again in %d seconds\n", UPDATE_INTERVAL);
            printf("Errno is %d\n", errno);
        } 
        else 
        {
            pprint_data(buf);
        }

        sleep(UPDATE_INTERVAL);
    }
}


int main() 
{
    struct termios original_termios;
    struct termios raw_termios;

    tcgetattr(STDIN_FILENO, &original_termios);
    raw_termios = original_termios;
    raw_termios.c_lflag &= ~(ICANON | ECHO); /* Put console into raw mode */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);


    int dht = open("/dev/dht22", O_RDONLY);

    if (dht == -1)
    {
        printf("Open failed. Errno is %d\n", errno);
        return 1;
    }
    
    pthread_t sensor_tid;
    pthread_create(&sensor_tid, NULL, sensor_thread, (void*) dht);

    while (getchar() != 'q');

    pthread_cancel(sensor_tid);
    pthread_join(sensor_tid, NULL);

    
    close(dht);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    return 0;
}
