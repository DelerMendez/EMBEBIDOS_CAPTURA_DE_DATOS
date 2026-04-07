/**
 * @file gc9a01.c
 * @brief Controlador de pantalla TFT circular GC9A01 para ESP-IDF.
 *
 * Este archivo implementa el driver de bajo nivel para la pantalla LCD circular
 * basada en el controlador GC9A01, utilizando el bus SPI del framework ESP-IDF.
 * Soporta modo directo y modo buffer (con opción de PSRAM), control de
 * retroiluminación por GPIO o PWM (LEDC), rotación de pantalla, inversión de
 * color y operaciones de dibujo de píxeles y rectángulos.
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
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "sdkconfig.h"
#include "gc9a01.h"

#if (CONFIG_GC9A01_RESET_USED)
#define RESET_HIGH()           gpio_set_level(CONFIG_GC9A01_PIN_NUM_RST,1)
#define RESET_LOW()            gpio_set_level(CONFIG_GC9A01_PIN_NUM_RST,0)
#endif

#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
#define BLK_HIGH()             gpio_set_level(CONFIG_GC9A01_PIN_NUM_BCKL,1)
#define BLK_LOW()              gpio_set_level(CONFIG_GC9A01_PIN_NUM_BCKL,0)
#endif

#define Cmd_SLPIN       0x10  /**< Comando: entrar en modo sleep */
#define Cmd_SLPOUT      0x11  /**< Comando: salir del modo sleep */
#define Cmd_INVOFF      0x20  /**< Comando: desactivar inversión de color */
#define Cmd_INVON       0x21  /**< Comando: activar inversión de color */
#define Cmd_DISPOFF     0x28  /**< Comando: apagar la pantalla */
#define Cmd_DISPON      0x29  /**< Comando: encender la pantalla */
#define Cmd_CASET       0x2A  /**< Comando: establecer rango de columnas */
#define Cmd_RASET       0x2B  /**< Comando: establecer rango de filas */
#define Cmd_RAMWR       0x2C  /**< Comando: escritura en memoria RAM */
#define Cmd_TEON      	0x35  /**< Comando: activar línea de tearing effect */
#define Cmd_MADCTL      0x36  /**< Comando: control de acceso a memoria de datos */
#define Cmd_COLMOD      0x3A  /**< Comando: configuración del formato de píxel */

#define Cmd_DisplayFunctionControl    0xB6  /**< Comando: control de función de pantalla */
#define Cmd_PWCTR1       0xC1  /**< Comando: control de potencia 1 */
#define Cmd_PWCTR2       0xC3  /**< Comando: control de potencia 2 */
#define Cmd_PWCTR3       0xC4  /**< Comando: control de potencia 3 */
#define Cmd_PWCTR4       0xC9  /**< Comando: control de potencia 4 */
#define Cmd_PWCTR7       0xA7  /**< Comando: control de potencia 7 */

#define Cmd_FRAMERATE      0xE8  /**< Comando: configuración de frecuencia de cuadro */
#define Cmd_InnerReg1Enable   0xFE  /**< Comando: habilitar registro interno 1 */
#define Cmd_InnerReg2Enable   0xEF  /**< Comando: habilitar registro interno 2 */

#define Cmd_GAMMA1       0xF0  /**< Comando: configurar curva gamma 1 */
#define Cmd_GAMMA2       0xF1  /**< Comando: configurar curva gamma 2 */
#define Cmd_GAMMA3       0xF2  /**< Comando: configurar curva gamma 3 */
#define Cmd_GAMMA4       0xF3  /**< Comando: configurar curva gamma 4 */

#define ColorMode_RGB_16bit  0x50  /**< Modo de color: RGB 16 bits (interfaz RGB) */
#define ColorMode_RGB_18bit  0x60  /**< Modo de color: RGB 18 bits (interfaz RGB) */
#define ColorMode_MCU_12bit  0x03  /**< Modo de color: MCU 12 bits */
#define ColorMode_MCU_16bit  0x05  /**< Modo de color: MCU 16 bits */
#define ColorMode_MCU_18bit  0x06  /**< Modo de color: MCU 18 bits */

#define MADCTL_MY        0x80  /**< MADCTL: espejo vertical (fila) */
#define MADCTL_MX        0x40  /**< MADCTL: espejo horizontal (columna) */
#define MADCTL_MV        0x20  /**< MADCTL: intercambio fila/columna (transposición) */
#define MADCTL_ML        0x10  /**< MADCTL: orden de refresco vertical */
#define MADCTL_BGR       0x08  /**< MADCTL: orden de color BGR en lugar de RGB */
#define MADCTL_MH        0x04  /**< MADCTL: orden de refresco horizontal */

uint8_t GC9A01_X_Start = 0, GC9A01_Y_Start = 0;  /**< Desplazamiento de origen en X e Y para el área activa de la pantalla */

#if (CONFIG_GC9A01_BUFFER_MODE)
#if (CONFIG_GC9A01_BUFFER_MODE_PSRAM)
uint16_t *ScreenBuff = NULL;  /**< Puntero al buffer de pantalla en PSRAM (asignado dinámicamente) */
#else
DMA_ATTR uint16_t ScreenBuff[GC9A01_Height * GC9A01_Width];  /**< Buffer de pantalla en memoria interna alineado para DMA */

#endif

#endif

