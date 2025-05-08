// SPDX-License-Identifier: BSD-3-Clause

#include "I2C_OLED.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "stm32f1xx_hal.h"

#include "i2c.h"

#include "Font_VertHorz.h"

I2C_HandleTypeDef *I2C_OLED_i2c_handler = NULL;

uint8_t I2C_OLED_cursor_column = 0;
uint8_t I2C_OLED_cursor_page = 0;

bool I2C_OLED_manual_update = true;
bool I2C_OLED_non_overwrite_direct = false;

uint8_t I2C_OLED_buffer[I2C_OLED_BUFFER_SIZE];

/*
    Reference:
        https://cdn-shop.adafruit.com/datasheets/UG-2864HSWEG01.pdf
        https://github.com/pkourany/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp
*/
static const uint8_t ssd1306_initialization_sequence[] =
{
    0xAE,       // Set display off
    
    0xD5, 0x80, // Set display clock divide ration oscillator frequency
    0xA8, 0x3F, // Set multiplex ratio
    0xD3, 0x00, // Set display offset
    0x40,       // Set display start line
    0x8D, 0x14, // Set charge pump          (2nd byte: 0x10 external VCC, 0x14 internal DC/DC)
    0xA1,       // Set segment re-map
    0xC8,       // Set COM output scan direction
    0xDA, 0x12, // Set COM pins hardware configuration
    0x81, 0xCF, // Set contrast control     (2nd byte: 0x9F external VCC, 0xCF internal DC/DC)
    0xD9, 0xF1, // Set pre-charge period    (2nd byte: 0x22 external VCC, 0xF1 internal DC/DC)
    0xDB, 0x40, // Set VCOMH deselect level
    0xA4,       // Set entire display on
    0xA6,       // Set normal display
    
    0xAF,       // Set display on
};

static const uint8_t ssd1306_addressing_mode_configuration_sequence[] =
{
    0x20, 0x10, // Set memory addressing mode (to horizontal mode)
};

void I2C_OLED_Initialize(I2C_HandleTypeDef *i2c_handler)
{
    I2C_OLED_i2c_handler = i2c_handler;
    
    // Initialization sequence //
    
    HAL_I2C_Mem_Write
    (
        I2C_OLED_i2c_handler,
        I2C_OLED_ADDR,
        0x00,
        1,
        (uint8_t *)ssd1306_initialization_sequence,
        sizeof(ssd1306_initialization_sequence),
        100
    );
        
    // Addressing mode //
        
    HAL_I2C_Mem_Write
    (
        I2C_OLED_i2c_handler,
        I2C_OLED_ADDR,
        0x00,
        1,
        (uint8_t *)ssd1306_addressing_mode_configuration_sequence,
        sizeof(ssd1306_addressing_mode_configuration_sequence),
        100
    );
}

void I2C_OLED_SetColumnPage(uint8_t column, uint8_t page)
{
    if (column >= I2C_OLED_COLUMNS || page >= I2C_OLED_PAGES)
        return;
    
    uint8_t column_nibble_h = (column >> 4) & 0x0F;
    uint8_t column_nibble_l = column & 0x0F;
    
    uint8_t addressing_setting_sequence[] =
    {
        0xB0 + page,            // Page start address
        0x00 + column_nibble_l, // Column start address L nibble
        0x10 + column_nibble_h, // Column start address H nibble
    };
    
    HAL_I2C_Mem_Write
    (
        I2C_OLED_i2c_handler,
        I2C_OLED_ADDR,
        0x00,
        1,
        addressing_setting_sequence,
        sizeof(addressing_setting_sequence),
        I2C_OLED_TIMEOUT
    );
}

void I2C_OLED_WriteToRAM(const uint8_t *buffer, uint16_t count)
{
    if (buffer == NULL)
        return;
    
    HAL_I2C_Mem_Write
    (
        I2C_OLED_i2c_handler,
        I2C_OLED_ADDR,
        0x40,
        1,
        (uint8_t *)buffer,
        count,
        I2C_OLED_TIMEOUT
    );
}    
void I2C_OLED_Update(void)
{
    for (int i = 0; i < I2C_OLED_PAGES; i++)
    {
        I2C_OLED_SetColumnPage(0, i);
            
        int offset = i * I2C_OLED_COLUMNS;
            
        I2C_OLED_WriteToRAM(I2C_OLED_buffer + offset, 128);
    }
}

