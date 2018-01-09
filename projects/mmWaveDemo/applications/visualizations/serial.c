/*
*  Source: http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c/38318768#comment71422395_38318768
*  Compile: g++ SerialExample.cpp -o SerialExample
*  Run: ./SerialExample
*
*/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        printf("Error tcsetattr: %s\n", strerror(errno));
}

int open_serial(const char * portname)
{
    const char *port = portname;
    int fd;
    // to access serial port on linux, you need to open file.
    fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        return -1;
    }
    /*baudrate 115200, 8 bits, no parity, 1 stop bit */
    set_interface_attribs(fd, B115200);
    //set_mincount(fd, 0);                /* set to pure timed read */
    return fd;
}

/*
*   Read data from serial port into buffer array.  fd is int returned from open_serial.
*/
int read_serial(int fd, unsigned char *buffer)
{
    /* Loop until data is read */
    do {
        int rdlen;

        rdlen = read(fd, buffer, sizeof(buffer) - 1);
        if (rdlen > 0) {

            buffer[rdlen] = 0;
            //printf("Read %d: \"%s\"\n", rdlen, buffer);
            return 0;

        } else if (rdlen < 0) {
            printf("Error from read: %d: %s\n", rdlen, strerror(errno));
            return -1;
        }
        /* repeat read to get full message */
    } while (1);
}

/*
*
*/
int write_serial(int fd, const char *data) {
    int datalen = strlen(data);
    printf("Length = %d", datalen);
    int wrlen = write(fd, data, datalen);
    if (wrlen != datalen) {
        printf("Error from write: %d, %d\n", wrlen, errno);
        return -1;
    }
    tcdrain(fd);    /* delay for output */
    return 0;
}

int main() {
    int fd;

    const char *port_name = "/dev/ttyACM0";
    fd = open_serial(port_name);

    // Example to write data.
    int write = write_serial(fd, "1,0,250");

    // Example for reading data.
    unsigned char buf[80];
    int read = read_serial(fd,buf);
    printf("Data = %s\n", buf);

}