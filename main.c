#include "mqtt_client.h"

int main(int argc, char *argv[])
{
    const char *broker_ip = "127.0.0.1";
    int broker_port = MQTT_PORT;

    // cream soket-ul
    int socktfd = socket(AF_INET, SOCK_STREAM, 0);

    // verificam daca s-a creat sokectul
    if (socktfd < 0)
    {
        perror("Eroare: LA CREAREA SOCKET-ului!");
        exit(EXIT_FAILURE);
    }

    // configurez sockaddr_in
    struct sockaddr_in broker_addr;

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port);

    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) < 0)
    {
        perror("EROARE: ADRESA BROKER-ului este invalida!");
        close(socktfd);
        exit(EXIT_FAILURE);
    }
    // conectarea la broker
    if (connect(socktfd, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0)
    {
        perror("EROARE: LA CONEXIUNEA BROKER-ului! ");
    }
    // trimit pachet ul ce conectare
    if (mqtt_connect(socktfd, "client_c") < 0)
    {
        fprintf(stderr, "EROARE: LA TRIMITEREA PACHETULUI DE CONNECT\n");
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    // ne abonam la un topic
    const char *topic_sub = "test/topic";
    if (mqtt_subscribe(socktfd, topic_sub) < 0)
    {
        fprintf(stderr, "EROARE: LA OPERATIA DE SUBSCRIBE!\n");
        mqtt_disconnect(socktfd);
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    // publicarea unui mesaj pe topic
    const char *message = "Salut de la client!";
    if (mqtt_publish(socktfd, topic_sub, message) < 0)
    {
        fprintf(stderr, "EROARE: LA OPERATIA DE PUBLISH\n");
        mqtt_disconnect(socktfd);
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    // setez socket-ul non-blocat
    int flags = fcntl(socktfd, F_GETFD, 0);
    if (flags == -1)
    {
        perror("EROARE: LA OBTINEREA FLAG-urilor SOCKET-ului");
        mqtt_disconnect(socktfd);
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    if (fcntl(socktfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("EROARE: LA SETAREA SOCKET-ului IN MODUL NON-BLOCAT!");
        mqtt_disconnect(socktfd);
        close(socktfd);
        exit(EXIT_FAILURE);
    }

    // ascultarea pentru mesaje
    int i = 0;
    while (i < 5)
    {
        unsigned char buffer[BUFFER_SIZE];
        int bytes_received = recv(socktfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0)
        {
            int index = 0;
            unsigned char packet_type = buffer[0] & 0xF0;

            // remaining
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

            switch (packet_type)
            {
            case 0x30: // publish
            {
                // index start variable header
                int start_index = index;
                // Topic name
                int topic_len = (buffer[index++] << 8);
                topic_len |= buffer[index++];
                char topic_received[topic_len + 1];
                memcpy(topic_received, &buffer[index], topic_len);
                topic_received[topic_len] = '\0';
                index += topic_len;

                // daca Qos > 0, citim packet id
                unsigned short msg_packet_id = 0;
                if (((buffer[0] >> 1) & 0x30) > 0)
                {
                    msg_packet_id = (buffer[index++] << 8);
                    msg_packet_id |= buffer[index++];
                }

                // calculam payload_len
                int payload_len = remaining_length - (index - start_index);

                if (payload_len < 0 || (index + payload_len) > bytes_received)
                {
                    fprintf(stderr, "EROARE: LUNGIME PAYLOAD INVALIDA!");
                    break;
                }

                // pqyload
                char message_recevied[payload_len + 1];
                memcpy(message_recevied, &buffer[index], payload_len);
                message_recevied[payload_len] = '\0';

                printf("MESAJ PRIMIT PE TOPICUL '%s': %s\n", topic_received, message_recevied);

                // daca Qos este 1 trimitem PUBACK
                if (((buffer[0] >> 1) & 0x03) == 1)
                {
                    unsigned char puback_packet[4];
                    puback_packet[0] = 0x40; // puback
                    puback_packet[1] = 0x02; // remaining length
                    puback_packet[2] = (msg_packet_id >> 8) & 0xFF;
                    puback_packet[3] = msg_packet_id & 0xFF;
                    send(socktfd, puback_packet, 4, 0);
                }
                break;
            }
            case 0x40: // puback
            {
                if (remaining_length != 2)
                {
                    fprintf(stderr, "EROARE: PACHER PUBACK CU LENGTH INVALID!\n");
                    break;
                }
                // citim packet identifier
                unsigned short recv_packet_id = (buffer[index++] << 8);
                recv_packet_id |= buffer[index++];

                printf("PUBACK PRIMIT PENTRU PACKET ID: %d\n", recv_packet_id);
                break;
            }
            default:
                printf("PACHETUL NECUNOSCUT PRIMIT , tip: 0x%X\n", packet_type);
                break;
            }
        }
        else if (bytes_received == 0)
        {
            // s-a inchis conexiunea de catre broker
            printf("CONEXIUNEA CU BROKER-ul A FOST INCHISA\n");
            break;
        }
        else
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                // nu sunt date disponibile momentan
                printf("NU SUNT DATE DISPONIBILE MOMENTAN\n");
            }
            else
            {
                perror("EROARE LA PRIMIREA DATELOR");
                break;
            }
        }
        i++;
        sleep(2);
    }

    mqtt_disconnect(socktfd);
    close(socktfd);

    return 0;
}