void I2C_OLED_UpdatePartially
(
    uint8_t start_column, uint8_t end_column,
    uint8_t start_page,   uint8_t end_page
)
{
    if (start_column > end_column)
    {
        int16_t temp = start_column;
        start_column = end_column;
        end_column = temp;
    }
    if (start_page > end_page)
    {
        int16_t temp = start_page;
        start_page = end_page;
        end_page = temp;
    }
    
    if (start_column >= I2C_OLED_COLUMNS || start_page >= I2C_OLED_PAGES)
        return;
    
    if (end_column >= I2C_OLED_COLUMNS)
        end_column = I2C_OLED_COLUMNS - 1;
    
    if (end_page >= I2C_OLED_PAGES)
        end_page = I2C_OLED_PAGES - 1;
    
    uint8_t columns_to_update_per_page = end_column - start_column + 1;
    
    for (int page = start_page; page <= end_page; page++)
    {
        I2C_OLED_SetColumnPage(start_column, page);
        
        int offset = (page * I2C_OLED_COLUMNS) + start_column;
        
        I2C_OLED_WriteToRAM(I2C_OLED_buffer + offset, columns_to_update_per_page);
    }
}

void I2C_OLED_ClearDirect(void)
{
    static const uint8_t buffer_clear[8] = { 0x00 };
    
    for (int i = 0; i < I2C_OLED_PAGES; i++)
    {
        I2C_OLED_SetColumnPage(0, i);
        
        for (int j = 0; j < I2C_OLED_COLUMNS / sizeof(buffer_clear); j++)
            I2C_OLED_WriteToRAM(buffer_clear, sizeof(buffer_clear));
    }
}

void I2C_OLED_ClearBuffer(void)
{
    uint8_t *ptr_buffer = I2C_OLED_buffer;
    
    for (int i = 0; i < sizeof(I2C_OLED_buffer); i++)
    {
        *ptr_buffer = 0x00;
        ptr_buffer++;
    }
    
    if (!I2C_OLED_manual_update)
        I2C_OLED_Update();
}

void I2C_OLED_ClearBufferAndUpdate(void)
{
    I2C_OLED_ClearBuffer();
    
    if (I2C_OLED_manual_update)
        I2C_OLED_Update();
}

void I2C_OLED_SetCursor(uint8_t column, uint8_t page)
{
    if (column >= I2C_OLED_COLUMNS)
        column = I2C_OLED_COLUMNS - 1;
    
    if (page >= I2C_OLED_PAGES)
        page = I2C_OLED_PAGES - 1;
    
    I2C_OLED_cursor_column = column;
    I2C_OLED_cursor_page = page;
}

void I2C_OLED_PutCharDirect(char character, bool inverted)
{
    if (character < 0x20 || character > 0x7F)
        character = 0x20;
    
    int index_of_glyph = character - 0x20;
    
    if (!inverted)
    {
        uint8_t empty_byte = 0x00;
        
        I2C_OLED_WriteToRAM(Font_VertHorz_ascii[index_of_glyph], 5);
        I2C_OLED_WriteToRAM(&empty_byte, 1);
    }
    else
    {
        uint8_t glyph_buffer[6];
        
        uint8_t *ptr_glyph_source = (uint8_t *)Font_VertHorz_ascii[index_of_glyph];
        uint8_t *ptr_glyph_buffer = glyph_buffer;
        
        for (int i = 0; i < 5; i++)
        {
            *ptr_glyph_buffer = ~(*ptr_glyph_source);
            
            ptr_glyph_buffer++;
            ptr_glyph_source++;
        }
        
        glyph_buffer[5] = 0x00;
        
        I2C_OLED_WriteToRAM(glyph_buffer, 6);
    }
}

void I2C_OLED_PrintStrDirect(const char *str, bool inverted)
{
    if (str == NULL)
        return;
    
    while (*str)
    {
        I2C_OLED_PutCharDirect(*str, inverted);
        str++;
    }
}

void I2C_OLED_PutChar(char character, bool inverted)
{
    if (character < 0x20 || character > 0x7F)
        character = 0x20;
    
    uint8_t start_column = I2C_OLED_cursor_column;
    
    int index_of_glyph = character - 0x20;
    
    uint8_t *ptr_buffer = &I2C_OLED_buffer[(I2C_OLED_cursor_page * I2C_OLED_COLUMNS) + I2C_OLED_cursor_column];
    uint8_t *ptr_font = (uint8_t *)Font_VertHorz_ascii[index_of_glyph];
    
    for (int i = 0; i < 5; i++)
    {
        if (!inverted)
            *ptr_buffer |= *ptr_font;
        else
            *ptr_buffer &= ~(*ptr_font);
        
        if (I2C_OLED_cursor_column >= I2C_OLED_COLUMNS - 1)
            break;
        
        I2C_OLED_cursor_column++;
        
        ptr_buffer++;
        ptr_font++;
    }
    
    I2C_OLED_cursor_column++;
    
    if (!I2C_OLED_manual_update)
        I2C_OLED_UpdatePartially(start_column, I2C_OLED_cursor_column, I2C_OLED_cursor_page, I2C_OLED_cursor_page);
}

