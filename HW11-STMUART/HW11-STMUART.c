#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart0
#define BAUD_RATE 115200


// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 0
#define UART_RX_PIN 1



int main()
{
    stdio_init_all();

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, "Hello, UART!\n");
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart
    char pico_msg[] = "Hello from Pico!\r\n";
    while (true) {
        // Send string to STM32
        uart_puts(uart0, pico_msg);
        
        if (uart_is_readable(uart0)) {
            char c = uart_getc(uart0);
            printf("%c", c);
        }

        sleep_ms(1000);
    }
}
