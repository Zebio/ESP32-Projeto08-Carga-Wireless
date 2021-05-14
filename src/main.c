/*--------------------------------------Bibliotecas de projeto----------------------------------------------*/

#include <stdio.h>                  //printf
#include <driver/adc.h>             //biblioteca do ADC
#include <driver/uart.h>            //portTICK_RATE_MS
#include "esp_adc_cal.h"            //struct esp_adc_cal_characteristics_t
#include "nvs_flash.h"              //memória nvs
#include "freertos/event_groups.h"  //grupo de eventos
#include <esp_wifi.h>               //wireless
#include "esp_log.h"                //log de eventos no monitor serial
#include <esp_http_server.h>        //biblioteca para poder usar o server http



/*--------------------------------------------Mapeamento de Hardware----------------------------------------*/

#define pino_de_leitura ADC1_CHANNEL_5 //Pino escolhido para leitura do adc: ADC1-CH5 = GPIO33



/*--------------------------------------------Definições de Projeto-----------------------------------------*/

/* O event Group permite muitos eventos por grupo mas nos só nos importamos com 2 eventos:
 * - Conseguimos conexão com o AP com um IP
 * - Falhamos na conexão apos o número máximo de tentativas*/
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/*Aqui definimos o SSID, a Senha e o número máximo de tentativas que vamos 
tentar ao se conectar à rede Wireless*/
#define ESP_WIFI_SSID      "tira_o_zoio"
#define ESP_WIFI_PASS      "jabuticaba"
#define ESP_MAXIMUM_RETRY  10



/*-----------------------------------------------Constantes de Projeto --------------------------------------*/

static const uint32_t defaultVref=1114;    //Tensão de Referência do meu ESP32 =1.114 V
//Mais informações em: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration

static const char *TAG = "ESP";     //A tag que será impressa no log do sistema  




/*-------------------------------------------------Variáveis Globais-----------------------------------------*/

static uint32_t tensao_da_bateria;                      //valor da tensão da bateria em mV
static int32_t  porcentagem_da_bateria;                 //porcentagem estimada de carga da bateria
static uint32_t numero_tentativa_de_conexao_wifi = 0;   //numero atual da tentativa de se conectar a rede,
                                                        //tentativas máximas= EXAMPLE_ESP_MAXIMUM_RETRY



/*----------------------------------------------------Objetos------------------------------------------------*/

//Handle dos Eventos Wireless
static EventGroupHandle_t s_wifi_event_group;

static esp_adc_cal_characteristics_t adc_config;//struct para uso da função de calibração: esp_adc_cal_characterize();
//mais informações em: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv424esp_adc_cal_characterize10adc_unit_t11adc_atten_t16adc_bits_width_t8uint32_tP29esp_adc_cal_characteristics_t

//Declaração do server http
static httpd_handle_t server =NULL;



/*-----------------------------------------------Declaração das Funções--------------------------------------*/

static void setup_ADC();     //Configurações iniciais do ADC

static void leitura_ADC();   //Realiza a leitura no pino ADC e atualiza as variaveis

static void setup_nvs();     //Inicia a memória nvs. Ela é necessária para se conectar à rede Wireless

void wifi_init_sta(); //Configura a rede wireless

/* Lida com os Eventos da rede Wireless(reconexão, IPs, etc), essa função é ativada durante a chamada de 
 * void wifi_init_sta() e permanece monitorando os eventos de rede e imprimindo no terminal
 * as tentativas de conexão e o endereço IP recebido que o ESP ganhou*/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);


//Cria o Server, Faz as configurações Padrão e Inicia os URI Handlers para os GETs
static httpd_handle_t start_webserver(void);

//Imprimimos a Webpage
static void print_webpage(httpd_req_t *req);

//handler do Get da Página Principal
static esp_err_t main_page_get_handler(httpd_req_t *req);



/*--------------------------------------Declaração dos GETs do http------------------------------------------*/
//a declaração do GET da página Principal
static const httpd_uri_t main_page = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = main_page_get_handler,
    .user_ctx  = NULL
};



/*--------------------------------------------- Função Principal --------------------------------------------*/

void app_main() {
    setup_ADC();                  //configura ADC
    setup_nvs();                  //configura a memoria nvs  
    wifi_init_sta();              //configura e inicia a conexão wireless
    server = start_webserver();   //configura e inicia o server
   /*while (true){
        leitura_ADC();            //realiza a leitura do ADC a cada 100ms
        vTaskDelay(100/portTICK_RATE_MS);
    }*/
}



/*-------------------------------------Implementação das Funções----------------------------------------------*/

