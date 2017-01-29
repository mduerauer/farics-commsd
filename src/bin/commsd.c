#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

#include "utils.h"

int amqp_port = 5672;
int amqp_channel_max = 0;
int amqp_frame_max = 131072;
int amqp_heartbeat = 0;
char const *amqp_hostname = "localhost";
char const *amqp_username = "farics";
char const *amqp_password = "farics";
char const *amqp_vhost = "/farics";
char const *amqp_exchange = "test";
char const *amqp_routingkey = "test";
amqp_socket_t *amqp_socket = NULL;
amqp_connection_state_t amqp_conn;

int main(int argc, char const *const *argv)
{
  int status;
  char const *messagebody;

  messagebody = argv[1];

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

  die_on_amqp_error(amqp_channel_close(amqp_conn, 1, AMQP_REPLY_SUCCESS), "Closing channel");
  die_on_amqp_error(amqp_connection_close(amqp_conn, AMQP_REPLY_SUCCESS), "Closing connection");
  die_on_error(amqp_destroy_connection(amqp_conn), "Ending connection");
  return 0;
}
