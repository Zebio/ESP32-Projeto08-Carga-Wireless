
# Nível de Carga de Bateria Wireless com ESP32

### Resumo

Este projeto é muito parecido com o projeto 07. Neste projeto vamos usar o ESP32 para realizar a medição da tensão de uma bateria de lítio-ion, e com base na tensão estimar sua carga. Contudo há um problema em estimar a carga de baterias de lítio baseado somente na tensão, como podemos ver na imagem abaixo, a curva tensão X carga não é linear.


![](imagens/voltXcarga.jpg)

podemos ver que de 15% a 90% a alteração da tensão é mínima. No caso do nosso projeto vamos realizar uma aproximação para tentarmos estimar a carga da bateria. Vamos supor que 3.6 V equivale a 0% e 4.0 V equivale a 100%  da carga.

Adicionalmente, estamos mostrando o valor lido da bateria e a porcentagem de carga estimada via servidor web, para oferecer um monitoramento da carga remoto.

### Como Usar

Para clonar projetos do PlatformIO como esse, siga o guia passo-a-passo disponível no repositório: [Instrucoes-PlatformIO](https://github.com/Zebio/Instrucoes-PlatformIO)

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration

ADC VRef calibration: 1114mV
