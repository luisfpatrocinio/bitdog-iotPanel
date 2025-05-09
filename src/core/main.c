#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// Patro Libs
#include "display.h"
#include "text.h"
#include "analog.h"
#include "draw.h"
#include "text.h"
#include "buttons.h"
#include "pico/unique_id.h"

#define WIFI_SSID "patro"
#define WIFI_PASSWORD "cafecombiscoito"

#define LED_PIN CYW43_WL_GPIO_LED_PIN

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Copia a requisição HTTP para uma string alocada dinamicamente
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0'; // Adiciona o terminador nulo
    printf("Request: %s\n", request);

    // Criação da resposta HTML:

    char id_string[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(id_string, sizeof(id_string));

    // Body
    char body[1024];
    snprintf(body, sizeof(body),
             "<!DOCTYPE html>"
             "<html><head><meta charset='utf-8'><meta http-equiv='refresh' content='1'>"
             "<style>"
             "body { background-color: #0f0f0f; color: #00ffc3; font-family: monospace; text-align: center; padding: 40px; }"
             ".panel { background: #1a1a1a; border: 1px solid #00ffc3; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px #00ffc3; display: inline-block; }"
             "h1 { color: #00ffd5; text-shadow: 0 0 5px #00ffd5; }"
             "p { font-size: 1.2em; margin: 10px 0; }"
             "</style></head>"
             "<body>"
             "<div class='panel'>"
             "<h1>Leituras da BitDogLab</h1>"
             "<p><strong>Botão 1:</strong> %s</p>"
             "<p><strong>Botão 2:</strong> %s</p>"
             "<p><strong>Analógico X:</strong> %d</p>"
             "<p><strong>Analógico Y:</strong> %d</p>"
             "<p><strong>ID da Placa:</strong> %s</p>"
             "<p style='margin-top:20px; font-size: 0.9em;'>"
             "<a href='https://github.com/luisfpatrocinio' style='color:#00ffc3; text-decoration:none;'>Desenvolvido por Luis F. Patrocinio</a>"
             "</p>"
             "</div>"
             "</body></html>",
             gpio_get(BTA) ? "Solto" : "Pressionado",
             gpio_get(BTB) ? "Solto" : "Pressionado",
             readAnalogX(),
             readAnalogY(),
             id_string);

    // Tamanho do corpo
    int body_length = strlen(body);

    // Cabeçalho + Corpo
    // Cabeçalho + corpo
    char html[1200];
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             body_length,
             body);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    pbuf_free(p);

    return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

void displayMessage(const char *message)
{
    clearDisplay();
    drawText(0, 0, message);
    printf("%s\n", message);
    showDisplay();
}

void setup()
{
    stdio_init_all();
    initDisplay();
    initAnalog();
    initButtons();
}

int main()
{
    setup();

    // Initialize Wifi
    displayMessage("Connecting to WiFi...");
    if (cyw43_arch_init())
    {
        displayMessage("Failed to initialize CYW43");
        return -1;
    }
    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    // Connecting to WiFi
    char _connectingMessage[50];
    snprintf(_connectingMessage, sizeof(_connectingMessage), "Connecting to %s...", WIFI_SSID);
    displayMessage(_connectingMessage);
    cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
    while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_JOIN)
    {
        // Aguarda a conexão WiFi
        static int led_state = 0;
        static int led_timer = 0;
        led_timer++;
        if (led_timer > 30)
        {
            led_timer = 0;
            led_state = !led_state;
        }
        cyw43_arch_gpio_put(LED_PIN, led_state);

        // Draw Text and Wave
        clearDisplay();
        drawTextCentered("Connecting...", 0);
        drawWave(SCREEN_HEIGHT / 2, 6, 12);
        showDisplay();
    }

    displayMessage("Connected to WiFi!");
    if (netif_default)
    {
        printf("IP Address: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }
    else
    {
        printf("Failed to get IP address\n");
    }

    // Configura o servidor TCP na porta 80
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Failed to create TCP Server\n");
        return -1;
    }
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Failed to bind TCP Server to port 80\n");
        tcp_close(server);
        return -1;
    }

    server = tcp_listen(server);

    tcp_accept(server, tcp_server_accept);

    printf("TCP Server listening on port 80\n");

    while (1)
    {
        cyw43_arch_poll();
        clearDisplay();
        drawText(0, 0, "Hello from Patrocinio!");
        drawText(0, 10, "IP Address: ");
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "%s", ipaddr_ntoa(&netif_default->ip_addr));
        drawText(0, 20, ip_str);

        showDisplay();
    }
}
