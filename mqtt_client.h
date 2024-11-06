#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#define MQTT_PORT 1883
#define MQTT_KEEPALIVE 60
#define BUFFER_SIZE 1024

int mqtt_connect(int socktfd, const char *client_id);
int mqtt_publish(int socktfd, const char *topic, const char *message);
int mqtt_subscribe(int socktfd, const char *topic);
int mqtt_ping(int socktfd);
void mqtt_disconnect(int socktfd);

#endif // ! MQTT_CLIENT_H