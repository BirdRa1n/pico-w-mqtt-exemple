#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/apps/mqtt.h"

// Configurações de Wi-Fi
#define WIFI_SSID "NOME DA REDE"
#define WIFI_PASS "SENHA DA REDE"

// Configurações do Broker MQTT
#define MQTT_BROKER_IP "192.168.1.xx"
#define MQTT_BROKER_PORT 1883

// Pino do LED para indicar conexão
#define LED_PIN 12

mqtt_client_t *mqtt_client;

// Função de callback para receber mensagens
void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    printf("Mensagem recebida no tópico: %s\n", topic);
}

void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    printf("Dados recebidos: %.*s\n", len, data);
}

// Função de callback para confirmação de inscrição (opcional)
void mqtt_subscribe_cb(void *arg, err_t err)
{
    if (err == ERR_OK)
    {
        printf("Inscrição confirmada!\n");
    }
    else
    {
        printf("Erro na confirmação da inscrição: %d\n", err);
    }
}

// Função para se inscrever em um tópico
void subscribe_to_topic(mqtt_client_t *client, const char *topic, uint8_t qos)
{
    err_t err = mqtt_subscribe(client, topic, qos, mqtt_subscribe_cb, NULL);
    if (err != ERR_OK)
    {
        printf("Erro ao se inscrever no tópico %s: %d\n", topic, err);
    }
    else
    {
        printf("Inscrito no tópico %s com QoS %d\n", topic, qos);
    }
}

// Função para publicar uma mensagem
void publish_message(mqtt_client_t *client, const char *topic, const char *message, uint8_t qos, uint8_t retain)
{
    err_t err = mqtt_publish(client, topic, message, strlen(message), qos, retain, NULL, NULL);
    if (err != ERR_OK)
    {
        printf("Erro ao publicar mensagem no tópico %s: %d\n", topic, err);
    }
    else
    {
        printf("Mensagem publicada no tópico %s: %s\n", topic, message);
    }
}

// Função de callback para conexão MQTT
void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("Conectado ao broker MQTT!\n");
        gpio_put(LED_PIN, 1); // Acende o LED ao conectar

        // Inscreve-se em um tópico após a conexão
        subscribe_to_topic(client, "pico/topic", 0);
    }
    else
    {
        printf("Erro na conexao MQTT: %d\n", status);
        gpio_put(LED_PIN, 0); // Apaga o LED em caso de erro
    }
}

// Inicialização do MQTT
void mqtt_init()
{
    ip_addr_t broker_ip;
    if (!ipaddr_aton(MQTT_BROKER_IP, &broker_ip)) // Converte o endereço IP para o formato correto
    {
        printf("Erro ao converter o endereço IP do broker MQTT\n");
        return;
    }

    mqtt_client = mqtt_client_new();
    if (mqtt_client == NULL)
    {
        printf("Erro ao criar cliente MQTT\n");
        return;
    }

    // Configura os callbacks para receber mensagens
    mqtt_set_inpub_callback(mqtt_client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);

    // Estrutura de informações do cliente MQTT
    struct mqtt_connect_client_info_t client_info = {
        .client_id = "pico_client",
        .client_user = NULL, // Se precisar de autenticação, adicione usuário e senha
        .client_pass = NULL,
        .keep_alive = 60,
        .will_topic = NULL,
        .will_msg = NULL,
        .will_qos = 0,
        .will_retain = 0};

    // Tenta conectar ao broker MQTT
    err_t err = mqtt_client_connect(mqtt_client, &broker_ip, MQTT_BROKER_PORT, mqtt_connection_cb, NULL, &client_info);
    if (err != ERR_OK)
    {
        printf("Erro ao tentar conectar ao broker MQTT: %d\n", err);
    }
}

// Função para piscar o LED
void piscar_led()
{
    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);
}

int main()
{
    stdio_init_all();

    sleep_ms(2000);
    printf("Iniciando programa\n");

    // Configura o LED como saída
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Inicializa o Wi-Fi
    if (cyw43_arch_init())
    {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");
    piscar_led();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    }
    else
    {
        printf("Connected.\n");
        // Lê o endereço IP de forma legível
        uint8_t *ip_address = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    printf("Wi-Fi conectado!\n");

    mqtt_init();

    while (true)
    {
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo

        // Publica uma mensagem a cada 5 segundos
        static absolute_time_t last_publish_time;
        if (absolute_time_diff_us(last_publish_time, get_absolute_time()) > 5000000)
        {
            publish_message(mqtt_client, "pico/topic", "Hello from Pico!", 0, 0);
            last_publish_time = get_absolute_time();
        }

        sleep_ms(100);
    }

    return 0;
}