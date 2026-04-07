/**
 * @file GC9A01.h
 * @brief Interfaz de control para el display LCD basado en el controlador GC9A01 utilizando ESP-IDF.
 *
 * Este archivo define las funciones necesarias para inicializar, controlar y dibujar en un display
 * GC9A01. Incluye operaciones básicas como dibujo de píxeles, llenado de regiones, control de energía,
 * y manejo de buffers (incluyendo soporte opcional para PSRAM).
 *
 * Las funciones están diseñadas para ser utilizadas en sistemas embebidos con ESP-IDF. Dependiendo
 * de la implementación en el archivo fuente, algunas funciones pueden ser bloqueantes debido al uso
 * de comunicación SPI y no son inherentemente thread-safe si se accede al bus desde múltiples tareas.
 *
 * @author Deler Mendez
 * @author Nathalia Piamba
 * @author Danny Ramirez
 * @version 1.0
 */

//--------------------------------------------------------------------------------------------------------
// Nadyrshin Ruslan - [YouTube-channel: https://www.youtube.com/channel/UChButpZaL5kUUl_zTyIDFkQ]
// Liyanboy74
//--------------------------------------------------------------------------------------------------------
#ifndef _GC9A01_H
#define _GC9A01_H

/**
 * @def GC9A01_Width
 * @brief Ancho del display definido por configuración en ESP-IDF.
 */
#define GC9A01_Width	CONFIG_GC9A01_Width

/**
 * @def GC9A01_Height
 * @brief Alto del display definido por configuración en ESP-IDF.
 */
#define GC9A01_Height 	CONFIG_GC9A01_Height

/**
 * @brief Obtiene el ancho actual del display.
 *
 * @return uint16_t Ancho del display en píxeles.
 */
uint16_t GC9A01_GetWidth();

/**
 * @brief Obtiene el alto actual del display.
 *
 * @return uint16_t Alto del display en píxeles.
 */
uint16_t GC9A01_GetHeight();

/**
 * @brief Inicializa el display GC9A01.
 *
 * Configura la interfaz de comunicación (usualmente SPI), inicializa registros internos
 * del controlador y prepara el display para su uso.
 *
 * @note Esta función es bloqueante y debe ejecutarse antes de cualquier operación gráfica.
 * @note No es thread-safe si múltiples tareas acceden al bus SPI simultáneamente.
 */
void GC9A01_Init();

#if (CONFIG_GC9A01_BUFFER_MODE_PSRAM)
/**
 * @brief Libera el buffer asignado en PSRAM.
 *
 * Esta función libera la memoria previamente asignada en PSRAM cuando se utiliza
 * el modo de buffer para el display.
 *
 * @note Debe llamarse cuando ya no se requiera el buffer para evitar fugas de memoria.
 * @note No es thread-safe si se accede simultáneamente desde múltiples tareas.
 */
void GC9A01_Free(void);
#endif

/**
 * @brief Activa o desactiva el modo de suspensión del display.
 *
 * @param Mode Valor que indica el estado:
 *             - 0: Desactivar modo sleep (display activo)
 *             - 1: Activar modo sleep (bajo consumo)
 *
 * @note Puede implicar retardos internos del hardware (función potencialmente bloqueante).
 */
void GC9A01_SleepMode(uint8_t Mode);

/**
 * @brief Controla la alimentación del display.
 *
 * @param On Valor de control:
 *           - 0: Apagar display
 *           - 1: Encender display
 *
 * @note Esta función puede afectar el consumo energético del sistema.
 */
void GC9A01_DisplayPower(uint8_t On);

/**
 * @brief Dibuja un píxel en una posición específica.
 *
 * @param x Coordenada horizontal (posición en X).
 * @param y Coordenada vertical (posición en Y).
 * @param color Color del píxel en formato RGB565.
 *
 * @note Función bloqueante debido al acceso al bus de comunicación.
 * @note No es thread-safe si múltiples tareas dibujan simultáneamente.
 */
void GC9A01_DrawPixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief Rellena un rectángulo con un color específico.
 *
 * @param x Coordenada X de la esquina superior izquierda.
 * @param y Coordenada Y de la esquina superior izquierda.
 * @param w Ancho del rectángulo.
 * @param h Alto del rectángulo.
 * @param color Color de relleno en formato RGB565.
 *
 * @note Operación potencialmente costosa y bloqueante.
 */
void GC9A01_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Actualiza el contenido del display.
 *
 * Envía el buffer interno (si se usa) al display físico.
 *
 * @note Función bloqueante que transmite datos por SPI.
 * @note Debe llamarse después de modificar el buffer en modo bufferizado.
 */
void GC9A01_Update();

/**
 * @brief Ajusta el nivel de brillo de la retroiluminación (Backlight).
 *
 * @param Value Valor del brillo (dependiente del hardware, típicamente 0–255).
 *
 * @note Puede utilizar PWM dependiendo de la implementación.
 */
void GC9A01_SetBL(uint8_t Value);

/**
 * @brief Obtiene el color de un píxel en una posición específica.
 *
 * @param x Coordenada horizontal (posición en X).
 * @param y Coordenada vertical (posición en Y).
 *
 * @return uint16_t Color del píxel en formato RGB565.
 *
 * @note Puede requerir acceso al buffer interno o lectura del display.
 */
uint16_t GC9A01_GetPixel(int16_t x, int16_t y);

/**
 * @brief Captura una región del display en un buffer.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param width Ancho de la región a capturar.
 * @param height Alto de la región a capturar.
 * @param Buffer Puntero al buffer donde se almacenarán los datos.
 *
 * @note El buffer debe tener suficiente memoria para almacenar width * height píxeles.
 * @note Operación bloqueante.
 */
void GC9A01_Screen_Shot(uint16_t x,uint16_t y,uint16_t width ,uint16_t height,uint16_t * Buffer);

/**
 * @brief Carga una imagen desde un buffer hacia el display.
 *
 * @param x Coordenada X inicial.
 * @param y Coordenada Y inicial.
 * @param width Ancho de la imagen.
 * @param height Alto de la imagen.
 * @param Buffer Puntero al buffer que contiene los datos de la imagen.
 *
 * @note El buffer debe contener datos en formato RGB565.
 * @note Operación bloqueante debido a la transferencia de datos.
 */
void GC9A01_Screen_Load(uint16_t x,uint16_t y,uint16_t width ,uint16_t height,uint16_t * Buffer);

#endif