//SPI Config
spi_device_handle_t spi;  /**< Handle del dispositivo SPI utilizado para comunicar con el GC9A01 */
spi_host_device_t LCD_HOST=CONFIG_GC9A01_SPI_HOST;  /**< Host SPI configurado para la pantalla LCD */

//LEDC Config
#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE)
ledc_channel_config_t  ledc_cConfig;  /**< Configuración del canal LEDC para control PWM de retroiluminación */
ledc_timer_config_t    ledc_tConfig;  /**< Configuración del timer LEDC para control PWM de retroiluminación */
void LEDC_PWM_Duty_Set(uint8_t DutyP);
#endif
#endif

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
/**
 * @brief Estructura que representa una entrada en la tabla de inicialización del LCD.
 *
 * Cada entrada contiene un comando SPI y sus datos asociados, utilizados
 * durante la secuencia de inicialización del controlador GC9A01.
 */
typedef struct {
    uint8_t cmd;        /**< Código del comando a enviar al controlador LCD */
    uint8_t data[16];   /**< Bytes de datos asociados al comando (máximo 16) */
    uint8_t databytes;  /**< Número de bytes de datos válidos; bit 7 activo indica delay posterior; 0xFF indica fin de la tabla */
} lcd_init_cmd_t;

static const lcd_init_cmd_t lcd_init_cmds[]={
    {0xef,{0},0},
    {0xeb,{0x14},1},
    {0xfe,{0},0},
    {0xef,{0},0},
    {0xeb,{0x14},1},
    {0x84,{0x40},1},
    {0x85,{0xff},1},
    {0x86,{0xff},1},
    {0x87,{0xff},1},
    {0x88,{0x0a},1},
    {0x89,{0x21},1},
    {0x8a,{0x00},1},
    {0x8b,{0x80},1},
    {0x8c,{0x01},1},
    {0x8d,{0x01},1},
    {0x8e,{0xff},1},
    {0x8f,{0xff},1},
    {Cmd_DisplayFunctionControl,{0x00,0x20},2},// Scan direction S360 -> S1
    //{Cmd_MADCTL,{0x08},1},//MemAccessModeSet(0, 0, 0, 1);
    //{Cmd_COLMOD,{ColorMode_MCU_16bit&0x77},1},
    {0x90,{0x08,0x08,0x08,0x08},4},
    {0xbd,{0x06},1},
    {0xbc,{0x00},1},
    {0xff,{0x60,0x01,0x04},3},
    {Cmd_PWCTR2,{0x13},1},
    {Cmd_PWCTR3,{0x13},1},
    {Cmd_PWCTR4,{0x22},1},
    {0xbe,{0x11},1},
    {0xe1,{0x10,0x0e},2},
    {0xdf,{0x21,0x0c,0x02},3},
    {Cmd_GAMMA1,{0x45,0x09,0x08,0x08,0x26,0x2a},6},
    {Cmd_GAMMA2,{0x43,0x70,0x72,0x36,0x37,0x6f},6},
    {Cmd_GAMMA3,{0x45,0x09,0x08,0x08,0x26,0x2a},6},
    {Cmd_GAMMA4,{0x43,0x70,0x72,0x36,0x37,0x6f},6},
    {0xed,{0x1b,0x0b},2},
    {0xae,{0x77},1},
    {0xcd,{0x63},1},
    {0x70,{0x07,0x07,0x04,0x0e,0x0f,0x09,0x07,0x08,0x03},9},
    {Cmd_FRAMERATE,{0x34},1},// 4 dot inversion
    {0x62,{0x18,0x0D,0x71,0xED,0x70,0x70,0x18,0x0F,0x71,0xEF,0x70,0x70},12},
    {0x63,{0x18,0x11,0x71,0xF1,0x70,0x70,0x18,0x13,0x71,0xF3,0x70,0x70},12},
    {0x64,{0x28,0x29,0xF1,0x01,0xF1,0x00,0x07},7},
    {0x66,{0x3C,0x00,0xCD,0x67,0x45,0x45,0x10,0x00,0x00,0x00},10},
    {0x67,{0x00,0x3C,0x00,0x00,0x00,0x01,0x54,0x10,0x32,0x98},10},
    {0x74,{0x10,0x85,0x80,0x00,0x00,0x4E,0x00},7},
    {0x98,{0x3e,0x07},2},
    {Cmd_TEON,{0},0},// Tearing effect line on
    {0, {0}, 0xff},//END
};

/**
 * @brief Callback de pre-transferencia SPI ejecutado en contexto de interrupción.
 *
 * Esta función es invocada automáticamente por el driver SPI de ESP-IDF justo
 * antes de cada transmisión. Configura el pin D/C (Data/Command) al valor
 * almacenado en el campo @c user de la transacción, indicando si se envía
 * un comando (0) o un dato (1).
 *
 * @warning Esta función se ejecuta en contexto de IRQ (interrupción). No debe
 *          llamar a funciones bloqueantes ni a la API de FreeRTOS que no sean
 *          seguras desde ISR.
 *
 * @param t Puntero a la estructura de transacción SPI activa. El campo
 *          @c t->user contiene el nivel lógico deseado para el pin D/C.
 */
static IRAM_ATTR void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(CONFIG_GC9A01_PIN_NUM_DC, dc);
}

/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
/**
 * @brief Envía un byte de comando al controlador LCD GC9A01 por SPI.
 *
 * Configura el pin D/C en nivel bajo (modo comando) y transmite el byte
 * usando @c spi_device_polling_transmit, que bloquea hasta completar la
 * transferencia. Esta función es bloqueante y no debe usarse desde una ISR.
 *
 * @param cmd Byte de comando a enviar al controlador GC9A01.
 */
