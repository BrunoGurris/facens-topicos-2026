/*
 * FreeRTOSConfig.h dos testes no PC: e' a MESMA configuracao da placa
 * (Core/Inc/FreeRTOSConfig.h), so' com a pilha minima maior porque no port
 * POSIX cada tarefa vira uma thread do Linux.
 */
#ifndef FREERTOS_CONFIG_HOST_H
#define FREERTOS_CONFIG_HOST_H

#include "../../Core/Inc/FreeRTOSConfig.h"

#undef  configMINIMAL_STACK_SIZE
#define configMINIMAL_STACK_SIZE 4096

#endif