static void setup_ADC(){

    adc1_config_width(ADC_WIDTH_BIT_12);//ADC1 com 12 bits

    adc1_config_channel_atten(pino_de_leitura,ADC_ATTEN_DB_0);// Canal 5 do ADC1(GPIO33) e 0dB de atenuação 

    //função para aumentar a precisão na leitura do ADC
    //ela diminui o erro do ADC caracterizando a curva de tensão
    //recebe a tensão de referencia e a configuração do ADC
    esp_adc_cal_characterize(ADC_UNIT_1,ADC_ATTEN_DB_0,ADC_WIDTH_BIT_12,defaultVref,&adc_config);
}

static void leitura_ADC(){

        //É guardado na variável reading o valor bruto lido pelo adc
        uint32_t reading =  adc1_get_raw(pino_de_leitura); 

        //Usamos essa função para converter o valor bruto do ADC para um valor em Tensão em mV.
        //Se não usarmos essa função podemos não ler um valor preciso
        //Para chamá-la é necessário primeiro configurar o ADC com a função
        //esp_adc_cal_characterize()
        uint32_t voltage =  esp_adc_cal_raw_to_voltage(reading,&adc_config);

        tensao_da_bateria = voltage*4;
        porcentagem_da_bateria=((tensao_da_bateria-3600)/7);
        //printf("Tensao: %d ; Porcentagem: %d \n",tensao_da_bateria,porcentagem_da_bateria);
}

//Inicializa a memória nvs pois é necessária para o funcionamento do Wireless
static void setup_nvs(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

//Lida com os Eventos da rede Wireless, conexão à rede e endereço IP
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); //se o Wifi ja foi iniciado tenta se conectar
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (numero_tentativa_de_conexao_wifi < ESP_MAXIMUM_RETRY) { //se o numero atual de tentativas
            esp_wifi_connect();                                             //de conexão não atingiu o máximo
            numero_tentativa_de_conexao_wifi++;                             //tenta denovo            
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);          //se o numero atingiu o máximo ativa
        }                                                                   //o envento de falha no wifi            
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { //se estamos conectados a rede vamos
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;         //imprimir o IP no terminal via ESP_LOGI()
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        numero_tentativa_de_conexao_wifi = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(){
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

//Cria o Server, Faz as configurações Padrão e Inicia os URI Handlers
//para os GETs
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Inicia o server http
    printf("Iniciando o Server na Porta: '%d'\n", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        printf("Registrando URI handlers\n");
        httpd_register_uri_handler(server, &main_page);
        return server;
    }

    printf("Erro ao iniciar Server\n");
    return NULL;
}

//Imprime a Webpage
static void print_webpage(httpd_req_t *req)
{
    //Constantes Cstring para Armazenar os pedaços do código HTML 
    const char *index_html_part1= "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"icon\" href=\"data:,\"><title>Projeto07 - Nivel de Carga de bateria</title><script type = \"text/JavaScript\">       function AutoRefresh( t ) {   setTimeout(\"location.reload(true);\", t);}</script></head><style>html{color: #ffffff;font-family: Verdana;text-align: center;background-color: #0c0c27fd}h2{font-size: 30px;}p{font-size: 22px;}</style><body onload = \"JavaScript:AutoRefresh(1000);\"><h2>Monitoramento de Bateria IOT</h2><p> Tensao lida na bateria: ";
    const char *index_html_part2= "</p><p> Porcentagem estimada: ";
    const char *index_html_part3= "</p></body></html>";

    //converte a tensão da bateria de int para char* para enviar no html
    char char_tensao_da_bateria[4];
    sprintf(char_tensao_da_bateria, "%d", tensao_da_bateria);

    //converte a porcentagem da bateria de int para char* para enviar no html
    char char_porcentagem_da_bateria[3];
    sprintf(char_porcentagem_da_bateria, "%d",porcentagem_da_bateria); 

    //vamos concatenando os "pedaços" do html no buffer de acordo com os valores das variáveis
    char buffer[617] =""; 
    strcat(buffer, index_html_part1);
    strcat(buffer, char_tensao_da_bateria);
    strcat(buffer, index_html_part2);
    strcat(buffer, char_porcentagem_da_bateria);
    strcat(buffer, index_html_part3);

    httpd_resp_send(req, buffer, strlen(buffer)); //envia via http o buffer que contém a página html completa
}


//handler do Get da Página Principal
static esp_err_t main_page_get_handler(httpd_req_t *req)
{
    //imprime a página
    print_webpage(req);
    //retorna OK
    return ESP_OK;
}