void lcd_cmd(uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
/**
 * @brief Envía un bloque de datos al controlador LCD GC9A01 por SPI.
 *
 * Configura el pin D/C en nivel alto (modo datos) y transmite el buffer
 * usando @c spi_device_polling_transmit. Si el tamaño del buffer supera el
 * límite DMA (@c SPI_MAX_DMA_LEN), la transmisión se fragmenta
 * automáticamente en múltiples transferencias consecutivas. Esta función es
 * bloqueante y no debe usarse desde una ISR.
 *
 * @param data Puntero al buffer de datos a transmitir.
 * @param len  Número de bytes a enviar. Si es 0, la función retorna sin acción.
 */
void lcd_data(const uint8_t *data, int len)
{
    esp_err_t ret;
    if (len==0) return;             //no need to send anything

    /*
    On certain MC's the max SPI DMA transfer length might be smaller than the buffer. We then have to split the transmissions.
    */
    int offset = 0;
    do {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));       //Zero out the transaction

        int tx_len = ((len - offset) < SPI_MAX_DMA_LEN) ? (len - offset) : SPI_MAX_DMA_LEN;
        t.length=tx_len * 8;                       //Len is in bytes, transaction length is in bits.
        t.tx_buffer= data + offset;                //Data
        t.user=(void*)1;                           //D/C needs to be set to 1
        ret=spi_device_polling_transmit(spi, &t);  //Transmit!
        assert(ret==ESP_OK);                       //Should have had no issues.
        offset += tx_len;                          // Add the transmission length to the offset
    }
    while (offset < len);
}

/**
 * @brief Envía un único byte de datos al controlador LCD por SPI.
 *
 * Función auxiliar que invoca @c lcd_data con un buffer de un solo byte.
 * Útil para enviar parámetros individuales de comandos de configuración.
 * Esta función es bloqueante.
 *
 * @param Data Byte de datos a enviar al controlador GC9A01.
 */
void lcd_send_byte(uint8_t Data)
{
	lcd_data(&Data,1);
}

/**
 * @brief Genera un retardo bloqueante usando FreeRTOS.
 *
 * Suspende la tarea actual durante el tiempo especificado, cediendo el
 * procesador al scheduler de FreeRTOS. No debe invocarse desde una ISR.
 *
 * @param Delay_ms Tiempo de espera en milisegundos.
 */
void delay_ms (uint32_t Delay_ms)
{
	vTaskDelay(Delay_ms/portTICK_PERIOD_MS);
}

/**
 * @brief Retorna el ancho configurado de la pantalla GC9A01.
 *
 * @return Ancho de la pantalla en píxeles (valor de @c GC9A01_Width).
 */
uint16_t GC9A01_GetWidth() {
	return GC9A01_Width;
}

/**
 * @brief Retorna la altura configurada de la pantalla GC9A01.
 *
 * @return Altura de la pantalla en píxeles (valor de @c GC9A01_Height).
 */
uint16_t GC9A01_GetHeight() {
	return GC9A01_Height;
}

/**
 * @brief Realiza un reset por hardware del controlador GC9A01.
 *
 * Pulsa el pin RST a nivel bajo durante 10 ms y luego lo lleva a nivel alto,
 * esperando 150 ms para que el controlador complete su secuencia de reset.
 * Solo tiene efecto si @c CONFIG_GC9A01_RESET_USED está habilitado en
 * la configuración de menuconfig. Esta función es bloqueante.
 */
void GC9A01_HardReset(void) {
	#if (CONFIG_GC9A01_RESET_USED)
	RESET_LOW();
	delay_ms(10);
	RESET_HIGH();
	delay_ms(150);
	#endif
}

/**
 * @brief Controla el modo sleep del controlador GC9A01.
 *
 * Envía el comando de entrada o salida de modo sleep según el parámetro.
 * Después del comando se espera 500 ms como lo requiere la hoja de datos
 * del GC9A01. Esta función es bloqueante.
 *
 * @param Mode  1 para entrar en modo sleep (pantalla apagada, bajo consumo),
 *              0 para salir del modo sleep (pantalla operativa).
 */
void GC9A01_SleepMode(uint8_t Mode) {
	if (Mode)
		lcd_cmd(Cmd_SLPIN);
	else
		lcd_cmd(Cmd_SLPOUT);

	delay_ms(500);
}

/**
 * @brief Controla la inversión de colores de la pantalla GC9A01.
 *
 * Activa o desactiva la inversión de todos los colores mostrados en pantalla.
 *
 * @param Mode  1 para activar la inversión de color,
 *              0 para desactivarla.
 */
void GC9A01_InversionMode(uint8_t Mode) {
	if (Mode)
		lcd_cmd(Cmd_INVON);
	else
		lcd_cmd(Cmd_INVOFF);
}

/**
 * @brief Enciende o apaga el panel de la pantalla GC9A01.
 *
 * Controla el estado de encendido del panel LCD sin afectar el contenido
 * de la memoria de video ni la retroiluminación.
 *
 * @param On  1 para encender la pantalla (DISPON),
 *            0 para apagarla (DISPOFF).
 */
void GC9A01_DisplayPower(uint8_t On) {
	if (On)
		lcd_cmd(Cmd_DISPON);
	else
		lcd_cmd(Cmd_DISPOFF);
}

