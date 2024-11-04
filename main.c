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

    mqtt_disconnect(socktfd);
    close(socktfd);

    return 0;
}