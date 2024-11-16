#include "mqtt_client.h"

int encode_remaining_length(unsigned char *packet, int remaining_length)
{
    int index = 0;
    do
    {
        unsigned char encoded_byte = remaining_length % 128;
        remaining_length = remaining_length / 128;
        if (remaining_length > 0)
        {
            encoded_byte |= 0x80;
        }
        packet[index++] = encoded_byte;
    } while (remaining_length > 0);
    return index;
}

int mqtt_connect(int socktfd, const char *client_id, const char *username, const char *password)
{
    unsigned char packet[BUFFER_SIZE];
    int index = 0;

    packet[index++] = 0x10; // CONNECT control packet type
    int remaining_length_index = index;
    int remaining_length = 0;
    index++;

    // Variable header
    packet[index++] = 0x00;
    packet[index++] = 0x04;
    packet[index++] = 'M';
    packet[index++] = 'Q';
    packet[index++] = 'T';
    packet[index++] = 'T';
    packet[index++] = 0x04;
    packet[index++] = 0xC2;
    packet[index++] = 0x00;
    packet[index++] = MQTT_KEEPALIVE;

    int client_id_len = strlen(client_id);
    packet[index++] = (client_id_len >> 8) & 0xFF;
    packet[index++] = client_id_len & 0xFF;
    memcpy(&packet[index], client_id, client_id_len);
    index += client_id_len;

    int username_len = strlen(username);
    packet[index++] = (username_len >> 8) & 0xFF;
    packet[index++] = username_len & 0xFF;
    memcpy(&packet[index], username, username_len);
    index += username_len;

    int password_len = strlen(password);
    packet[index++] = (password_len >> 8) & 0xFF;
    packet[index++] = password_len & 0xFF;
    memcpy(&packet[index], password, password_len);
    index += password_len;

    remaining_length = index - (remaining_length_index + 1);
    unsigned char remaining_length_bytes[4];
    int remaining_length_size = encode_remaining_length(remaining_length_bytes, remaining_length);

    memmove(&packet[remaining_length_index + remaining_length_size], &packet[remaining_length_index + 1], remaining_length);
    memcpy(&packet[remaining_length_index], remaining_length_bytes, remaining_length_size);

    index = remaining_length_index + remaining_length_size + remaining_length;

    if (send(socktfd, packet, index, 0) < 0)
    {
        perror("ERROR: Sending CONNECT packet!");
        return -1;
    }

    unsigned char response[4];
    if (recv(socktfd, response, 4, 0) < 0)
    {
        perror("ERROR: Receiving CONNACK!");
        return -1;
    }

    if (response[0] != 0x20 || response[1] != 0x02 || response[3] != 0x00)
    {
        fprintf(stderr, "ERROR: Invalid CONNACK packet! Error code: %d\n", response[3]);
        return -1;
    }

    printf("Connected successfully to MQTT broker\n");
    return 0;
}

int mqtt_publish(int socktfd, const char *topic, const char *message)
{
    unsigned char packet[BUFFER_SIZE];
    int index = 0;
    static unsigned short packet_id = 1;

    packet[index++] = 0x32; // PUBLISH with QoS 1
    int remaining_length_index = index;
    int remaining_length = 0;
    index++;

    int topic_len = strlen(topic);
    packet[index++] = (topic_len >> 8) & 0xFF;
    packet[index++] = topic_len & 0xFF;
    memcpy(&packet[index], topic, topic_len);
    index += topic_len;

    packet[index++] = (packet_id >> 8) & 0xFF;
    packet[index++] = packet_id & 0xFF;

    int message_len = strlen(message);
    memcpy(&packet[index], message, message_len);
    index += message_len;

    remaining_length = index - (remaining_length_index + 1);
    unsigned char remaining_length_bytes[4];
    int remaining_length_size = encode_remaining_length(remaining_length_bytes, remaining_length);

    memmove(&packet[remaining_length_index + remaining_length_size], &packet[remaining_length_index + 1], remaining_length);
    memcpy(&packet[remaining_length_index], remaining_length_bytes, remaining_length_size);

    index = remaining_length_index + remaining_length_size + remaining_length;

    if (send(socktfd, packet, index, 0) < 0)
    {
        perror("ERROR: Sending PUBLISH packet!");
        return -1;
    }

    printf("Message published on topic '%s'\n", topic);
    packet_id++;
    return 0;
}

int mqtt_subscribe(int socktfd, const char *topic)
{
    unsigned char packet[BUFFER_SIZE];
    int index = 0;
    static unsigned short packet_id = 1;

    packet[index++] = 0x82; // SUBSCRIBE packet type with QoS 1
    int remaining_length_index = index;
    int remaining_length = 0;
    index++;

    packet[index++] = (packet_id >> 8) & 0xFF;
    packet[index++] = packet_id & 0xFF;
    packet_id++;

    int topic_len = strlen(topic);
    packet[index++] = (topic_len >> 8) & 0xFF;
    packet[index++] = topic_len & 0xFF;
    memcpy(&packet[index], topic, topic_len);
    index += topic_len;

    packet[index++] = 0x01; // Requested QoS level

    remaining_length = index - (remaining_length_index + 1);
    unsigned char remaining_length_bytes[4];
    int remaining_length_size = encode_remaining_length(remaining_length_bytes, remaining_length);

    memmove(&packet[remaining_length_index + remaining_length_size], &packet[remaining_length_index + 1], remaining_length);
    memcpy(&packet[remaining_length_index], remaining_length_bytes, remaining_length_size);

    index = remaining_length_index + remaining_length_size + remaining_length;

    if (send(socktfd, packet, index, 0) < 0)
    {
        perror("ERROR: Sending SUBSCRIBE packet!");
        return -1;
    }

    printf("Subscribed to topic '%s'\n", topic);
    return 0;
}

int mqtt_ping(int socktfd)
{
    unsigned char packet[2] = {0xC0, 0x00};

    if (send(socktfd, packet, 2, 0) < 0)
    {
        perror("ERROR: Sending PINGREQ packet!");
        return -1;
    }

    unsigned char response[2];
    if (recv(socktfd, response, 2, 0) < 0)
    {
        perror("ERROR: Receiving PINGRESP!");
        return -1;
    }

    if (response[0] != 0xD0 || response[1] != 0x00)
    {
        fprintf(stderr, "ERROR: Invalid PINGRESP packet!\n");
        return -1;
    }

    printf("PINGRESP received from broker\n");
    return 0;
}

void mqtt_disconnect(int socktfd)
{
    unsigned char packet[2] = {0xE0, 0x00};

    if (send(socktfd, packet, 2, 0) < 0)
    {
        perror("ERROR: Sending DISCONNECT packet!");
    }
    else
    {
        printf("Disconnected from MQTT broker\n");
    }
}
