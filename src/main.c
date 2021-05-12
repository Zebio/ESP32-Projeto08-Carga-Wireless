/*--------------------------------------Bibliotecas de projeto------------------------------------------*/

#include <stdio.h>                  //printf
#include <driver/adc.h>             //biblioteca do ADC
#include <driver/uart.h>            //portTICK_RATE_MS
#include "esp_adc_cal.h"            //struct esp_adc_cal_characteristics_t
#include "nvs_flash.h"              //memória nvs
#include "freertos/event_groups.h"  //grupo de eventos
#include <esp_wifi.h>               //wireless
#include "esp_log.h"                //log de eventos no monitor serial


/*
#include "esp_event.h"              //eventos
*/

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
#define EXAMPLE_ESP_WIFI_SSID      "tira_o_zoio"
#define EXAMPLE_ESP_WIFI_PASS      "jabuticaba"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10



/*-----------------------------------------------Constantes de Projeto --------------------------------------*/

const uint32_t defaultVref=1114;    //Tensão de Referência do meu ESP32 =1.114 V
//Mais informações em: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration

static const char *TAG = "ESP";     //A tag que será impressa no log do sistema  



/*-------------------------------------------------Variáveis Globais-----------------------------------------*/

uint32_t tensao_da_bateria;                      //valor da tensão da bateria em mV
uint32_t porcentagem_da_bateria;                 //porcentagem estimada de carga da bateria
static int numero_tentativa_de_conexao_wifi = 0; //numero atual da tentativa de se conectar a rede,
                                                 //tentativas máximas= EXAMPLE_ESP_MAXIMUM_RETRY



/*----------------------------------------------------Objetos------------------------------------------------*/

//Handle dos Eventos Wireless
static EventGroupHandle_t s_wifi_event_group;

esp_adc_cal_characteristics_t adc_config;//struct para uso da função de calibração: esp_adc_cal_characterize();
//mais informações em: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv424esp_adc_cal_characterize10adc_unit_t11adc_atten_t16adc_bits_width_t8uint32_tP29esp_adc_cal_characteristics_t



/*-----------------------------------------------Declaração das Funções--------------------------------------*/

void setup_ADC();     //Configurações iniciais do ADC

void leitura_ADC();   //Realiza a leitura no pino ADC e atualiza as variaveis

void setup_nvs();     //Inicia a memória nvs. Ela é necessária para se conectar à rede Wireless

void wifi_init_sta(); //Configura a rede wireless

/* Lida com os Eventos da rede Wireless(reconexão, IPs, etc), essa função é ativada durante a chamada de 
 * void wifi_init_sta() e permanece monitorando os eventos de rede e imprimindo no terminal
 * as tentativas de conexão e o endereço IP recebido que o ESP ganhou*/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);




/*--------------------------------------------- Função Principal --------------------------------------------*/

void app_main() {
    setup_ADC();    //configura ADC
    setup_nvs();    //configura a memoria nvs
    while (1)
    {
        leitura_ADC();
        vTaskDelay(100/portTICK_RATE_MS);
    }
}



/*-------------------------------------Implementação das Funções----------------------------------------------*/

void setup_ADC(){

    adc1_config_width(ADC_WIDTH_BIT_12);//ADC1 com 12 bits

    adc1_config_channel_atten(pino_de_leitura,ADC_ATTEN_DB_0);// Canal 5 do ADC1(GPIO33) e 0dB de atenuação 

    //função para aumentar a precisão na leitura do ADC
    //ela diminui o erro do ADC caracterizando a curva de tensão
    //recebe a tensão de referencia e a configuração do ADC
    esp_adc_cal_characterize(ADC_UNIT_1,ADC_ATTEN_DB_0,ADC_WIDTH_BIT_12,defaultVref,&adc_config);
}

void leitura_ADC(){

        //É guardado na variável reading o valor bruto lido pelo adc
        uint32_t reading =  adc1_get_raw(pino_de_leitura); 

        //Usamos essa função para converter o valor bruto do ADC para um valor em Tensão em mV.
        //Se não usarmos essa função podemos não ler um valor preciso
        //Para chamá-la é necessário primeiro configurar o ADC com a função
        //esp_adc_cal_characterize()
        uint32_t voltage =  esp_adc_cal_raw_to_voltage(reading,&adc_config);

        tensao_da_bateria = voltage*4;
        porcentagem_da_bateria=((tensao_da_bateria-3600)/7);
        printf("Tensao: %d ; Porcentagem: %d \n",tensao_da_bateria,porcentagem_da_bateria);
}


void setup_nvs(){
    //Inicializa a memória nvs pois é necessária para o funcionamento do Wireless
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

//Lida com os Eventos da rede Wireless
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect(); //se o Wifi ja foi iniciado tenta se conectar
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (numero_tentativa_de_conexao_wifi < EXAMPLE_ESP_MAXIMUM_RETRY) { //se o numero atual de tentativas
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
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
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
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