/**
 * @brief Configura el rango de columnas activo para la siguiente escritura en RAM.
 *
 * Envía el comando CASET con las coordenadas de inicio y fin de columna,
 * aplicando el desplazamiento @c GC9A01_X_Start. Si los parámetros son
 * inválidos (inicio > fin o fin excede el ancho), la función retorna sin acción.
 *
 * @param ColumnStart Columna de inicio del área de escritura (0-based).
 * @param ColumnEnd   Columna de fin del área de escritura (inclusiva).
 */
static void ColumnSet(uint16_t ColumnStart, uint16_t ColumnEnd) {
	if (ColumnStart > ColumnEnd)
		return;
	if (ColumnEnd > GC9A01_Width)
		return;

	ColumnStart += GC9A01_X_Start;
	ColumnEnd += GC9A01_X_Start;

	lcd_cmd(Cmd_CASET);
	lcd_send_byte(ColumnStart >> 8);
	lcd_send_byte(ColumnStart & 0xFF);
	lcd_send_byte(ColumnEnd >> 8);
	lcd_send_byte(ColumnEnd & 0xFF);
}

/**
 * @brief Configura el rango de filas activo para la siguiente escritura en RAM.
 *
 * Envía el comando RASET con las coordenadas de inicio y fin de fila,
 * aplicando el desplazamiento @c GC9A01_Y_Start. Si los parámetros son
 * inválidos (inicio > fin o fin excede la altura), la función retorna sin acción.
 *
 * @param RowStart Fila de inicio del área de escritura (0-based).
 * @param RowEnd   Fila de fin del área de escritura (inclusiva).
 */
static void RowSet(uint16_t RowStart, uint16_t RowEnd) {
	if (RowStart > RowEnd)
		return;
	if (RowEnd > GC9A01_Height)
		return;

	RowStart += GC9A01_Y_Start;
	RowEnd += GC9A01_Y_Start;

	lcd_cmd(Cmd_RASET);
	lcd_send_byte(RowStart >> 8);
	lcd_send_byte(RowStart & 0xFF);
	lcd_send_byte(RowEnd >> 8);
	lcd_send_byte(RowEnd & 0xFF);
}

/**
 * @brief Define la ventana de escritura activa en la RAM del GC9A01.
 *
 * Configura el área rectangular de destino mediante los comandos CASET y RASET,
 * luego envía RAMWR para habilitar la escritura de datos de píxel. Las llamadas
 * a @c GC9A01_RamWrite o @c lcd_data posteriores depositarán datos dentro de
 * esta ventana.
 *
 * @param x0 Coordenada X de la esquina superior izquierda.
 * @param y0 Coordenada Y de la esquina superior izquierda.
 * @param x1 Coordenada X de la esquina inferior derecha (inclusiva).
 * @param y1 Coordenada Y de la esquina inferior derecha (inclusiva).
 */
void GC9A01_SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
	ColumnSet(x0, x1);
	RowSet(y0, y1);

	lcd_cmd(Cmd_RAMWR);
}

/**
 * @brief Configura el formato de color (profundidad de color) del controlador.
 *
 * Envía el comando COLMOD con el modo de color solicitado, enmascarando
 * los bits reservados con 0x77 según la hoja de datos del GC9A01.
 *
 * @param ColorMode Valor del modo de color. Usar las constantes
 *                  @c ColorMode_MCU_16bit, @c ColorMode_MCU_18bit, etc.
 */
static void ColorModeSet(uint8_t ColorMode) {
	lcd_cmd(Cmd_COLMOD);
	lcd_send_byte(ColorMode & 0x77);
}

/**
 * @brief Configura la orientación y el orden de acceso a la memoria de video.
 *
 * Envía el comando MADCTL con los bits de control construidos a partir de
 * los parámetros. Soporta 8 rotaciones (0–7) y opciones de espejo horizontal,
 * espejo vertical y orden de subpíxeles BGR.
 *
 * @param Rotation    Orientación de la pantalla: valores de 0 a 7.
 *                    - 0: Normal
 *                    - 1: Espejo horizontal (MX)
 *                    - 2: Espejo vertical (MY)
 *                    - 3: Rotación 180° (MX | MY)
 *                    - 4: Transposición (MV)
 *                    - 5: MV + MX
 *                    - 6: MV + MY
 *                    - 7: MV + MX + MY
 * @param VertMirror  1 para activar espejo de refresco vertical (MADCTL_ML).
 * @param HorizMirror 1 para activar espejo de refresco horizontal (MADCTL_MH).
 * @param IsBGR       1 para usar orden de subpíxeles BGR; 0 para RGB.
 */
static void MemAccessModeSet(uint8_t Rotation, uint8_t VertMirror,
		uint8_t HorizMirror, uint8_t IsBGR) {
	uint8_t Ret=0;
	Rotation &= 7;

	lcd_cmd(Cmd_MADCTL);

	switch (Rotation) {
	case 0:
		Ret = 0;
		break;
	case 1:
		Ret = MADCTL_MX;
		break;
	case 2:
		Ret = MADCTL_MY;
		break;
	case 3:
		Ret = MADCTL_MX | MADCTL_MY;
		break;
	case 4:
		Ret = MADCTL_MV;
		break;
	case 5:
		Ret = MADCTL_MV | MADCTL_MX;
		break;
	case 6:
		Ret = MADCTL_MV | MADCTL_MY;
		break;
	case 7:
		Ret = MADCTL_MV | MADCTL_MX | MADCTL_MY;
		break;
	}

	if (VertMirror)
		Ret = MADCTL_ML;
	if (HorizMirror)
		Ret = MADCTL_MH;

	if (IsBGR)
		Ret |= MADCTL_BGR;

	lcd_send_byte(Ret);
}

