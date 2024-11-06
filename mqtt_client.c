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

int mqtt_connect(int socktfd, const char *client_id)
{
    unsigned char packet[BUFFER_SIZE];
    int index = 0;

    // header fix
    packet[index++] = 0x10; // connect control packet type

    int remaining_length_index = index;
    int remaining_length = 0;
    index++;

    // variable header
    // protocol name
    packet[index++] = 0x00;
    packet[index++] = 0x04;
    packet[index++] = 'M';
    packet[index++] = 'Q';
    packet[index++] = 'T';
    packet[index++] = 'T';

    // protocol level
    packet[index++] = 0x04;
    // flag
    packet[index++] = 0x02;
    // pentru keep alive
    packet[index++] = 0x00;
    packet[index++] = MQTT_KEEPALIVE;

    // Contruim payload-ul
    // ID client
    int client_id_len = strlen(client_id);
    packet[index++] = (client_id_len >> 8) & 0xFF;
    packet[index++] = client_id_len & 0xFF;
    memcpy(&packet[index], client_id, client_id_len);
    index += client_id_len;

    // se actualizeaza remaining length
    remaining_length = index - (remaining_length_index + 1);

    // codificarea remaining_length
    unsigned char remaining_length_bytes[4];
    int remaining_length_size = encode_remaining_length(remaining_length_bytes, remaining_length);

    memmove(&packet[remaining_length_index + remaining_length_size], &packet[remaining_length_index + 1], remaining_length);

    memcpy(&packet[remaining_length_index], remaining_length_bytes, remaining_length_size);

    index = remaining_length_index + remaining_length_size + remaining_length;

    // trimitem pachet- ul connect
    if (send(socktfd, packet, index, 0) < 0)
    {
        perror("EROARE: LA TRIMITEREA PACHETULUI CONNECT!");
        return -1;
    }

    // primim inapoi connack
    unsigned char response[4];
    if (recv(socktfd, response, 4, 0) < 0)
    {
        perror("EROARE: LA PRIMIREA CONNACK");
        return -1;
    }

    // verificam connack
    if (response[0] != 0x20 || response[1] != 0x02)
    {
        perror("EROARE: PACHET CONNACK INVALID!\n");
        return -1;
    }

    if (response[3] != 0x00)
    {
        fprintf(stderr, "EROARE: LA CINNECTARE, COD DE EROARE: %d\n", response[3]);
        return -1;
    }

    printf("CONEXIUNE REUSITA CU BROKER-ul MQTT\n");

    return 0;
}

int mqtt_publish(int socktfd, const char *topic, const char *message)
{
    unsigned char packet[BUFFER_SIZE];
    int index = 0;
    static unsigned short packet_id = 1;

    // Fixed Header
    unsigned char fixed_header = 0x30 | (1 << 1); // PUBLISH cu QoS 1
    packet[index++] = fixed_header;

    // Placeholder pentru Remaining Length
    int remaining_length_index = index;
    int remaining_length = 0;
    index++; // Rezervăm spațiu pentru Remaining Length

    // Variable Header
    // Topic Name
    int topic_len = strlen(topic);
    packet[index++] = (topic_len >> 8) & 0xFF;
    packet[index++] = topic_len & 0xFF;
    memcpy(&packet[index], topic, topic_len);
    index += topic_len;

    // Packet Identifier
    packet[index++] = (packet_id >> 8) & 0xFF;
    packet[index++] = packet_id & 0xFF;

    // Payload
    int message_len = strlen(message);
    memcpy(&packet[index], message, message_len);
    index += message_len;

    // Calcularea Remaining Length
    remaining_length = index - (remaining_length_index + 1);

    // Codificarea Remaining Length
    unsigned char remaining_length_bytes[4];
    int remaining_length_size = encode_remaining_length(remaining_length_bytes, remaining_length);

    // Mutăm datele pentru a face loc Remaining Length codificat
    memmove(&packet[remaining_length_index + remaining_length_size],
            &packet[remaining_length_index + 1],
            remaining_length);

    // Copiem Remaining Length codificat în pachet
    memcpy(&packet[remaining_length_index], remaining_length_bytes, remaining_length_size);

    // Actualizăm indexul final
    index = remaining_length_index + remaining_length_size + remaining_length;

    // Trimitem pachetul PUBLISH
    if (send(socktfd, packet, index, 0) < 0)
    {
        perror("EROARE: LA TRIMITEREA PACHETULUI PUBLISH!");
        return -1;
    }

    printf("MESAJ PUBLICAT PE TOPICUL '%s'\n", topic);

    packet_id++;

    return 0;
}

int mqtt_subscribe(int socktfd, const char *topic)
{
    unsigned char packet[BUFFER_SIZE];
    int index = 0;
    static int packet_id = 1;

    // fixed header
    packet[index++] = 0x82; // subscrbe packet type
    packet[index++] = 0;

    // variable header
    packet[index++] = (packet_id >> 8) & 0xFF;
    packet[index++] = packet_id & 0xFF;

    // payload
    int topic_len = strlen(topic);
    packet[index++] = (topic_len >> 8) & 0xFF;
    packet[index++] = topic_len & 0xFF;
    memcpy(&packet[index], topic, topic_len);
    index += topic_len;

    packet[index++] = 0x00;

    // remaining length
    int remaining_length = index - 2;
    packet[1] = remaining_length;

    if (send(socktfd, packet, index, 0) < 0)
    {
        perror("EROARE: LA TRIMITEREA PACHETULUI DE SUBSCRIBE!");
        return -1;
    }

    // primim suback
    unsigned char response[5];
    if (recv(socktfd, response, 5, 0) < 0)
    {
        perror("EROARE: LA PRIMIREA SUBACK");
        return -1;
    }

    // verificam SUBACK
    if (response[0] != 0x90 || response[1] != 0x03)
    {
        fprintf(stderr, "PACHER SUBACK INVALID\n");
        return -1;
    }

    printf("ABONARE REUSITA LA TOPICUL '%s'\n", topic);
    packet_id++;
    return 0;
}

int mqtt_ping(int socktfd)
{
    unsigned char packet[2];

    packet[0] = 0xC0;
    packet[1] = 0x00;

    if (send(socktfd, packet, 2, 0) < 0)
    {
        perror("EROARE: LA TRIMITEREA PACHETULUI DE PINGREQ!");
        return -1;
    }

    unsigned char response[2];
    if (recv(socktfd, response, 2, 0) < 0)
    {
        perror("EROARE: LA PRIMIREA PINGRES!");
        return -1;
    }

    // verificare pachet PINGRES
    if (response[0] != 0xD0 || response[1] != 0x00)
    {
        fprintf(stderr, "PACHET PINGRES INVALID\n");
        return -1;
    }

    printf("PINGRES PRIMIT DE LA BROKER\n");
    return 0;
}

void mqtt_disconnect(int socktfd)
{
    unsigned char packet[2];

    packet[0] = 0xE0;
    packet[1] = 0x00;

    if (send(socktfd, packet, 2, 0) < 0)
    {
        perror("EROARE: LA TRIMITEREA PACHETULUI DISCONNECT!");
    }
    else
    {
        printf("DECONECTAT DE LA BROKER-ul MQTT\n");
    }
}
