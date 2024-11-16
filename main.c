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

// Function to handle incoming messages
void *listen_for_messages(void *arg)
{
    while (1)
    {
        unsigned char buffer[BUFFER_SIZE];
        int bytes_received = recv(socktfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0)
        {
            int index = 0;
            unsigned char packet_type = buffer[0] & 0xF0;

            // Process PUBLISH packets
            if (packet_type == 0x30)
            {
                index++;
                int multiplier = 1;
                int remaining_length = 0;
                unsigned char encoded_byte;

                do
                {
                    encoded_byte = buffer[index++];
                    remaining_length += (encoded_byte & 127) * multiplier;
                    multiplier *= 128;
                } while ((encoded_byte & 128) != 0);

                // Read the topic name
                int topic_len = (buffer[index++] << 8) | buffer[index++];
                char topic_received[topic_len + 1];
                memcpy(topic_received, &buffer[index], topic_len);
                topic_received[topic_len] = '\0';
                index += topic_len;

                // Read the message payload
                int payload_len = remaining_length - (index - 2);
                char message_received[payload_len + 1];
                memcpy(message_received, &buffer[index], payload_len);
                message_received[payload_len] = '\0';

                printf("Message received on topic '%s': %s\n", topic_received, message_received);
            }
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

    pthread_t listen_thread;
    pthread_create(&listen_thread, NULL, listen_for_messages, NULL);

    // Command loop to send messages
    char input[BUFFER_SIZE];
    while (true)
    {
        fgets(input, BUFFER_SIZE, stdin);
        if (strncmp(input, "trimite ", 8) == 0)
        {
            // Copy the message to a local array for safe modification
            char message[BUFFER_SIZE];
            strncpy(message, input + 8, BUFFER_SIZE - 1); // Skip "trimite " and copy the rest
            message[BUFFER_SIZE - 1] = '\0';              // Ensure null termination

            // Remove newline character
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
    mqtt_disconnect(socktfd);
    close(socktfd);

    return 0;
}