/**
 * @brief Controla el nivel de retroiluminación de la pantalla.
 *
 * Si el modo PWM está habilitado (@c CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE),
 * ajusta el ciclo de trabajo del canal LEDC. En caso contrario, conmuta el
 * pin de retroiluminación de forma digital (encendido/apagado).
 * Si el control de retroiluminación no está habilitado en menuconfig,
 * la función no tiene efecto.
 *
 * @param Value Nivel de brillo deseado, de 0 (apagado) a 100 (máximo brillo).
 *              Valores superiores a 100 se recortan a 100.
 */
void GC9A01_SetBL(uint8_t Value)
{
	if (Value > 100) Value = 100;
	#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
		#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE)
		LEDC_PWM_Duty_Set(Value);
		#else
		if (Value) BLK_HIGH();
		else BLK_LOW();
		#endif
	#endif
}

//Direct Mode
#if (!CONFIG_GC9A01_BUFFER_MODE)

	/**
	 * @brief Escribe un bloque de píxeles directamente en la RAM del GC9A01 (modo directo).
	 *
	 * Envía @p Len píxeles en formato RGB565 big-endian directamente al
	 * controlador por SPI. Debe invocarse después de @c GC9A01_SetWindow.
	 * Esta función es bloqueante. Solo disponible cuando
	 * @c CONFIG_GC9A01_BUFFER_MODE está desactivado.
	 *
	 * @param pBuff Puntero al buffer de píxeles en formato RGB565.
	 * @param Len   Número de píxeles a escribir.
	 */
	void GC9A01_RamWrite(uint16_t *pBuff, uint16_t Len)
	{
	while (Len--)
	{
		lcd_send_byte(*pBuff >> 8);
		lcd_send_byte(*pBuff & 0xFF);
	}
	}

	/**
	 * @brief Dibuja un píxel individual en la pantalla (modo directo).
	 *
	 * Configura una ventana de 1x1 píxel y envía el color por SPI.
	 * Si las coordenadas están fuera de los límites de la pantalla, la función
	 * retorna sin acción. Esta función es bloqueante. Solo disponible cuando
	 * @c CONFIG_GC9A01_BUFFER_MODE está desactivado.
	 *
	 * @param x     Coordenada X del píxel (0 a GC9A01_Width - 1).
	 * @param y     Coordenada Y del píxel (0 a GC9A01_Height - 1).
	 * @param color Color del píxel en formato RGB565.
	 */
	void GC9A01_DrawPixel(int16_t x, int16_t y, uint16_t color)
	{
	if ((x < 0) ||(x >= GC9A01_Width) || (y < 0) || (y >= GC9A01_Height))
		return;

	GC9A01_SetWindow(x, y, x, y);
	GC9A01_RamWrite(&color, 1);
	}

	/**
	 * @brief Rellena un rectángulo con un color sólido (modo directo).
	 *
	 * Recorta el rectángulo a los límites de la pantalla si es necesario,
	 * configura la ventana de escritura y envía el color píxel a píxel por SPI.
	 * Esta función es bloqueante. Solo disponible cuando
	 * @c CONFIG_GC9A01_BUFFER_MODE está desactivado.
	 *
	 * @param x     Coordenada X de la esquina superior izquierda.
	 * @param y     Coordenada Y de la esquina superior izquierda.
	 * @param w     Ancho del rectángulo en píxeles.
	 * @param h     Alto del rectángulo en píxeles.
	 * @param color Color de relleno en formato RGB565.
	 */
	void GC9A01_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
	{
	if ((x >= GC9A01_Width) || (y >= GC9A01_Height))
		return;

	if ((x + w) > GC9A01_Width)
		w = GC9A01_Width - x;

	if ((y + h) > GC9A01_Height)
		h = GC9A01_Height - y;

	GC9A01_SetWindow(x, y, x + w - 1, y + h - 1);

	for (uint32_t i = 0; i < (h * w); i++)
		GC9A01_RamWrite(&color, 1);
	}

