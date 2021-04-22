#include <driver/adc.h> //biblioteca do ADC
#include <driver/uart.h>//biblioteca do UART
#include <string.h>     //strlen



void setup_ADC();
void setup_UART();

void app_main() {
    setup_ADC();
    setup_UART();
    while (1)
    {
        int val = adc1_get_raw(ADC1_CHANNEL_0); //captura o valor de tensão do GPIO36.

        char* test_str = "This is a test string.\n";
        uart_write_bytes(UART_NUM_0, (const char*)test_str, strlen(test_str));
    }
    

}


void setup_ADC(){
    //ADC1 com 12 bits, canal 0(GPIO36) e 0dB de atenuação 
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);
}

void setup_UART(){
    //feita a configuração UART baseada na documentação da Espressiv: 
    //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html#uart-api-setting-communication-parameters
   
    //1)Setting Communication Parameters - Setting baud rate, data bits, stop bits, etc.
    const int uart_num = UART_NUM_2;
    uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
    .rx_flow_ctrl_thresh = 122,
};
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    //2)Setting Communication Pins - Assigning pins for connection to a device.

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, 18, 19));


    //3)Driver Installation - Allocating ESP32’s resources for the UART driver.

    // Setup UART buffered IO with event queue
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, uart_buffer_size, \
                                        uart_buffer_size, 10, &uart_queue, 0));


}