/* main.h falso para os testes no PC: so' o que as tarefas do AMV usam do HAL. */
#ifndef MAIN_H_HOST
#define MAIN_H_HOST

#include <stdint.h>
#include <stdlib.h>

typedef struct { int dummy; } UART_HandleTypeDef;
typedef enum { HAL_OK = 0, HAL_BUSY } HAL_StatusTypeDef;

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, const uint8_t *d, uint16_t n);
void Error_Handler(void);

#define __disable_irq()     ((void)0)
#define NVIC_SystemReset()  exit(3)

#endif