//Buffer mode
#else

	/**
	 * @brief Intercambia los bytes alto y bajo de un valor de color RGB565.
	 *
	 * Convierte entre el orden de bytes little-endian del procesador y el
	 * big-endian requerido por el controlador GC9A01. Se usa internamente
	 * antes de almacenar colores en el buffer de pantalla.
	 *
	 * @param color Puntero al valor de color a convertir (modificado in-place).
	 */
	static void SwapBytes(uint16_t *color) {
		uint8_t temp = *color >> 8;
		*color = (*color << 8) | temp;
	}

	/**
	 * @brief Dibuja un píxel individual en el buffer de pantalla (modo buffer).
	 *
	 * Almacena el color en la posición correspondiente del buffer @c ScreenBuff,
	 * con los bytes intercambiados para compatibilidad con el controlador.
	 * Si las coordenadas están fuera de los límites, la función retorna sin acción.
	 * El cambio no es visible hasta llamar a @c GC9A01_Update.
	 * Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE está activado.
	 *
	 * @param x     Coordenada X del píxel (0 a GC9A01_Width - 1).
	 * @param y     Coordenada Y del píxel (0 a GC9A01_Height - 1).
	 * @param color Color del píxel en formato RGB565.
	 */
	void GC9A01_DrawPixel(int16_t x, int16_t y, uint16_t color) {
		if ((x < 0) || (x >= GC9A01_Width) || (y < 0) || (y >= GC9A01_Height))
			return;

		SwapBytes(&color);

		ScreenBuff[y * GC9A01_Width + x] = color;
	}

	/**
	 * @brief Obtiene el color de un píxel del buffer de pantalla (modo buffer).
	 *
	 * Lee el color almacenado en la posición indicada del buffer @c ScreenBuff
	 * y lo retorna en formato RGB565 con los bytes en orden correcto.
	 * Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE está activado.
	 *
	 * @param x Coordenada X del píxel (0 a GC9A01_Width - 1).
	 * @param y Coordenada Y del píxel (0 a GC9A01_Height - 1).
	 * @return  Color del píxel en formato RGB565, o 0 si las coordenadas
	 *          están fuera de los límites.
	 */
	uint16_t GC9A01_GetPixel(int16_t x, int16_t y) {
		if ((x < 0) || (x >= GC9A01_Width) || (y < 0) || (y >= GC9A01_Height))
			return 0;

		uint16_t color = ScreenBuff[y * GC9A01_Width + x];
		SwapBytes(&color);
		return color;
	}

	/**
	 * @brief Rellena un rectángulo con un color sólido en el buffer de pantalla (modo buffer).
	 *
	 * Escribe directamente en @c ScreenBuff sin enviar datos al controlador.
	 * Recorta el área al tamaño visible. Los cambios se hacen visibles al
	 * llamar a @c GC9A01_Update. Solo disponible cuando
	 * @c CONFIG_GC9A01_BUFFER_MODE está activado.
	 *
	 * @param x     Coordenada X de la esquina superior izquierda (puede ser negativa).
	 * @param y     Coordenada Y de la esquina superior izquierda (puede ser negativa).
	 * @param w     Ancho del rectángulo en píxeles.
	 * @param h     Alto del rectángulo en píxeles.
	 * @param color Color de relleno en formato RGB565.
	 */
	void GC9A01_FillRect(int16_t x, int16_t y, int16_t w, int16_t h,
			uint16_t color) {
		if ((w <= 0) || (h <= 0) || (x >= GC9A01_Width) || (y >= GC9A01_Height))
			return;

		if (x < 0) {
			w += x;
			x = 0;
		}
		if (y < 0) {
			h += y;
			y = 0;
		}

		if ((w <= 0) || (h <= 0))
			return;

		if ((x + w) > GC9A01_Width)
			w = GC9A01_Width - x;
		if ((y + h) > GC9A01_Height)
			h = GC9A01_Height - y;

		SwapBytes(&color);

		for (uint16_t row = 0; row < h; row++) {
			for (uint16_t col = 0; col < w; col++)
				//GC9A01_DrawPixel(col, row, color);
				ScreenBuff[(y + row) * GC9A01_Width + x + col] = color;
		}
	}

	/**
	 * @brief Vuelca el buffer de pantalla completo al controlador GC9A01 por SPI.
	 *
	 * Configura la ventana de escritura para la pantalla completa y transfiere
	 * todo el contenido de @c ScreenBuff mediante @c lcd_data. Esta función
	 * es bloqueante y puede tomar varios milisegundos dependiendo de la velocidad
	 * del bus SPI. Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE está activado.
	 */
	void GC9A01_Update()
	{
		int len = GC9A01_Width * GC9A01_Height;
		GC9A01_SetWindow(0, 0, GC9A01_Width - 1, GC9A01_Height - 1);
		lcd_data((uint8_t*) &ScreenBuff[0], len*2);
	}

	/**
	 * @brief Limpia el buffer de pantalla llenándolo con color negro.
	 *
	 * Equivale a llamar @c GC9A01_FillRect con color 0x0000 sobre toda la
	 * pantalla. El buffer no se transfiere al controlador; es necesario llamar
	 * a @c GC9A01_Update para visualizar el resultado.
	 * Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE está activado.
	 */
	void GC9A01_Clear(void)
	{
		GC9A01_FillRect(0, 0, GC9A01_Width, GC9A01_Height, 0x0000);
	}

#endif

/**
 * @brief Inicializa los pines GPIO necesarios para el controlador GC9A01.
 *
 * Configura como salida el pin D/C y, según la configuración de menuconfig,
 * también los pines RST (reset por hardware) y BL (retroiluminación en modo
 * GPIO). Los pines se configuran sin pull-up, sin pull-down y sin interrupciones.
 * Esta función debe llamarse antes de @c gc9a01_SPI_init.
 */
static void gc9a01_GPIO_init(void)
{
	gpio_config_t gpiocfg={
		.pin_bit_mask= ((uint64_t)1UL<<CONFIG_GC9A01_PIN_NUM_DC),
		.mode=GPIO_MODE_OUTPUT,
		.pull_up_en=GPIO_PULLUP_DISABLE,
		.pull_down_en=GPIO_PULLDOWN_DISABLE,
		.intr_type=GPIO_INTR_DISABLE,
	};

	gpio_config(&gpiocfg);
	gpio_set_level(CONFIG_GC9A01_PIN_NUM_DC,0);

	#if(CONFIG_GC9A01_RESET_USED)
	gpiocfg.pin_bit_mask|=((uint64_t)1UL<<CONFIG_GC9A01_PIN_NUM_RST);
	gpio_config(&gpiocfg);
	gpio_set_level(CONFIG_GC9A01_PIN_NUM_RST,1);
	#endif




	#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
	#if(!CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE)
	gpiocfg.pin_bit_mask|=((uint64_t)1UL<<CONFIG_GC9A01_PIN_NUM_BCKL);
	gpio_config(&gpiocfg);
	gpio_set_level(CONFIG_GC9A01_PIN_NUM_BCKL,0);
	#endif
	#endif

}

