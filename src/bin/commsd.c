#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <pthread.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h> 

#include <sys/queue.h>

#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include "utils.h"
#include "commsd.h"
#include "pei.h"

int amqp_channel_max = 0;
int amqp_frame_max = 131072;
int amqp_heartbeat = 0;

// The following settings should be put in a config file
int amqp_port = 5672;
char const *amqp_hostname = "localhost";
char const *amqp_username = "farics";
char const *amqp_password = "farics";
char const *amqp_vhost = "/farics";
char const *amqp_exchange = "test";
char const *amqp_routingkey = "test";
char const *serial_interface = "/dev/ttyUSB0";
int serial_baudrate = 9600;

amqp_socket_t *amqp_socket = NULL;
amqp_connection_state_t amqp_conn;

pthread_t serial_read_thr;
pthread_t serial_write_thr;

int main(int argc, char const *const *argv)
{
  int fd;
  void *status;
  char const *dev_init = "ATZ\r\n";
  char const *dev_init2 = "AT+CTSP=2,0,0\r\n";
  char const *dev_init3 = "AT+CTSP=1,1,11\r\n";
  amqp_init();

  fd = open(serial_interface, O_RDWR | O_NOCTTY); 
  if(0 > fd) 
  {
    perror(serial_interface); 
    exit(-1); 
  }

  // Initialize serial port
  set_serial_attrs(fd, serial_baudrate, 0, 20);

  // Create reader tread
  if(pthread_create(&serial_read_thr, NULL, &serial_reader_thread, (void *)&fd)) {
    fprintf(stderr, "Error creating reader thread\n");
    return 1;
  }

  // Create console tread
  if(pthread_create(&serial_write_thr, NULL, &serial_console_thread, (void *)&fd)) {
    fprintf(stderr, "Error creating console thread\n");
    return 1;
  }

  // Join reader tread
  if(pthread_join(serial_read_thr, &status)) {
    fprintf(stderr, "Error joining reader thread\n");
    return 2;
  }

  // Join console tread
  if(pthread_join(serial_write_thr, &status)) {
    fprintf(stderr, "Error joining console thread\n");
    return 2;
  }

  amqp_close();
  
  close(fd);
  return 0;
}

void *serial_console_thread(void *parameters) {
  char read_buf[MAX_STR_LEN];
  int fd,loop,len;


  loop = 1;
  fd = *((int*)parameters);
  while(loop)
  {
    memset(read_buf, 0, MAX_STR_LEN);
    len = scanf("%s", read_buf);
    if(len == -1)
    {
       fprintf(stderr, "Error joining console thread\n");
       pthread_exit(4);
    }
    write(fd, read_buf, strlen(read_buf));
    write(fd, CRLF, strlen(CRLF));
  }

  pthread_exit(0);
}


void *serial_reader_thread(void *parameters) {

  char read_buf[MAX_STR_LEN];
  int loop,fd,pos;
  ssize_t len;

  loop = 1;
  fd = *((int*)parameters);
  pos = 0;
  memset(read_buf, 0, MAX_STR_LEN);

  while(loop)
  {
    // Read characters
    while(pos < MAX_STR_LEN)
    {
      len = read(fd, read_buf+pos, 1);
      if(read_buf[pos]  == '\n' || read_buf[pos] == '\r')
      {
        break;
      }

      if(len > 0)
        pos++;
      else
        usleep(200);
    }

    // Terminate string
    read_buf[pos] = '\0';

    // Ignore empty lines
    if(pos > 0) {
	process_serial_input(read_buf);
    } else {
      //printf("got empty line\n");
    }

    pos = 0;
    memset(read_buf, 0, MAX_STR_LEN);
  }

  pthread_exit(0);
}

void process_serial_input(const char *line)
{
  char helper_str[MAX_STR_LEN];
  int err_num = 0;

  memset(helper_str, '\0', sizeof(helper_str));

  if(!strncmp(line,PEI_OK, strlen(PEI_OK)))
  {
     printf("OK received.\n"); 
  }
  else if(!strncmp(line,PEI_ERR, strlen(PEI_ERR-2))) {
     sscanf(line, PEI_ERR, &err_num);

     printf("ERROR %d received.\n", err_num);
  }
  printf("%s\n", line);
  amqp_publish(line);
}

void amqp_init()
{
  int status;

  amqp_conn = amqp_new_connection();

  amqp_socket = amqp_tcp_socket_new(amqp_conn);
  if (!amqp_socket) {
    die("creating TCP socket");
  }

  status = amqp_socket_open(amqp_socket, amqp_hostname, amqp_port);
  if (status) {
    die("opening TCP socket");
  }

  die_on_amqp_error(amqp_login(amqp_conn, amqp_vhost, amqp_channel_max, amqp_frame_max, amqp_heartbeat, AMQP_SASL_METHOD_PLAIN, amqp_username, amqp_password),
                    "Logging in");
  amqp_channel_open(amqp_conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(amqp_conn), "Opening channel");

}

void amqp_publish(char const *messagebody)
{
  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
  props.content_type = amqp_cstring_bytes("text/plain");
  props.delivery_mode = 2; /* persistent delivery mode */
  die_on_error(amqp_basic_publish(amqp_conn,
                                  1,
                                  amqp_cstring_bytes(amqp_exchange),
                                  amqp_cstring_bytes(amqp_routingkey),
                                  0,
                                  0,
                                  &props,
                                  amqp_cstring_bytes(messagebody)),
               "Publishing");

}

void amqp_close()
{
  die_on_amqp_error(amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS), "Closing channel");
  die_on_amqp_error(amqp_connection_close(amqp_conn, AMQP_REPLY_SUCCESS), "Closing connection");
  die_on_error(amqp_destroy_connection(amqp_conn), "Ending connection");
}

int set_serial_attrs(int fd, int speed, int parity, int waitTime)
{
  int isBlockingMode;
  struct termios tty;
        
  isBlockingMode = 0;
  if(waitTime < 0 || waitTime > 255)
    isBlockingMode = 1;
   
   memset (&tty, 0, sizeof tty);
   if (tcgetattr (fd, &tty) != 0) /* save current serial port settings */
   {
     printf("__LINE__ = %d, error %s\n", __LINE__, strerror(errno));
     return -1;
   }

   cfsetospeed (&tty, speed);
   cfsetispeed (&tty, speed);

   tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
   // disable IGNBRK for mismatched speed tests; otherwise receive break
   // as \000 chars
   tty.c_iflag &= ~IGNBRK;         // disable break processing
   tty.c_lflag = 0;                // no signaling chars, no echo,
                                   // no canonical processing
   tty.c_oflag = 0;                // no remapping, no delays
   tty.c_cc[VMIN]  = (1 == isBlockingMode) ? 1 : 0;            // read doesn't block
   tty.c_cc[VTIME] =  (1 == isBlockingMode)  ? 0 : waitTime;   // in unit of 100 milli-sec for set timeout value
   tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
   tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls,
                                    // enable reading
   tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
   tty.c_cflag |= parity;
   tty.c_cflag &= ~CSTOPB;
   tty.c_cflag &= ~CRTSCTS;

   if (tcsetattr (fd, TCSANOW, &tty) != 0)
   {
      printf("__LINE__ = %d, error %s\n", __LINE__, strerror(errno));
      return -1;
   }
   return 0;
}
