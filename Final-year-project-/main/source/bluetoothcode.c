#include "ssd1306_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <pthread.h>

struct ps {
    int st;
    pthread_t *thr;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int status = 0;

void *recvsocket(void *arg)
{
    struct ps *p = (struct ps *)arg;
    int st = p->st;
    char s[1024];

    while (1) {
        memset(s, 0, sizeof(s));
        int rc = read(st, s, sizeof(s) - 1);
        if (rc <= 0)
            break;
        printf("phone:%s\n", s);
        printf("Display OLED\n");
        ssd1306_clearDisplay();
        ssd1306_drawString(s);
        ssd1306_display();
    }

    pthread_mutex_lock(&mutex);
    status = 0;
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *sendsocket(void *arg)
{
    int st = *(int *)arg;
    char s[1024];
    while (1) {
        memset(s, 0, sizeof(s));
        ssize_t n = read(STDIN_FILENO, s, sizeof(s) - 1);
        if (n <= 0) break;
        write(st, s, strlen(s));
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int s, client;
    socklen_t opt = sizeof(rem_addr);

    ssd1306_begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);

    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 1;
    bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));

    listen(s, 1);

    pthread_t thrd1, thrd2;
    while (1) {
        client = accept(s, (struct sockaddr *)&rem_addr, &opt);
        if (client < 0) continue;

        ba2str(&rem_addr.rc_bdaddr, buf);
        fprintf(stderr, "accepted connection from %s\n", buf);

        struct ps ps1;
        ps1.st = client;
        ps1.thr = &thrd2;
        pthread_create(&thrd1, NULL, recvsocket, &ps1);
        pthread_detach(thrd1);
        pthread_create(&thrd2, NULL, sendsocket, &client);
        pthread_detach(thrd2);
    }

    close(s);
    return 0;
}