/**
 * @brief Inicializa el bus SPI y registra el dispositivo GC9A01.
 *
 * Configura el bus SPI con los pines MOSI, SCK y CS definidos en menuconfig,
 * activa la gestión automática de canales DMA y registra el dispositivo con
 * la frecuencia de reloj y el callback de pre-transferencia. Utiliza
 * @c ESP_ERROR_CHECK para abortar en caso de error. Esta función es bloqueante
 * y no es thread-safe respecto a otros accesos al mismo bus SPI.
 */
void gc9a01_SPI_init(void)
{
	esp_err_t ret;
    spi_bus_config_t buscfg={
        .mosi_io_num=CONFIG_GC9A01_PIN_NUM_MOSI,
        .miso_io_num=GPIO_NUM_NC,
        .sclk_io_num=CONFIG_GC9A01_PIN_NUM_SCK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=250*250*2,
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=CONFIG_GC9A01_SPI_SCK_FREQ_M*1000*1000,
        .mode=0,
        .spics_io_num=CONFIG_GC9A01_PIN_NUM_CS,
        .queue_size=7,
        .pre_cb=lcd_spi_pre_transfer_callback,
    };

    ret=spi_bus_initialize(LCD_HOST,&buscfg,SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    ret=spi_bus_add_device(LCD_HOST,&devcfg,&spi);
    ESP_ERROR_CHECK(ret);
}

#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE)
/**
 * @brief Ajusta el ciclo de trabajo del canal LEDC para controlar el brillo.
 *
 * Convierte el porcentaje de brillo (@p DutyP) al valor entero correspondiente
 * según la resolución de bits configurada en @c ledc_tConfig, y aplica la
 * configuración al canal LEDC. Solo disponible cuando
 * @c CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE está activado.
 *
 * @param DutyP Ciclo de trabajo en porcentaje (0–100).
 *              Valores ≥ 100 establecen el duty máximo.
 */
void LEDC_PWM_Duty_Set(uint8_t DutyP)
{
	uint16_t Duty,MaxD;

	MaxD=(1<<(int)ledc_tConfig.duty_resolution)-1;

	if(DutyP>=100)Duty=MaxD;
	else
	{
		Duty=DutyP*(MaxD/(float)100);
	}
	ledc_cConfig.duty=Duty;
	ledc_channel_config(&ledc_cConfig);
}

/**
 * @brief Configura el canal LEDC para el control PWM de la retroiluminación.
 *
 * Asigna el GPIO de retroiluminación al canal 0 del LEDC en modo de baja
 * velocidad, con duty inicial de 0 y sin interrupciones. Debe llamarse
 * después de @c LEDC_Timer_Config. Solo disponible cuando
 * @c CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE está activado.
 */
void LEDC_Channel_Config(void)
{
	ledc_cConfig.gpio_num=CONFIG_GC9A01_PIN_NUM_BCKL;
	ledc_cConfig.speed_mode=LEDC_LOW_SPEED_MODE;
	ledc_cConfig.channel=LEDC_CHANNEL_0;
	ledc_cConfig.intr_type=LEDC_INTR_DISABLE;
	ledc_cConfig.timer_sel=LEDC_TIMER_0;
	ledc_cConfig.duty=0;
	ledc_cConfig.hpoint=0;
	ledc_channel_config(&ledc_cConfig);
}

/**
 * @brief Configura el timer LEDC utilizado para la retroiluminación PWM.
 *
 * Inicializa el timer 0 del LEDC en modo de baja velocidad con resolución
 * de 8 bits, frecuencia de 1 kHz y clock automático. Solo disponible cuando
 * @c CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE está activado.
 */
void LEDC_Timer_Config(void)
{
	ledc_tConfig.speed_mode=LEDC_LOW_SPEED_MODE ;
	ledc_tConfig.duty_resolution=LEDC_TIMER_8_BIT;
	//ledc_tConfig.bit_num=LEDC_TIMER_8_BIT;
	ledc_tConfig.timer_num=LEDC_TIMER_0;
	ledc_tConfig.freq_hz=1000;
	ledc_tConfig.clk_cfg=LEDC_AUTO_CLK;
	ledc_timer_config(&ledc_tConfig);
}
#endif
#endif

/**
 * @brief Inicializa completamente el controlador de pantalla GC9A01.
 *
 * Realiza la secuencia completa de inicialización del driver:
 * -# Asigna el buffer en PSRAM si corresponde (@c CONFIG_GC9A01_BUFFER_MODE_PSRAM).
 * -# Inicializa los GPIOs mediante @c gc9a01_GPIO_init.
 * -# Inicializa el bus SPI mediante @c gc9a01_SPI_init.
 * -# Configura LEDC para PWM de retroiluminación si está habilitado.
 * -# Realiza reset por hardware si está habilitado.
 * -# Envía la tabla completa de comandos de inicialización @c lcd_init_cmds.
 * -# Configura orientación, modo de color, inversión y salida del modo sleep.
 * -# Enciende la pantalla y, en modo buffer, realiza un primer volcado limpio.
 * -# Establece la retroiluminación al 100% si el control está habilitado.
 *
 * Esta función es bloqueante e incluye múltiples retardos requeridos por la
 * hoja de datos del GC9A01. Debe llamarse una única vez desde la tarea de
 * inicialización de la aplicación.
 */
void GC9A01_Init()
{
	int cmd=0;

	GC9A01_X_Start = 0;
	GC9A01_Y_Start = 0;

    #if (CONFIG_GC9A01_BUFFER_MODE_PSRAM)
    if (ScreenBuff == NULL) {
        // ScreenBuffer has not yet been allocated
        ScreenBuff = heap_caps_malloc((GC9A01_Height * GC9A01_Width) * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT );
    }
    #endif

	gc9a01_GPIO_init();
	gc9a01_SPI_init();

	#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
	#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_MODE)
	LEDC_Timer_Config();
	LEDC_Channel_Config();
	#endif
	#endif

	#if(CONFIG_GC9A01_RESET_USED)
	GC9A01_HardReset();
	#endif

    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff)
	{
        lcd_cmd(lcd_init_cmds[cmd].cmd);
        lcd_data(lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80)
		{
            delay_ms(100);
        }
        cmd++;
    }

	MemAccessModeSet(0,0,0,1);
	ColorModeSet(ColorMode_MCU_16bit);

	GC9A01_InversionMode(1);
	GC9A01_SleepMode(0);

	delay_ms(120);
	GC9A01_DisplayPower(1);
	delay_ms(20);

	#if(CONFIG_GC9A01_BUFFER_MODE)
	GC9A01_Clear();
	GC9A01_Update();
	delay_ms(30);
	#endif

	#if(CONFIG_GC9A01_CONTROL_BACK_LIGHT_USED)
	GC9A01_SetBL(100);
	#endif
}


