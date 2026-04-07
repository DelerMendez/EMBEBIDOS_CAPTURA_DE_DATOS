/**
 * @file ui.c
 * @brief Implementación de la interfaz gráfica (UI) para visualización de datos del sensor en pantalla GC9A01.
 *
 * Este archivo implementa una interfaz gráfica simple para mostrar datos de un sensor IMU
 * (como aceleración, giroscopio y temperatura) utilizando un display GC9A01.
 *
 * Incluye una fuente personalizada 5x7, funciones para renderizar caracteres y texto,
 * y utilidades para actualizar dinámicamente la información en pantalla.
 *
 * Las funciones que dibujan en pantalla utilizan internamente el driver GC9A01,
 * por lo que son bloqueantes debido al uso de SPI y no son thread-safe si se accede
 * desde múltiples tareas simultáneamente.
 *
 * @author Deler Mendez
 * @author Nathalia Piamba
 * @author Danny Ramirez
 * @version 1.0
 */

#include "ui.h"
#include "gc9a01.h"
#include <stdio.h>
#include <stdbool.h>

/* Colores */
/** @brief Color negro (RGB565) */
#define COLOR_BLACK   0x0000
/** @brief Color blanco (RGB565) */
#define COLOR_WHITE   0xFFFF
/** @brief Color rojo (RGB565) */
#define COLOR_RED     0xF800
/** @brief Color verde (RGB565) */
#define COLOR_GREEN   0x07E0
/** @brief Color cian (RGB565) */
#define COLOR_CYAN    0x07FF
/** @brief Color amarillo (RGB565) */
#define COLOR_YELLOW  0xFFE0
/** @brief Color gris (RGB565) */
#define COLOR_GRAY    0x8410

/* ========================= */
/* ======= FUENTE 5x7 ====== */
/* ========================= */

/** @brief Representación del carácter espacio */
static const uint8_t ch_space[7] = {0,0,0,0,0,0,0};
/** @brief Representación del carácter '.' */
static const uint8_t ch_dot[7]   = {0,0,0,0,0,0x0C,0x0C};
/** @brief Representación del carácter '-' */
static const uint8_t ch_dash[7]  = {0,0,0,0x1E,0,0,0};
/** @brief Representación del carácter ':' */
static const uint8_t ch_colon[7] = {0,0x0C,0x0C,0,0x0C,0x0C,0};

