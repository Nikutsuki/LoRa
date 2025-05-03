#include <string.h>
#include <stdio.h>
#include "board.h"
#include "gpio.h"
#include "delay.h"
#include "radio.h"
#include "pico/stdlib.h"
#include "sx126x.h"
#include "sx126x-board.h"
#include "pico/time.h"

const uint LED_PIN = PICO_DEFAULT_LED_PIN;

#define RF_FREQUENCY                433000000 // Hz
#define TX_OUTPUT_POWER             22        // dBm
#define LORA_BANDWIDTH              2         // 125 kHz
#define LORA_SPREADING_FACTOR       12
#define LORA_CODINGRATE             1
#define LORA_PREAMBLE_LENGTH        8
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false
#define TX_TIMEOUT_VALUE            3000      // ms

#define BUFFER_SIZE                 256
uint8_t Buffer[BUFFER_SIZE];
char input_buffer[BUFFER_SIZE];

static RadioEvents_t RadioEvents;

void OnTxDone(void)
{
    printf("Transmission complete\r\n");
}

// Character-by-character input function
void read_line(char *buffer, int max_len) {
    int i = 0;
    int c;
    
    printf("> ");
    
    while (i < max_len - 1) {
        c = getchar();
        
        // Check if character is available
        if (c == PICO_ERROR_TIMEOUT) {
            sleep_ms(10);  // Small delay to prevent tight loop
            continue;
        }
        
        // Process Enter key
        if (c == '\r' || c == '\n') {
            printf("\r\n");  // Echo newline
            break;
        }
        
        // Process backspace
        if (c == 8 || c == 127) {
            if (i > 0) {
                i--;
                printf("\b \b");  // Erase character on terminal
            }
            continue;
        }
        
        // Regular character
        if (c >= 32 && c <= 126) {  // Printable ASCII
            buffer[i++] = c;
            putchar(c);  // Echo character
        }
    }
    
    buffer[i] = '\0';  // Null-terminate the string
}

int main(void)
{
    // Initialize serial with non-blocking input
    stdio_init_all();
    stdio_set_translate_crlf(&stdio_usb, false);
    
    // Wait for USB serial connection
    sleep_ms(2000);
    
    // Initialize board
    BoardInitMcu();
    BoardInitPeriph();
    
    // Setup LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Initialize radio with minimal callbacks
    RadioEvents.TxDone = OnTxDone;
    Radio.Init(&RadioEvents);
    
    // Set frequency
    Radio.SetChannel(RF_FREQUENCY);
    
    // Configure radio for LoRa transmission
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);
    
    printf("LoRa Transmitter Ready - Type a message and press Enter to send\r\n");
    
    while(1)
    {
        // Clear input buffer
        memset(input_buffer, 0, BUFFER_SIZE);
        
        // Get input using our custom function
        read_line(input_buffer, BUFFER_SIZE);
        
        size_t len = strlen(input_buffer);
        
        // Skip empty messages
        if (len == 0) {
            continue;
        }
        
        // Copy message to transmit buffer
        strncpy((char*)Buffer, input_buffer, len);
        
        printf("Sending: %s\r\n", input_buffer);
        
        // Blink LED to indicate transmission
        gpio_put(LED_PIN, 1);
        
        // Enable antenna switch for transmission
        SX126xAntSwOff();
        DelayMs(10);
        
        // Send the message
        Radio.Send(Buffer, len);
        
        // Wait for transmission to complete
        DelayMs(100);
        gpio_put(LED_PIN, 0);
        
        // Process radio IRQs
        if(Radio.IrqProcess != NULL) {
            Radio.IrqProcess();
        }
        
        // Return antenna to receive mode
        SX126xAntSwOn();
        DelayMs(500);
    }
}