#if(CONFIG_GC9A01_BUFFER_MODE)

#if (CONFIG_GC9A01_BUFFER_MODE_PSRAM)
/**
 * @brief Libera el buffer de pantalla asignado en PSRAM.
 *
 * Llama a @c free sobre @c ScreenBuff si no es NULL. Debe usarse cuando
 * el driver ya no sea necesario para evitar fugas de memoria en PSRAM.
 * Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE_PSRAM está activado.
 */
void GC9A01_Free(void) {
    if (ScreenBuff != NULL) {
        free(ScreenBuff);
    }
}
#endif


/**
 * @brief Captura una región rectangular de la pantalla en un buffer externo.
 *
 * Copia los píxeles del área especificada desde @c ScreenBuff (o mediante
 * @c GC9A01_GetPixel si @c CONFIG_GC9A01_BUFFER_SCREEN_FAST_MODE está
 * desactivado) hacia el buffer @p Buffer proporcionado por el usuario.
 * Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE está activado.
 *
 * @param x      Coordenada X de la esquina superior izquierda del área a capturar.
 * @param y      Coordenada Y de la esquina superior izquierda del área a capturar.
 * @param width  Ancho del área a capturar en píxeles.
 * @param height Alto del área a capturar en píxeles.
 * @param Buffer Puntero al buffer destino donde se almacenarán los píxeles
 *               en formato RGB565. Debe tener capacidad para al menos
 *               @p width × @p height elementos de tipo @c uint16_t.
 */
void GC9A01_Screen_Shot(uint16_t x,uint16_t y,uint16_t width ,uint16_t height,uint16_t * Buffer)
{
	uint16_t i,j;
	for (i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			#if(!CONFIG_GC9A01_BUFFER_SCREEN_FAST_MODE)
			Buffer[i*width+j]=GC9A01_GetPixel(x+j,y+i);
			#else
			Buffer[i*width+j]=ScreenBuff[((y+i) * GC9A01_Width )+ (x+j)];
			#endif
		}
	}
}

/**
 * @brief Carga una región de píxeles desde un buffer externo hacia el buffer de pantalla.
 *
 * Copia los píxeles del buffer @p Buffer a las posiciones correspondientes
 * en @c ScreenBuff (o mediante @c GC9A01_DrawPixel si
 * @c CONFIG_GC9A01_BUFFER_SCREEN_FAST_MODE está desactivado). Los cambios
 * no son visibles hasta llamar a @c GC9A01_Update.
 * Solo disponible cuando @c CONFIG_GC9A01_BUFFER_MODE está activado.
 *
 * @param x      Coordenada X de la esquina superior izquierda del área destino.
 * @param y      Coordenada Y de la esquina superior izquierda del área destino.
 * @param width  Ancho del área a cargar en píxeles.
 * @param height Alto del área a cargar en píxeles.
 * @param Buffer Puntero al buffer fuente con los píxeles en formato RGB565.
 *               Debe contener al menos @p width × @p height elementos de
 *               tipo @c uint16_t.
 */
void GC9A01_Screen_Load(uint16_t x,uint16_t y,uint16_t width ,uint16_t height,uint16_t * Buffer)
{
	uint16_t i,j;
	for (i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			#if(!CONFIG_GC9A01_BUFFER_SCREEN_FAST_MODE)
			GC9A01_DrawPixel(x+j,y+i,Buffer[i*width+j]);
			#else
			ScreenBuff[((y+i) * GC9A01_Width )+ (x+j)] = Buffer[i*width+j];
			#endif
		}
	}
}

#endif