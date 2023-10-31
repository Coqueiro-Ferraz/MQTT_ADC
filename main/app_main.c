//leia com atenção!!!!!

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                _                    //
//                   _  .   _         _   _               _   ___   _         _   _    _               //
//              |   |_  |  |_|       |   | |  |\/|       |_|   |   |_  |\ |  |   |_|  | |              //    
//              |_  |_  |  | |       |_  |_|  |  |       | |   |   |_  | \|  |_  | |  |_|              //
//                                                                            /                        //
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/*  MQTT_ADC (em TCP) por HagaCeeF. 
    
    Não me responsabilizo se não funcionar, não está completo,
    é apenas um código para fins didáticos, podem haver problemas sobressalentes.
    
    Usado para efetuar testes na nossa placa de desenvolvimento:

    Explicação sobre ADC

        A função   hcf_adc_ler(&valor) retorna para a variável valor o valor lido no ADC0 da placa (método ONESHOT)
        
        A função   hcf_adc_ler3(&valor) retorna para a variável valor o valor lido no ADC3 da placa (método ONESHOT)

        O sinal lido é uma média de 256 amostras para minimizar o ruído do ADC e a atenuação é de 11dB, portanto a entrada pode variar de 0 a 3.3V (0 a 4095 bits)

        A função   Porcentagem_1 = vAdcPorcento(valor); transforma o valor que foi lido no ADC (que é de 0 a 4095) em uma porcentagem (0 a 100 %)

        Porcentagem_1 se refere ao ADC0 e Porcentagem_2 ao ADC3

    Explicação sobre controle de entradas e saídas (IOs)

        Foi estabelecido um ramo que executa uma rotina em paralelo com a principal chamada  rotina_IOs, nela as entradas são lidas e as saídas escritas a cada 100ms

        Se você precisar do valor de todos os 8 bits da entrada, eles ficam gravados na variável entradas

        Se você precisar escrever algo na saída, utilize a variável saída = os oito bits que você quer escrever, por exemplo 0b00100110
            Se você quiser ligar apenas um relé, use a macro LIGAR_RELE_1 ou 2 (#define LIGAR_RELE_1 saidas|=0b00000001)

    Explicação sobre o display e console

        A função    ESP_LOGI("MAIN", "Valor da entrada analógica: %"PRIu32" ADC0", valor); mostra apenas no console

        A função    sprintf(mostrador,"Sensor 1: %d %%      ", Porcentagem_2); organiza o texto na variavel mostrador, ela escreve "Sensor 1: 35 %" se a variável porcentagem_2 estiver em 35

        A função    lcd595_write(2, 0, mostrador); escreve no display, o primeiro valor é a linha e o segundo, a coluna, o terceiro é o texto para escrever


*/

#include <inttypes.h> //<- não tem problema se ficar sublinhado de vermelho ou azul, se quiser resolver basta 
#include <stdio.h> //incluir o arquivo cpp_properties.json na pasta .vscode
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
//#include "connect.h"
#include "ioplaca.h"
#include "lcdvia595.h"
#include "hcf_adc.h"

#define DESLIGAR_TUDO       saidas&=0b00000000
#define LIGAR_RELE_1        saidas|=0b00000001
#define DESLIGAR_RELE_1     saidas&=0b11111110
#define LIGAR_RELE_2        saidas|=0b00000010
#define DESLIGAR_RELE_2     saidas&=0b11111101
#define LIGAR_TRIAC_1       saidas|=0b00000100
#define DESLIGAR_TRIAC_1    saidas&=0b11111011
#define LIGAR_TRIAC_2       saidas|=0b00001000
#define DESLIGAR_TRIAC_2    saidas&=0b11110111
#define LIGAR_TBJ_1         saidas|=0b00010000
#define DESLIGAR_TBJ_1      saidas&=0b11101111
#define LIGAR_TBJ_2         saidas|=0b00100000
#define DESLIGAR_TBJ_2      saidas&=0b11011111
#define LIGAR_TBJ_3         saidas|=0b01000000
#define DESLIGAR_TBJ_3      saidas&=0b10111111
#define LIGAR_TBJ_4         saidas|=0b10000000
#define DESLIGAR_TBJ_4      saidas&=0b01111111

static const char *TAG = "MQTT_ADC";

//Área de declaração de variáveis
//-----------------------------------------------------------------------------------------------------------------------

int entradas = 0; //entradas da placa
int saidas = 0;   //saídas da placa

//Variáveis e Textos para display

char *Texto_1 = "S1: "; 
int Porcentagem_1 = 0;
char *Estado_1 = "---";

char *Texto_2 = "S2: ";
int Porcentagem_2 = 0;
char *Estado_2 = "---";

char mostrador[40];

//Rotina de aplicação do ADC: transforma 0 - 3.3V (0 - 4095 no ADC) em 0 a 100%
//-----------------------------------------------------------------------------------------------------------------------

int vAdcPorcento(uint32_t valor_adc)
{
    int percentual = 0;

    percentual = valor_adc * 100 / 4095;

    return percentual;
}


