/*-------------------Bibliotecas de projeto----------------------*/

#include <stdio.h>      //printf
#include <driver/adc.h> //biblioteca do ADC
#include <driver/uart.h>//biblioteca do UART
#include "esp_adc_cal.h"



/*-------------------Mapeamento de Hardware----------------------*/
#define pino_de_leitura ADC1_CHANNEL_5 //Pino escolhido para leitura do adc: ADC1-CH5 = GPIO33


/*-------------------Constantes de Projeto ----------------------*/
const uint32_t defaultVref=1114; //Tensão de Referência do meu ESP32 =1.114 V
//Mais informações em: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration



esp_adc_cal_characteristics_t adc_config;//struct para uso da função de calibração: esp_adc_cal_characterize();
//mais informações em: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#_CPPv424esp_adc_cal_characterize10adc_unit_t11adc_atten_t16adc_bits_width_t8uint32_tP29esp_adc_cal_characteristics_t

/*-------------------Declaração das Funções----------------------*/
void setup_ADC(); //Configurações iniciais do ADC

void app_main() {
    setup_ADC();
    while (1)
    {
        //É guardado na variável reading o valor bruto lido pelo adc
        uint32_t reading =  adc1_get_raw(pino_de_leitura); 

        //Usamos essa função para converter o valor bruto do ADC para um valor em Tensão.
        //Se tentarmos converter o valor sem essa função podemos não ler um valor preciso
        //Para chamá-la é necessário primeiro configurar o ADC como foi feito na função
        //esp_adc_cal_characterize()
        uint32_t voltage =  esp_adc_cal_raw_to_voltage(reading,&adc_config);

        float voltage2=voltage;
        voltage2=voltage2/1000;
        printf("Tensao: %f \n",voltage2);
        vTaskDelay(100/portTICK_RATE_MS);
    }
    

}

/*-------------------Implementação das Funções----------------------*/
void setup_ADC(){
    
    adc1_config_width(ADC_WIDTH_BIT_12);//ADC1 com 12 bits

    adc1_config_channel_atten(pino_de_leitura,ADC_ATTEN_DB_0);// Canal 5 do ADC1(GPIO33) e 0dB de atenuação 

    //função para aumentar a precisão na leitura do ADC
    esp_adc_cal_characterize(ADC_UNIT_1,ADC_ATTEN_DB_0,ADC_WIDTH_BIT_12,defaultVref,&adc_config);
}
