/*============================================================================
 * Autor:
 * Licencia:
 * Fecha:
 *===========================================================================*/

// Inlcusiones

#include "app.h"  // <= Su propia cabecera
#include "sapi.h"               // <= Biblioteca sAPI
#include "ff.h"                 // <= Biblioteca FAT FS
#include "sapi_adc128d818.h"
#include "sapi_sdcard.h"
#include <string.h>

#define BAUD_RATE 115200
#define BF_L      1024          // Uart's string buffer length

/*==================[internal data declaration]==============================*/


/*==================[internal functions declaration]=========================*/

/** @brief hardware initialization function
*	@return none
*/
//static void initHardware(void);

/*==================[internal data definition]===============================*/

char intbuff[1024];

/*==================[external data definition]===============================*/

/*==================[internal functions definition]==========================*/

char *intToHex(uint64_t value, uint8_t digits)
{
    char *ptr1;
    char *ptr2;
    char c;
    int ind = 0;
        
    do {
        if ((value & 0xf) < 10)
            intbuff[ind++] = '0'+(value & 0xf);
        else
            intbuff[ind++] = 'a'+((value & 0xf)-10);
        value = (value - (value & 0xf)) >> 4;
        digits--;
    } while (digits != 0);
    
    intbuff[ind] = '\0';
    
    ptr1 = intbuff;
    ptr2 = intbuff+(ind-1);
    while (ptr1 < ptr2) {
        c = *ptr1;
        *ptr1++ = *ptr2;
        *ptr2-- = c;
    }
    
    return intbuff;
}


void reset_all_douts(void) 
{
    gpioWrite( DO0, OFF );
    gpioWrite( DO1, OFF );
    gpioWrite( DO2, OFF );
    gpioWrite( DO3, OFF );
    gpioWrite( DO4, OFF );
    gpioWrite( DO5, OFF );
    gpioWrite( DO6, OFF );
    gpioWrite( DO7, OFF );
}

/*==================[external functions definition]==========================*/

void printTickCount(int length) {
    char buff[100]; 
    char buff2[100];
    
    strcpy(buff, intToString(tickRead()));
    while (strlen(buff) < length) {
        strcpy(buff2, buff);
        strcpy(buff, "0");
        strcat(buff, buff2);
    }
    strcat(buff, "ms ");
    uartWriteString( UART_USB,  buff);
}

void log(char *str) {
    printTickCount(8);
    uartWriteString( UART_USB, str );
    uartWriteString( UART_USB, "\r\n" );
}

void literal(char *str) {
    uartWriteString( UART_USB, str );
}

void logError(char *str) {
    printTickCount(8);
    uartWriteString( UART_USB, "\033[31mError: " );
    uartWriteString( UART_USB, str );
    uartWriteString( UART_USB, "\033[0m" );
    uartWriteString( UART_USB, "\r\n" );
}

void println(char *str) {
    uartWriteString( UART_USB, str );
    uartWriteString( UART_USB, "\r\n" );
}

void goodbye() {
    log("System Halt");
}

// Wrappers para manejar dispositivos USB Mass Storage y tarjeta SD
sdcard_t sdcard;

// Se guarda el ultimo estado para enviar un mensaje solo cuando el estado 
// cambie.
sdcardStatus_t sdcardUltimoEstado = -1;


void diskTickHook ( void *ptr );

static void mostrarEstadoTarjetaSD( void )
{
    // El estado actual es distinto al ultimo mostrado?
    if ( sdcardUltimoEstado == sdcardStatus() )
    {
        // Es igual, no hay nada que hacer aca
        return;
    }
    
    // Es diferente, guardo y envio estado
    sdcardUltimoEstado = sdcardStatus();
    
    switch( sdcardUltimoEstado )
    {
        case SDCARD_Status_Removed:
            logError("STATUS: Por favor inserte Tarjeta SD.");
            break;
        
        case SDCARD_Status_Inserted:
            log("STATUS: Tarjeta SD insertada.");
            break;
    
        case SDCARD_Status_NativeMode:
            log("STATUS: Configurando tarjeta SD.");
            break;
            
        case SDCARD_Status_Initializing:
            log("STATUS: Iniciando tarjeta SD.");
            break;
        
        case SDCARD_Status_ReadyUnmounted:
            log("STATUS: Tarjeta SD lista pero desmontada." );
            break;

        case SDCARD_Status_ReadyMounted:
            log("STATUS: Tarjeta SD lista y montada." );
            break;
        
        case SDCARD_Status_Error:
            log("STATUS: Tarjeta SD en estado de Error." );
            break;
    }
}

static void fatFsTestStart( const char* test )
{
    char msg[256];
    sprintf( msg, "TEST: Ejecutando '%s'...", test );
    log(msg );
}


