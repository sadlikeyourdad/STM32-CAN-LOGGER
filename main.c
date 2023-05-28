#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "spi.h"
#include "usart.h"

// Define MCP2515 registers
#define MCP2515_RXB0SIDH     0x61
#define MCP2515_RXB0SIDL     0x62
#define MCP2515_RXB0D0       0x66

// Define CAN message structure
typedef struct {
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
} CanMessage;

SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;

void MCP2515_Write(uint8_t address, uint8_t data) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // Select MCP2515
    HAL_SPI_Transmit(&hspi1, &address, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  // Deselect MCP2515
}

uint8_t MCP2515_Read(uint8_t address) {
    uint8_t data;
    address |= 0x03;  // Set READ command
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // Select MCP2515
    HAL_SPI_Transmit(&hspi1, &address, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  // Deselect MCP2515
    return data;
}

void MCP2515_Reset() {
    MCP2515_Write(0xC0, 0x03);  // Reset instruction
    HAL_Delay(10);
}

void CAN_Init() {
    MCP2515_Reset();
    MCP2515_Write(0x2A, 0x0F);  // Configure CAN timing, baudrate, etc.
    // Additional configuration settings as needed
}

void CAN_Receive(CanMessage* msg) {
    uint8_t buffer[13];
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // Select MCP2515
    HAL_SPI_Transmit(&hspi1, (uint8_t[]){MCP2515_RXB0SIDH}, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, buffer, 13, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  // Deselect MCP2515

    msg->id = (buffer[0] << 3) | (buffer[1] >> 5);
    msg->length = buffer[4] & 0x0F;
    for (int i = 0; i < msg->length; i++) {
        msg->data[i] = buffer[5 + i];
    }
}

void CAN_Transmit(CanMessage* msg) {
    uint8_t buffer[13];
    buffer[0] = MCP2515_RXB0SIDH;
    buffer[1] = (msg->id >> 3) & 0xFF;
    buffer[2] = (msg->id << 5) & 0xE0;
    buffer[3] = 0;
    buffer[4] = msg->length & 0x0F;
    for (int i = 0; i < msg->length; i++) {
        buffer[5 + i] = msg->data[i];
    }
    
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // Select MCP2515
    HAL_SPI_Transmit(&hspi1, buffer, 5 + msg->length, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  // Deselect MCP2515
}

void USART_Print(const char* message) {
    HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

void USART_PrintCANMessage(CanMessage* msg) {
    char buffer[100];
    sprintf(buffer, "ID: %lu, Length: %u, Data: ", msg->id, msg->length);
    USART_Print(buffer);
    
    for (int i = 0; i < msg->length; i++) {
        sprintf(buffer, "%02X ", msg->data[i]);
        USART_Print(buffer);
    }
    
    USART_Print("\r\n");
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();
    
    CAN_Init();
    
    while (1) {
        CanMessage rxMsg;
        CAN_Receive(&rxMsg);
        USART_PrintCANMessage(&rxMsg);
    }
}
