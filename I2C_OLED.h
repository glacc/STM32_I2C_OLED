// SPDX-License-Identifier: BSD-3-Clause

#ifndef __I2C_OLED_H__
#define __I2C_OLED_H__

#include <stdint.h>
#include <stdbool.h>

#include "stm32f1xx_hal.h"

#define I2C_OLED_ADDR           0x78

// 0-127 columns, 0-7 pages, 1 column = 1 byte
#define I2C_OLED_BUFFER_SIZE    (128 * 8)

#define I2C_OLED_TIMEOUT        100

#define I2C_OLED_COLUMNS        128
#define I2C_OLED_PAGES          8
#define I2C_OLED_ROWS           (I2C_OLED_PAGES * 8)

#ifdef __cplusplus
extern "C" {
#endif
    
    // Note: All direct draw functions will not change the buffer. //
    
    extern I2C_HandleTypeDef *I2C_OLED_i2c_handler;
    
    extern uint8_t I2C_OLED_cursor_column;
    extern uint8_t I2C_OLED_cursor_page;
    
    extern bool I2C_OLED_manual_update;
    
    extern uint8_t I2C_OLED_buffer[];
    
    extern void I2C_OLED_Initialize(I2C_HandleTypeDef *i2c_handler);
    
    extern void I2C_OLED_SetColumnPage(uint8_t column, uint8_t page);
    extern void I2C_OLED_SetCursor(uint8_t column, uint8_t page);
    
    extern void I2C_OLED_WriteToRAM(const uint8_t *buffer, uint16_t count);
    
    extern void I2C_OLED_Update(void);
    extern void I2C_OLED_UpdatePartially
    (
        uint8_t start_column, uint8_t end_column,
        uint8_t start_page,   uint8_t end_page
    );
    
    extern void I2C_OLED_ClearDirect(void);
    extern void I2C_OLED_ClearBuffer(void); 
    extern void I2C_OLED_ClearBufferAndUpdate(void);
    
    extern void I2C_OLED_PutCharDirect(char character, bool inverted);
    extern void I2C_OLED_PutChar(char character, bool inverted);
    
    extern void I2C_OLED_PrintStrDirect(const char *str, bool inverted);
    extern void I2C_OLED_PrintStr(const char *str, bool inverted);
    
    // Note: Functions below can only write to buffer. //
    
    extern void I2C_OLED_DrawRect
    (
        int16_t start_x, int16_t start_y,
        int16_t end_x, int16_t end_y,
        bool inverted
    );
    extern void I2C_OLED_FillRect
    (
        int16_t start_x, int16_t start_y,
        int16_t end_x, int16_t end_y,
        bool inverted
    );
    
    extern void I2C_OLED_GetStrSizeXY(const char *str, int *out_width, int *out_height);
    
    extern void I2C_OLED_PutCharXY
    (
        char character,
        int16_t x, int16_t y,
        bool inverted
    );
    extern void I2C_OLED_PrintStrXY
    (
        const char *str,
        int16_t x, int16_t y,
        bool inverted
    );
    
#ifdef __cplusplus
}
#endif

#endif