// Rotina de leitura das entradas e saídas da placa
//-----------------------------------------------------------------------------------------------------------------------

void rotina_IOs(void* pvParam)
{
    while(1)
    {
        entradas = io_le_escreve(saidas);
       // ESP_LOGI("MAIN", "INs: %d OUTs: %d", entradas, saidas);
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
}

//-----------------------------------------------------------------------------------------------------------------------

// Rotina para mostrar valores no display
//-----------------------------------------------------------------------------------------------------------------------

void exibe_lcd(void)
{

    // Organiza texto na string mostrador e imprime no LCD
    // Para a primeira entrada
    sprintf(mostrador,"%s %d%% %s      ", Texto_1, Porcentagem_1, Estado_1);  //imprime o text seguido do valor do sensor e o estado
    lcd595_write( 1, 0, mostrador); 

    // Para a segunda entrada
    sprintf(mostrador,"%s %d%% %s      ", Texto_2, Porcentagem_2, Estado_2);   
    lcd595_write( 2, 0, mostrador); 

}

//-----------------------------------------------------------------------------------------------------------------------

// Rotina de Tratamento da leitura anaógica
//-----------------------------------------------------------------------------------------------------------------------

void rotina_adc(void)
{
    uint32_t valor;

    // Lê o valor da entrada analógica
    esp_err_t read_result = hcf_adc_ler(&valor); 
    if (read_result == ESP_OK) {
        ESP_LOGI("MAIN", "Valor da entrada analógica: %"PRIu32" ADC0", valor); // mostra valor lido no console  
        Porcentagem_1 = vAdcPorcento(valor);

    } else {
        ESP_LOGE("MAIN", "Erro na leitura da entrada analógica 1"); 
        Estado_1 = "Erro 1";
    }

    vTaskDelay(10 / portTICK_PERIOD_MS); 

    // faz o mesmo, só que para a outra entrada analógica
    esp_err_t read_result2 = hcf_adc_ler_3(&valor);
    if (read_result2 == ESP_OK) {

        Porcentagem_2 = vAdcPorcento(valor);

    } else {
        ESP_LOGE("MAIN", "Erro na leitura da entrada analógica 3");
        Estado_2 = "Erro 2";
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); 
}
//-----------------------------------------------------------------------------------------------------------------------


void app_main(void)
{
    /////////////////////////////////////////////////////////////////////////////////////   Programa principal


    // a seguir, apenas informações de console, aquelas notas verdes no início da execução
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    /////////////////////////////////////////////////////////////////////////////////////   Inicializações de periféricos (manter assim)
    
    // inicializar os IOs da placa
    ioinit();      

    // inicializar o display LCD 
    lcd595_init();
    
    // Inicializar o componente de leitura de entrada analógica
    esp_err_t init_result = hcf_adc_iniciar();
    if (init_result != ESP_OK) {
        ESP_LOGE("MAIN", "Erro ao inicializar o componente ADC personalizado");
    }

    //delay
    vTaskDelay(1000 / portTICK_PERIOD_MS); 

    /////////////////////////////////////////////////////////////////////////////////////   Periféricos inicializados

    DESLIGAR_TUDO;

    /////////////////////////////////////////////////////////////////////////////////////   Ramos de execução

    xTaskCreate(rotina_IOs, "tarefa de leitura de IOs", 1024, NULL, 3, NULL);

    /////////////////////////////////////////////////////////////////////////////////////   Início do ramo principal                    
    while (1)                                                                                                                           //  | | | | | | | | | | |
    {                                                                                                                                   //  v v v v v v v v v v v
        rotina_adc();
//__________________________________________________________________________________________________________________________________________________________________
//-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  Escreve seu código aqui!!! //
                                                                                                                                                                    //

        if (Porcentagem_1 <= 50){                                                                                                                                   

            LIGAR_TRIAC_1;    
            Estado_1 = "ON ";

        } else if (Porcentagem_1 >= 90) {                                                                                                                           

            DESLIGAR_TRIAC_1;
            Estado_1 = "OFF";   

        }       
       
        vTaskDelay(10 / portTICK_PERIOD_MS); 

        if (Porcentagem_2 <= 5){                                                                                                                                   
  
            LIGAR_TBJ_4;   
            Estado_2 = "ON ";      

        } else if (Porcentagem_2 >= 35) {                                                                                                                           

            DESLIGAR_TBJ_4;   
            Estado_2 = "OFF";                                                                                                                                  

        }      
        
        exibe_lcd();     

        vTaskDelay(10 / portTICK_PERIOD_MS);     

//-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  Escreve seu até aqui!!!    //
//__________________________________________________________________________________________________________________________________________________________________//
        vTaskDelay(10 / portTICK_PERIOD_MS);    
    }
    
    // caso erro no programa desliga o módulo ADC
    hcf_adc_limpar();

    /////////////////////////////////////////////////////////////////////////////////////   Fim do ramo principal
    
}
