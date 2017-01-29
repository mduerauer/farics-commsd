#ifndef commsd_h
#define commsd_h

#define MAX_STR_LEN 512

void amqp_init();

void amqp_publish(char const *messagebody);

void amqp_close();

void *serial_reader_thread(void *parameters);

int set_serial_attrs(int fd, int speed, int parity, int waitTime);

#endif
