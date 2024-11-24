#include "mqtt_client.h"
#include <pthread.h>
#include <stdbool.h>

const char *broker_ip = "127.0.0.1";
// const char *broker_ip = "192.168.56.176";
int broker_port = MQTT_PORT;
const char *client_id = "client_c";
const char *username = "razvan";
const char *password = "razvan";
const char *topic = "test/topic";
int socktfd;

void *send_ping(void *arg)
{
    while (1)
    {
        time_t current_time = time(NULL);

        // verific dacă a trecut prea mult timp fără un PINGRESP
        if (last_pingresp_time != 0 && difftime(current_time, last_pingresp_time) > MQTT_KEEPALIVE)
        {
            fprintf(stderr, "ERROR: No PINGRESP received in %d seconds. Disconnecting...\n", MQTT_KEEPALIVE);
            break; // Ieși din buclă dacă timeout-ul este depășit
        }

        if (mqtt_ping(socktfd) < 0)
        {
            fprintf(stderr, "ERROR: Sending PINGREQ failed! Disconnecting...\n");
            break;
        }

        sleep(5); // Așteaptă 3 secunde înainte de următorul PINGREQ
    }

    return NULL;
}

void *listen_for_messages(void *arg)
{
    while (1)
    {
        unsigned char buffer[BUFFER_SIZE];
        int bytes_received = recv(socktfd, buffer, BUFFER_SIZE, 0);

        if (bytes_received < 0)
        {
            perror("ERROR: Receiving MQTT packet!");
            break;
        }
        else if (bytes_received == 0)
        {
            fprintf(stderr, "ERROR: Broker closed the connection!\n");
            break;
        }

        unsigned char packet_type = buffer[0] & 0xF0;

        switch (packet_type)
        {
        case 0x30: // PUBLISH
            process_publish(buffer, bytes_received);
            break;

        case 0x40: // PUBACK
            process_puback(buffer, bytes_received);
            break;

        case 0xD0: // PINGRESP
            printf("PINGRESP received from broker\n");
            last_pingresp_time = time(NULL); // Actualizează timpul ultimei recepții
            break;

        case 0x90: // SUBACK
            printf("SUBACK received\n");
            break;

        default:
            fprintf(stderr, "INFO: Unhandled packet type: 0x%X\n", packet_type);
            break;
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    socktfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socktfd < 0)
    {
        perror("ERROR: Creating socket!");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in broker_addr;
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port);

    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) <= 0)
    {
        perror("ERROR: Invalid broker address!");
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    if (connect(socktfd, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0)
    {
        perror("ERROR: Connecting to broker!");
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    if (mqtt_connect(socktfd, client_id, username, password) < 0)
    {
        fprintf(stderr, "ERROR: Sending CONNECT packet!\n");
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    if (mqtt_subscribe(socktfd, topic) < 0)
    {
        fprintf(stderr, "ERROR: Subscribing to topic!\n");
        mqtt_disconnect(socktfd);
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening for messages on topic '%s'. Type 'trimite Hello' to send 'Hello' to the topic.\n", topic);

    pthread_t ping_thread, listen_thread;

    if (pthread_create(&listen_thread, NULL, listen_for_messages, NULL) != 0)
    {
        perror("ERROR: Failed to create listen thread!");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&ping_thread, NULL, send_ping, NULL) != 0)
    {
        perror("ERROR: Failed to create ping thread!");
        pthread_cancel(listen_thread);
        exit(EXIT_FAILURE);
    }

    // send messages
    char input[BUFFER_SIZE];
    while (true)
    {
        fgets(input, BUFFER_SIZE, stdin);
        if (strncmp(input, "trimite ", 8) == 0)
        {
            char message[BUFFER_SIZE];
            strncpy(message, input + 8, BUFFER_SIZE - 1);
            message[BUFFER_SIZE - 1] = '\0';
            message[strcspn(message, "\n")] = '\0';

            if (mqtt_publish(socktfd, topic, message) < 0)
            {
                fprintf(stderr, "ERROR: Publishing message!\n");
                break;
            }
            printf("Message '%s' sent to topic '%s'\n", message, topic);
        }
    }
    pthread_cancel(listen_thread);
    pthread_cancel(ping_thread);
    mqtt_disconnect(socktfd);
    close(socktfd);

    return 0;
}
