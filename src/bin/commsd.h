#ifndef commsd_h
#define commsd_h

#define MAX_STR_LEN 512

static const char *CRLF = "\r\n";

void amqp_init();

void amqp_publish(char const *messagebody);

void amqp_close();

void *serial_reader_thread(void *parameters);

void *serial_writer_thread(void *parameters);

void *serial_console_thread(void *parameters);

int set_serial_attrs(int fd, int speed, int parity, int waitTime);

void process_serial_input(const char* input);

#endif