/* números */
/** @brief Representación del carácter '0' */
static const uint8_t ch_0[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
/** @brief Representación del carácter '1' */
static const uint8_t ch_1[7] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
/** @brief Representación del carácter '2' */
static const uint8_t ch_2[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
/** @brief Representación del carácter '3' */
static const uint8_t ch_3[7] = {0x1E,0x01,0x01,0x06,0x01,0x01,0x1E};
/** @brief Representación del carácter '4' */
static const uint8_t ch_4[7] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
/** @brief Representación del carácter '5' */
static const uint8_t ch_5[7] = {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E};
/** @brief Representación del carácter '6' */
static const uint8_t ch_6[7] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
/** @brief Representación del carácter '7' */
static const uint8_t ch_7[7] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
/** @brief Representación del carácter '8' */
static const uint8_t ch_8[7] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
/** @brief Representación del carácter '9' */
static const uint8_t ch_9[7] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};

/* letras */
/** @brief Representación del carácter 'A' */
static const uint8_t ch_A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
/** @brief Representación del carácter 'C' */
static const uint8_t ch_C[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
/** @brief Representación del carácter 'E' */
static const uint8_t ch_E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
/** @brief Representación del carácter 'G' */
static const uint8_t ch_G[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F};
/** @brief Representación del carácter 'I' */
static const uint8_t ch_I[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
/** @brief Representación del carácter 'L' */
static const uint8_t ch_L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
/** @brief Representación del carácter 'M' */
static const uint8_t ch_M[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
/** @brief Representación del carácter 'N' */
static const uint8_t ch_N[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
/** @brief Representación del carácter 'O' */
static const uint8_t ch_O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
/** @brief Representación del carácter 'P' */
static const uint8_t ch_P[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
/** @brief Representación del carácter 'Q' */
static const uint8_t ch_Q[7] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
/** @brief Representación del carácter 'R' */
static const uint8_t ch_R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
/** @brief Representación del carácter 'T' */
static const uint8_t ch_T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
/** @brief Representación del carácter 'U' */
static const uint8_t ch_U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
/** @brief Representación del carácter 'X' */
static const uint8_t ch_X[7] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
/** @brief Representación del carácter 'Y' */
static const uint8_t ch_Y[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
/** @brief Representación del carácter 'Z' */
static const uint8_t ch_Z[7] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};

/**
 * @brief Obtiene el bitmap de un carácter
 * @param c Carácter ASCII
 * @return Puntero al bitmap 5x7
 */
static const uint8_t* get_char(char c)
{
    switch (c) {
        case ' ': return ch_space;
        case '.': return ch_dot;
        case '-': return ch_dash;
        case ':': return ch_colon;

        case '0': return ch_0; case '1': return ch_1; case '2': return ch_2;
        case '3': return ch_3; case '4': return ch_4; case '5': return ch_5;
        case '6': return ch_6; case '7': return ch_7; case '8': return ch_8;
        case '9': return ch_9;

        case 'A': return ch_A; case 'C': return ch_C; case 'E': return ch_E;
        case 'G': return ch_G; case 'I': return ch_I; case 'L': return ch_L;
        case 'M': return ch_M; case 'N': return ch_N; case 'O': return ch_O;
        case 'P': return ch_P; case 'Q': return ch_Q; case 'R': return ch_R;
        case 'T': return ch_T; case 'U': return ch_U; case 'X': return ch_X;
        case 'Y': return ch_Y; case 'Z': return ch_Z;
    }
    return ch_space;
}

/**
 * @brief Dibuja un carácter en pantalla
 * @param x Posición X
 * @param y Posición Y
 * @param c Carácter a dibujar
 * @param color Color en RGB565
 * @param scale Escala del carácter
 */
static void draw_char(int x, int y, char c, uint16_t color, int scale)
{
    const uint8_t *glyph = get_char(c);

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if ((glyph[row] >> (4 - col)) & 1) {
                for (int ys = 0; ys < scale; ys++) {
                    for (int xs = 0; xs < scale; xs++) {
                        GC9A01_DrawPixel(x + col*scale + xs, y + row*scale + ys, color);
                    }
                }
            }
        }
    }
}

/**
 * @brief Dibuja texto en pantalla
 * @param x Posición X inicial
 * @param y Posición Y inicial
 * @param txt Cadena de texto
 * @param color Color del texto
 * @param scale Escala del texto
 */
static void draw_text(int x, int y, const char *txt, uint16_t color, int scale)
{
    while (*txt) {
        draw_char(x, y, *txt, color, scale);
        x += 6 * scale;
        txt++;
    }
}

/* ========================= */
/* ========= UI ============ */
/* ========================= */

/**
 * @brief Inicializa la interfaz gráfica
 *
 * Limpia la pantalla y dibuja las etiquetas principales.
 */
void ui_init(void)
{
    GC9A01_FillRect(0, 0, 240, 240, COLOR_BLACK);

    draw_text(90, 30, "QMI8658", COLOR_CYAN, 1);

    draw_text(30, 60,  "ACC", COLOR_YELLOW, 1);
    draw_text(30, 120, "GYR", COLOR_CYAN,   1);
    draw_text(30, 180, "TMP", COLOR_GREEN,  1);
}

/**
 * @brief Muestra un mensaje de estado en pantalla
 * @param msg Cadena de texto
 * @param color Color del texto
 */
void ui_show_status(const char *msg, uint16_t color)
{
    GC9A01_FillRect(60, 210, 120, 20, COLOR_BLACK);
    draw_text(70, 212, msg, color, 1);
}

/**
 * @brief Dibuja un valor numérico con etiqueta
 * @param y Posición vertical
 * @param label Etiqueta (ej: "X:")
 * @param val Valor flotante
 * @param color Color del texto
 */
static void draw_value(int y, const char *label, float val, uint16_t color)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%s %.2f", label, val);

    GC9A01_FillRect(60, y, 120, 16, COLOR_BLACK);
    draw_text(65, y, buf, color, 1);
}

/**
 * @brief Actualiza la UI con nuevos datos del IMU
 * @param imu Puntero a la estructura de datos del sensor
 *
 * @note La estructura qmi8658_data_t debe contener:
 * acc_x, acc_y, acc_z, gyr_x, gyr_y, gyr_z, temperature.
 */
void ui_update(const qmi8658_data_t *imu)
{
    draw_value(75,  "X:", imu->acc_x, COLOR_YELLOW);
    draw_value(90,  "Y:", imu->acc_y, COLOR_YELLOW);
    draw_value(105, "Z:", imu->acc_z, COLOR_YELLOW);

    draw_value(135, "X:", imu->gyr_x, COLOR_CYAN);
    draw_value(150, "Y:", imu->gyr_y, COLOR_CYAN);
    draw_value(165, "Z:", imu->gyr_z, COLOR_CYAN);

    draw_value(195, "T:", imu->temperature, COLOR_GREEN);
}