static void fatFsTestOK( void )
{
    log("OK!\r\n" );
}


static void fatFsTestERROR( int error )
{
    char msg[256];
    sprintf( msg, "ERROR %i", error );
    logError(msg );
}


static bool fatFsTest()
{
    char buf[1024];
    char filename[64];    
    FIL file;
    FRESULT fr;
    int r;
    
    const char *unidad  = "SSD";
    sprintf( filename, "%s/TEST.TXT", unidad );

    log("\r\n-------------------------------------------" );   
    sprintf( buf, "TEST sobre archivo '%s'.", filename);
    log(buf);
    log("-------------------------------------------" ); 
    
    // Ver http://elm-chan.org/fsw/ff/00index_e.html para una referencia de la
    // API de FatFs
    
    // Abre un archivo. Si no existe lo crea, si existe, lo sobreescribe.
    fatFsTestStart( "f_open( WRITE )" );
    fr = f_open( &file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if( fr != FR_OK )
    {
        fatFsTestERROR( fr );
        return false;
    }
    fatFsTestOK( );
 
    // Prueba de f_putc
    fatFsTestStart( "f_putc" );
    sprintf (buf, "La unidad bajo prueba es '%s'"
                  "Lista de caracteres ASCII:",
                  unidad);
    // Escribe mensaje
    for (uint32_t i = 0; i < strlen(buf); ++i)
    {
        r = f_putc( buf[i], &file );
        if (r < 1)
        {
            fatFsTestERROR( r );
            f_close( &file );
            return false;
        }
    }
    
    // Escribe todos los caracteres UTF-8 que overlapean ASCII
    // (sin combinaciones multibyte)    
    for (uint32_t i = 32; i < 127; ++i)
    {
        r = f_putc( (TCHAR)i, &file );
        if (r < 1)
        {
            fatFsTestERROR( r );
            f_close( &file );
            return false;
        }
    }
    fatFsTestOK( );
    
    // Prueba f_puts
    fatFsTestStart( "f_puts");  
    sprintf (buf, "\r\n"
                  "Fecha y hora de compilacion del programa: %s %s\r\n"
                  "Estado de pulsadores TEC 1 a 4 al momento de la prueba: %i%i%i%i\r\n",
                  __DATE__, __TIME__,
                  gpioRead( TEC1 ), gpioRead( TEC2 ), gpioRead( TEC3 ), gpioRead( TEC4 ));
    
    r = f_puts( buf, &file );
    if (r < 1)
    {
       fatFsTestERROR( r );
       f_close( &file );
       return false;
    }
    fatFsTestOK( );
    
    // Cierra el archivo y vuelve a abrirlo como LECTURA
    f_close( &file );
    
    fatFsTestStart( "f_open( READ )" );
    fr = f_open( &file, filename, FA_READ );
    if( fr != FR_OK )
    {
       fatFsTestERROR( fr );
       return false;
    }
    fatFsTestOK( );
    
    // Borro contenido del buffer, para que no haya dudas de que el contenido
    // se leyo desde el archivo
    memset( buf, 0, sizeof(buf) );
    
    // Carga el contenido del archivo
    UINT bytesLeidos = 0;
     fatFsTestStart( "f_read" );
    fr = f_read( &file, buf, sizeof(buf), &bytesLeidos );
    if (fr != FR_OK)
    {
       fatFsTestERROR( fr );
       return false;
    }
    fatFsTestOK( );
    
    f_close( &file );

    log("\r\n");        
    log(">>>> INICIO CONTENIDO DEL ARCHIVO LEIDO >>>>\r\n");    
    log(buf );
    log("<<<< FIN CONTENIDO DEL ARCHIVO LEIDO <<<<\r\n");    
    return true;   
}

// FUNCION que se ejecuta cada vezque ocurre un Tick
void diskTickHook( void *ptr )
{
    disk_timerproc (); 
}
// FUNCION PRINCIPAL, PUNTO DE ENTRADA AL PROGRAMA LUEGO DE ENCENDIDO O RESET.
int main( void )
{
    char buff[1024];
    int result;
    int i;
    char k;
    char *p;
    bool_t wait_key;
    uint8_t count;
    uint8_t v;
    // ---------- CONFIGURACIONES ------------------------------

    // Inicializar y configurar la plataforma
    boardConfig();
    spiConfig( SPI0 );
    
    // Inicializar el conteo de Ticks con resolucion de 10ms,
    // con tickHook diskTickHook
    tickConfig( 10 );
    tickCallbackSet( diskTickHook, NULL );

    /* Inicializar Uart */
    uartConfig( UART_USB, BAUD_RATE );
    literal("\e[2J");  // Escape command: Clear screen
    println("");
    println("");
    println("");
    println("New boot...");
    println("");
    
    log("I2C bus start");
    i2cInit(I2C0, 100000);

    // Set the LED to the state of "Off"
    // Board_Dout_Set(0, false);

    /* Inicializar el conteo de Ticks con resoluci�n de 1ms, sin tickHook */
    //tickConfig( 1, 0 );

    /* Inicializar GPIOs */
    gpioConfig( 0, GPIO_ENABLE );

    /* Configuraci�n de pines de salida para Leds de la CIAA-NXP */
    gpioConfig( DO0, GPIO_OUTPUT );
    gpioConfig( DO1, GPIO_OUTPUT );
    gpioConfig( DO2, GPIO_OUTPUT );
    gpioConfig( DO3, GPIO_OUTPUT );
    gpioConfig( DO4, GPIO_OUTPUT );
    gpioConfig( DO5, GPIO_OUTPUT );
    gpioConfig( DO6, GPIO_OUTPUT );
    gpioConfig( DO7, GPIO_OUTPUT );    
    
    reset_all_douts();

    log("Iniciando sdcard con configuracion:" );
    strcpy(buff, "  * Starting speed ");
    strcat(buff, intToString(FSSDC_GetSlowClock()));
    strcat(buff, " Hz");
    log(buff);    
    strcpy(buff, "  * Working speed ");
    strcat(buff, intToString(FSSDC_GetFastClock()));
    strcat(buff, " Hz");
    log(buff);    
    if (sdcardInit( &sdcard ) == false) {
        logError("SDCard init: FAIL.");        
    } else {
        log("SDCard init: success" );
    }

    fatFsTest();

   /* Inicializar ADC128D818 */
    result = ADC128D818_init(ADC128D818_ADDRESS_LOW_LOW, 
                   ADC128D818_OPERATION_MODE_1, 
                   ADC128D818_RATE_ONE_SHOT, 
                   ADC128D818_VREF_INT, 
                   0,
                   0);


    if (!result) {
        logError("No device ADC128D818 or device busy");
    }

    /* Inicializar ADC128D818 */
    result = ADC128D818_init(ADC128D818_ADDRESS_HIGH_HIGH, 
                   ADC128D818_OPERATION_MODE_1, 
                   ADC128D818_RATE_ONE_SHOT, 
                   ADC128D818_VREF_INT, 
                   0,
                   0);


    if (!result) {
        logError("No device ADC128D818 or device busy");
    }

    log("");
    log("");
    count = 0;
    

    // ---------- REPETIR POR SIEMPRE --------------------------
    while( TRUE ) {
      
        for (i=0; i<8; i++) {
            strcpy(buff, "Chip U15.  AN#");
            p=intToString(i);
            strcat(buff, p);
            strcat(buff, ": ");
            int16_t v = ADC128D818_readChannel(ADC128D818_ADDRESS_HIGH_HIGH, i);
            p=intToString(v);
            strcat(buff, p);
            strcat(buff, "                              ");
            log(buff);
        }

        for (i=0; i<8; i++) {
            if (i<2)
              strcpy(buff, "Chip U13.  AN#");
            else
              strcpy(buff, "Chip U13. AN#");
            p=intToString(i+8);
            strcat(buff, p);
            strcat(buff, ": ");
            p=intToString(ADC128D818_readChannel(ADC128D818_ADDRESS_LOW_LOW, i));
            strcat(buff, p);
            strcat(buff, "     ");
            log(buff);
        }
        log("=============================================");
        log(" Start/Stop: press space");

        while (uartRxReady( UART_USB )) {
            k = uartRxRead( UART_USB );
            wait_key = TRUE;
            while (wait_key) {
                if (uartRxReady( UART_USB )) {
                    char k = uartRxRead( UART_USB );
                    if (k == 32 ) wait_key = FALSE;
                }
            }
        }
        delay(1500);
        literal("\e[100D");   // Escape command: go left 100 (line start)
        literal("\e[18A");    // Escape command: go up 18
        reset_all_douts();
        switch (count) {
            case 0: gpioWrite( DO4, ON ); break;
            case 1: gpioWrite( DO5, ON ); break;
            case 2: gpioWrite( DO6, ON ); break;
            case 3: gpioWrite( DO7, ON ); break;
        }
        count++;
        if (count>=4) count = 0;
    }

    // NO DEBE LLEGAR NUNCA AQUI, debido a que a este programa se ejecuta
    // directamenteno sobre un microcontroladore y no es llamado por ningun
    // Sistema Operativo, como en el caso de un programa para PC.
    return 0;
}
