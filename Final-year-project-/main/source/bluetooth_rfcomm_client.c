#include "ssd1306_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

int main(int argc, char **argv)
{
    struct sockaddr_rc addr = { 0 };
    int s, status;
    const char *dest = (argc > 1) ? argv[1] : "01:23:45:67:89:AB";
    const char *msg  = (argc > 2) ? argv[2] : "NOTIF|WHATSAPP|SmartGlasses|Hello from RFCOMM client!\n";

    // Allocate an RFCOMM Bluetooth socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s < 0) {
        perror("Failed to allocate Bluetooth socket");
        return 1;
    }

    // Set the connection parameters (target Bluetooth MAC and RFCOMM channel 1)
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) 1;
    str2ba(dest, &addr.rc_bdaddr);

    // Connect to the Smart Glasses server
    status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (status == 0) {
        status = write(s, msg, strlen(msg));
        printf("Sent %d bytes to %s\n", status, dest);
    } else {
        perror("Bluetooth RFCOMM connection failed");
    }

    close(s);
    return 0;
}