void I2C_OLED_PrintStr(const char *str, bool inverted)
{
    if (str == NULL)
        return;
    
    while (*str)
    {
        I2C_OLED_PutChar(*str, inverted);
        str++;
    }
}

// Out of bounds shapes are allowed. //

void I2C_OLED_DrawRect
(
    int16_t start_x, int16_t start_y,
    int16_t end_x, int16_t end_y,
    bool inverted
)
{
    // start_x/y can be smaller than end_x/y //
    
    if (start_x > end_x)
    {
        int16_t temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    if (start_y > end_y)
    {
        int16_t temp = start_y;
        start_y = end_y;
        end_y = temp;
    }
    
    if (start_x < 0 && end_x >= I2C_OLED_COLUMNS && start_y < 0 && end_y >= I2C_OLED_ROWS)
        return;
    
    if ((start_x >= I2C_OLED_COLUMNS || end_x < 0) && (start_y >= I2C_OLED_ROWS || end_y < 0))
        return;
    
    bool start_x_in_bound = start_x >= 0;
    bool end_x_in_bound = end_x < I2C_OLED_COLUMNS;
    bool start_y_in_bound = start_y >= 0;
    bool end_y_in_bound = end_y < I2C_OLED_ROWS;
    
    uint8_t page_start_y = 0;
    uint8_t page_end_y = I2C_OLED_PAGES - 1;
    if (start_y_in_bound)
        page_start_y = start_y / 8;
    if (end_y_in_bound)
        page_end_y = end_y / 8;
    
    uint8_t start_y_mod_8 = start_y & 0x07;
    uint8_t end_y_mod_8 = end_y & 0x07;
    uint8_t end_y_shift = 7 - end_y_mod_8;
    
    uint8_t mask_edge_vert_upper = 0xFF << start_y_mod_8;
    uint8_t mask_edge_vert_lower = 0xFF >> end_y_shift;
    
    uint8_t edge_horz_upper = 0x01 << start_y_mod_8;
    uint8_t edge_horz_lower = 0x01 << end_y_mod_8;
    
    uint16_t buffer_offset;
    
    uint8_t horz_start_x = 0;
    uint8_t horz_end_x = I2C_OLED_COLUMNS - 1;
    if (start_x_in_bound)
        horz_start_x = start_x;
    if (end_x_in_bound)
        horz_end_x = end_x;
    
    if (page_start_y == page_end_y)
    {
        buffer_offset = I2C_OLED_COLUMNS * page_start_y;
        
        uint8_t edge_vert = mask_edge_vert_upper & mask_edge_vert_lower;
        uint8_t edges_horz = edge_horz_upper | edge_horz_lower;
        
        if (start_x_in_bound)
        {
            if (!inverted)
                I2C_OLED_buffer[buffer_offset + horz_start_x] |= edge_vert;
            else
                I2C_OLED_buffer[buffer_offset + horz_start_x] &= ~edge_vert;
            
            horz_start_x += 1;
        }
        
        if (end_x_in_bound)
        {
            if (!inverted)
                I2C_OLED_buffer[buffer_offset + horz_end_x] |= edge_vert;
            else
                I2C_OLED_buffer[buffer_offset + horz_end_x] &= ~edge_vert;
            horz_end_x -= 1;
        }
        
        uint8_t *ptr_buffer = I2C_OLED_buffer + buffer_offset + horz_start_x;
        
        if (!inverted)
        {
            for (int i = horz_start_x; i <= horz_end_x; i++)
            {
                *ptr_buffer |= edges_horz;
                ptr_buffer++;
            }
        }
        else
        {
            uint8_t edges_horz_inverted = ~edges_horz;
            
            for (int i = horz_start_x; i <= horz_end_x; i++)
            {
                *ptr_buffer &= edges_horz_inverted;
                ptr_buffer++;
            }
        }
    }
    else
    {
        buffer_offset = I2C_OLED_COLUMNS * page_start_y;
        
        uint16_t buffer_offset_to_lower = I2C_OLED_COLUMNS * (page_end_y - page_start_y);
        
        // Corners //
        
        if (start_x_in_bound)
        {
            if (start_y_in_bound)
            {
                if (!inverted)
                    I2C_OLED_buffer[buffer_offset + horz_start_x] |= mask_edge_vert_upper;
                else
                    I2C_OLED_buffer[buffer_offset + horz_start_x] &= ~mask_edge_vert_upper;
            }
            
            if (end_y_in_bound)
            {
                if (!inverted)
                    I2C_OLED_buffer[buffer_offset + horz_start_x + buffer_offset_to_lower] |= mask_edge_vert_lower;
                else
                    I2C_OLED_buffer[buffer_offset + horz_start_x + buffer_offset_to_lower] &= ~mask_edge_vert_lower;
            }
            
            horz_start_x += 1;
        }
        
        if (end_x_in_bound)
        {
            if (start_y_in_bound)
            {
                if (!inverted)
                    I2C_OLED_buffer[buffer_offset + horz_end_x] |= mask_edge_vert_upper;
                else
                    I2C_OLED_buffer[buffer_offset + horz_end_x] &= ~mask_edge_vert_upper;
            }
            
            if (end_y_in_bound)
            {
                if (!inverted)
                    I2C_OLED_buffer[buffer_offset + horz_end_x + buffer_offset_to_lower] |= mask_edge_vert_lower;
                else
                    I2C_OLED_buffer[buffer_offset + horz_end_x + buffer_offset_to_lower] &= ~mask_edge_vert_lower;
            }
            
            horz_end_x -= 1;
        }
        
        // Upper & Lower edge //
        
        uint8_t *ptr_buffer_upper = &I2C_OLED_buffer[buffer_offset + horz_start_x];
        uint8_t *ptr_buffer_lower = &I2C_OLED_buffer[buffer_offset + buffer_offset_to_lower + horz_start_x];
        
        if (!inverted)
        {
            for (int i = horz_start_x; i <= horz_end_x; i++)
            {
                if (start_y_in_bound)
                    *ptr_buffer_upper |= edge_horz_upper;
                
                if (end_y_in_bound)
                    *ptr_buffer_lower |= edge_horz_lower;
                
                ptr_buffer_upper++;
                ptr_buffer_lower++;
            }
        }
        else
        {
            uint8_t edge_horz_upper_inverted = ~edge_horz_upper;
            uint8_t edge_horz_lower_inverted = ~edge_horz_lower;
            
            for (int i = horz_start_x; i <= horz_end_x; i++)
            {
                if (start_y_in_bound)
                    *ptr_buffer_upper &= edge_horz_upper_inverted;
                
                if (end_y_in_bound)
                    *ptr_buffer_lower &= edge_horz_lower_inverted;
                
                ptr_buffer_upper++;
                ptr_buffer_lower++;
            }
        }
        
        // Left & right edge //
        
        uint16_t page_left_right_edge = page_start_y + 1;
        
        if (page_start_y + 1 < page_end_y)
        {        
            uint16_t offset_left_right_edge_start = I2C_OLED_COLUMNS * page_left_right_edge;
            
            uint8_t *ptr_buffer_left_edge = &I2C_OLED_buffer[offset_left_right_edge_start + start_x];
            uint8_t *ptr_buffer_right_edge = &I2C_OLED_buffer[offset_left_right_edge_start + end_x];
            
            uint8_t edge_pixels;
            if (!inverted)
                edge_pixels = 0xFF;
            else
                edge_pixels = 0x00;
            
            for (int i = page_left_right_edge; i < page_end_y; i++)
            {
                *ptr_buffer_left_edge = edge_pixels;
                *ptr_buffer_right_edge = edge_pixels;
                
                ptr_buffer_left_edge += I2C_OLED_COLUMNS;
                ptr_buffer_right_edge += I2C_OLED_COLUMNS;
            }
        }
    }
    
    if (!I2C_OLED_manual_update)
        I2C_OLED_UpdatePartially(start_x, end_x, page_start_y, page_end_y);
}

void I2C_OLED_FillRect
(
    int16_t start_x, int16_t start_y,
    int16_t end_x, int16_t end_y,
    bool inverted
)
{
    // start_x/y can be smaller than end_x/y //
    
    if (start_x > end_x)
    {
        int16_t temp = start_x;
        start_x = end_x;
        end_x = temp;
    }
    if (start_y > end_y)
    {
        int16_t temp = start_y;
        start_y = end_y;
        end_y = temp;
    }
    
    if ((start_x >= I2C_OLED_COLUMNS || end_x < 0) && (start_y >= I2C_OLED_ROWS || end_y < 0))
        return;
    
    if (start_x < 0)
        start_x = 0;
    if (end_x >= I2C_OLED_COLUMNS)
        end_x = I2C_OLED_COLUMNS - 1;
    
    if (start_y < 0)
        start_y = 0;
    if (end_y >= I2C_OLED_ROWS)
        end_y = I2C_OLED_ROWS - 1;
    
    bool start_y_in_bound = start_y >= 0;
    bool end_y_in_bound = end_y < I2C_OLED_ROWS;
    
    uint8_t page_start_y = 0;
    uint8_t page_end_y = I2C_OLED_PAGES - 1;
    if (start_y_in_bound)
        page_start_y = start_y / 8;
    if (end_y_in_bound)
        page_end_y = end_y / 8;
    
    uint8_t start_y_mod_8 = start_y & 0x07;
    uint8_t end_y_mod_8 = end_y & 0x07;
    uint8_t end_y_shift = 7 - end_y_mod_8;
    
    uint8_t mask_edge_vert_upper = 0xFF << start_y_mod_8;
    uint8_t mask_edge_vert_lower = 0xFF >> end_y_shift;
    
    uint8_t *ptr_buffer = &I2C_OLED_buffer[(I2C_OLED_COLUMNS * page_start_y) + start_x];
    
    if (page_start_y == page_end_y)
    {
        // When upper edge and lower edge is in the same page //
        
        uint8_t pixels_vert = mask_edge_vert_upper & mask_edge_vert_lower;
        
        if (!inverted)
        {
            for (int i = start_x; i <= end_x; i++)
            {
                *ptr_buffer |= pixels_vert;
                ptr_buffer++;
            }
        }
        else
        {
            uint8_t pixels_vert_inverted = ~pixels_vert;
            
            for (int i = start_x; i <= end_x; i++)
            {
                *ptr_buffer &= pixels_vert_inverted;
                ptr_buffer++;
            }
        }
    }
    else
    {
        if (!inverted)
        {
            // Upper edge //
            
            for (int i = start_x; i <= end_x; i++)
            {
                *ptr_buffer |= mask_edge_vert_upper;
                ptr_buffer++;
            }
            
            // Lower edge //
            
            ptr_buffer = &I2C_OLED_buffer[(I2C_OLED_COLUMNS * page_end_y) + start_x];
            
            for (int i = start_x; i <= end_x; i++)
            {
                *ptr_buffer |= mask_edge_vert_lower;
                ptr_buffer++;
            }
        }
        else
        {
            uint8_t mask_edge_vert_upper_inverted = ~mask_edge_vert_upper;
            uint8_t mask_edge_vert_lower_inverted = ~mask_edge_vert_lower;
            
            // Upper edge (inverted) //
            
            for (int i = start_x; i <= end_x; i++)
            {
                *ptr_buffer &= mask_edge_vert_upper_inverted;
                ptr_buffer++;
            }
            
            // Lower edge (inverted) //
            
            ptr_buffer = &I2C_OLED_buffer[(I2C_OLED_COLUMNS * page_end_y) + start_x];
            
            for (int i = start_x; i <= end_x; i++)
            {
                *ptr_buffer &= mask_edge_vert_lower_inverted;
                ptr_buffer++;
            }
        }
        
        // Middle //
        
        uint8_t page_middle = page_start_y + 1;
        
        if (page_middle < page_end_y)
        {
            uint8_t pixels;
            if (!inverted)
                pixels = 0xFF;
            else
                pixels = 0x00;
            
            for (int i = page_middle; i < page_end_y; i++)
            {
                ptr_buffer = &I2C_OLED_buffer[(I2C_OLED_COLUMNS * i) + start_x];
                
                for (int j = start_x; j <= end_x; j++)
                {
                    *ptr_buffer = pixels;
                    ptr_buffer++;
                }
            }
        }
    }
    
    if (!I2C_OLED_manual_update)
        I2C_OLED_UpdatePartially(start_x, end_x, page_start_y, page_end_y);
}

void I2C_OLED_GetStrSizeXY(const char *str, int *out_width, int *out_height)
{
    char *ptr_str = (char *)str;
    
    int text_width = 0;
    int width_current_line = 0;
    
    int text_height = 8;
    
    while (*ptr_str)
    {
        char current_char = *ptr_str;
        
        ptr_str++;
        
        if (current_char == '\n')
        {
            text_height += 8;
            
            width_current_line = 0;
            continue;
        }
        
        if (current_char == '\r')
            continue;
        
        width_current_line += 6;
        
        if (width_current_line > text_width)
            text_width = width_current_line;
    }
    
    if (out_width != NULL)
        *out_width = text_width;
    
    if (out_height != NULL)
        *out_height = text_height;
}

void I2C_OLED_PutCharXY
(
    char character,
    int16_t x, int16_t y,
    bool inverted
)
{
    if (character < 0x20 || character > 0x7F)
        return;
    
    if ((x <= -5 || x >= I2C_OLED_COLUMNS + 5) || (y <= -8 || y >= I2C_OLED_ROWS + 8))
        return;
    
    int16_t page_y = y / 8;
    
    uint8_t *ptr_glyph = (uint8_t *)Font_VertHorz_ascii[character - 0x20];
    
    int16_t current_x = x;
    
    if ((y & 0x07) == 0)
    {
        // If y is aligned to page //
        
        if (page_y >= I2C_OLED_PAGES)
            return;
        
        uint8_t *ptr_buffer = &I2C_OLED_buffer[(I2C_OLED_COLUMNS * page_y) + x];
        
        for (int i = 0; i < 5; i++)
        {
            if (current_x >= I2C_OLED_COLUMNS)
                break;
            
            if (current_x >= 0)
            {
                if (!inverted)
                    *ptr_buffer |= *ptr_glyph;
                else
                    *ptr_buffer &= ~(*ptr_glyph);
            }
            
            ptr_buffer++;
            ptr_glyph++;
            
            current_x++;
        }
        
        if (!I2C_OLED_manual_update)
            I2C_OLED_UpdatePartially(x, x + 4, page_y, page_y);
    }
    else
    {
        // If y is not aligned to page //
        
        uint8_t *ptr_buffer_upper = &I2C_OLED_buffer[(I2C_OLED_COLUMNS * page_y) + x];
        uint8_t *ptr_buffer_lower = ptr_buffer_upper + I2C_OLED_COLUMNS;
        
        int16_t y_mod_8 = y & 0x07;
        int16_t y_shift_lower = 8 - y_mod_8;
        
        bool no_upper = y < 0;
        bool no_lower = page_y >= I2C_OLED_PAGES - 1;
        
        for (int i = 0; i < 5; i++)
        {
            if (current_x >= I2C_OLED_COLUMNS)
                break;
            
            if (current_x >= 0)
            {
                uint8_t glyph_pixels = *ptr_glyph;
                
                if (!no_upper)
                {
                    uint8_t glyph_upper = glyph_pixels << y_mod_8;
                    
                    if (!inverted)
                        *ptr_buffer_upper |= glyph_upper;
                    else
                        *ptr_buffer_upper &= ~glyph_upper;
                }
                
                if (!no_lower)
                {
                    uint8_t glyph_lower = glyph_pixels >> y_shift_lower;
                    
                    if (!inverted)
                        *ptr_buffer_lower |= glyph_lower;
                    else
                        *ptr_buffer_lower &= ~glyph_lower;
                }
            }
            
            ptr_buffer_upper++;
            ptr_buffer_lower++;
                
            ptr_glyph++;
            
            current_x++;
        }
        
        if (!I2C_OLED_manual_update)
            I2C_OLED_UpdatePartially(x, x + 4, page_y, page_y + 1);
    }
}

void I2C_OLED_PrintStrXY
(
    const char *str,
    int16_t x, int16_t y,
    bool inverted
)
{
    if (str == NULL)
        return;
    
    int text_width;
    
    I2C_OLED_GetStrSizeXY(str, &text_width, NULL);
    
    if ((x <= -text_width || x >= I2C_OLED_COLUMNS) || (y <= -8 || y >= I2C_OLED_ROWS + 8))
        return;
    
    int16_t current_x = x;
    int16_t current_y = y;
    
    char *ptr_str = (char *)str;
    
    while (*ptr_str)
    {
        char current_char = *ptr_str;
        
        ptr_str++;
        
        if (current_char == '\n')
        {
            current_y += 8;
            
            if (current_y >= I2C_OLED_ROWS)
                break;
            
            current_x = x;
            
            continue;
        }
        
        I2C_OLED_PutCharXY(current_char, current_x, current_y, inverted);
        
        current_x += 6;
    }
}
