/*****************************************************************************
 *   A demo example using several of the peripherals on the base board
 *
 *   Copyright(C) 2011, EE2024
 *   All rights reserved.
 *
 ******************************************************************************/

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "oled.h"
#include "rgb.h"
#include "led7seg.h"
#include "temp.h"
#include "light.h"
#include "string.h"

static uint8_t barPos = 2;
volatile uint32_t msTicks = 0;
static uint8_t value[60];

static void moveBar(uint8_t steps, uint8_t dir)
{
    uint16_t ledOn = 0;

    if (barPos == 0)
        ledOn = (1 << 0) | (3 << 14);
    else if (barPos == 1)
        ledOn = (3 << 0) | (1 << 15);
    else
        ledOn = 0x07 << (barPos-2);

    barPos += (dir*steps);
    barPos = (barPos % 16);

    pca9532_setLeds(ledOn, 0xffff);
}


static void drawOled(uint8_t joyState)
{
    static int wait = 0;
    static uint8_t currX = 48;
    static uint8_t currY = 32;
    static uint8_t lastX = 0;
    static uint8_t lastY = 0;

    if ((joyState & JOYSTICK_CENTER) != 0) {
        oled_clearScreen(OLED_COLOR_BLACK);
        return;
    }

    if (wait++ < 3)
        return;

    wait = 0;

    if ((joyState & JOYSTICK_UP) != 0 && currY > 0) {
        currY--;
    }

    if ((joyState & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
        currY++;
    }

    if ((joyState & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
        currX++;
    }

    if ((joyState & JOYSTICK_LEFT) != 0 && currX > 0) {
        currX--;
    }

    if (lastX != currX || lastY != currY) {
        oled_putPixel(currX, currY, OLED_COLOR_WHITE);
        lastX = currX;
        lastY = currY;
    }
}


#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);




static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

static void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
    	Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
    switch (ch) {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}

static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);

    }
}

static uint8_t * song = (uint8_t*)"C2.C2,D4,C4,F4,E8,";
        //(uint8_t*)"C2.C2,D4,C4,F4,E8,C2.C2,D4,C4,G4,F8,C2.C2,c4,A4,F4,E4,D4,A2.A2,H4,F4,G4,F8,";
        //"D4,B4,B4,A4,A4,G4,E4,D4.D2,E4,E4,A4,F4,D8.D4,d4,d4,c4,c4,B4,G4,E4.E2,F4,F4,A4,A4,G8,";

void SysTick_Handler(void)
{
	msTicks++;
}

uint32_t getTicks(void)
{
	return msTicks;
}


static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);
}

static void init_GPIO(void)
{
	PINSEL_CFG_Type pikachu;		//PIO1_4

		pikachu.Portnum = 1;
		pikachu.Pinnum = 31;
		// this means that the PIN that pikachu is declared with is PIN 1.31
		pikachu.Funcnum = 0;
		//declared as a number from 0 to 3. For Pin 1:31, when funcnum = 00, the function is declared as GPIO Port 1.31
		pikachu.OpenDrain = 0;
		//allows communication of different devices bi-directionally on a single pin.
		//we set it to low (0) so that the current is a sink
		pikachu.Pinmode = 0;
		//enables the pull-up resistor (Pinmode = 00).

		PINSEL_ConfigPin(&pikachu); 		//takes the initialised parameters and store them inside ConfigPin to initialise the values

		/*Done initialising SW4*/
		pikachu.Portnum = 2;
		pikachu.Pinnum = 10;
		PINSEL_ConfigPin(&pikachu);

		pikachu.Portnum = 0;
		pikachu.Pinnum = 2;
		PINSEL_ConfigPin(&pikachu);

		GPIO_SetDir(1, 1<<31, 0); // 1 << 31 is 31 bitshift
		//look for PIO1_4 - connected. This is connected to P1.31 on the board
		GPIO_SetDir(2, 1<<10, 0);
		GPIO_SetDir(0, 1<<2, 0);


		// red light
		pikachu.Funcnum = 0;
		pikachu.OpenDrain = 0;
		pikachu.Pinmode = 0;
		pikachu.Portnum = 2;
		pikachu.Pinnum = 0;
		PINSEL_ConfigPin(&pikachu);
		GPIO_SetDir(2, 1<<0, 1);

		// blue light
		pikachu.Funcnum = 0;
		pikachu.OpenDrain = 0;
		pikachu.Pinmode = 0;
		pikachu.Portnum = 0;
		pikachu.Pinnum = 26;
		PINSEL_ConfigPin(&pikachu);
		GPIO_SetDir(0, 1<<26, 1);
}






int main (void) {

    int32_t xoff = 0;
    int32_t yoff = 0;
    int32_t zoff = 0;

    int8_t x = 0;

    int8_t y = 0;
    int8_t z = 0;
    uint8_t dir = 1;
    uint8_t wait = 0;

    uint8_t state    = 0;

    uint8_t btn1 = 1;
    uint8_t btn2 = 1;

    int32_t temperature;

    uint32_t my_light;
    uint8_t my_oled_light[40];


    //declared in main.c
    init_i2c();
    init_ssp();
    init_GPIO();

    pca9532_init();				    //call pca9532.c
    joystick_init();				//call joystick.c
    acc_init();						//call acc.c
    oled_init();					//call oled.c
    led7seg_init();					//call 	led7sec.c
    //temp_init (&getMsTick);
    light_init();					//call light.c
    light_enable();					//call light.c

    rgb_init();


    SysTick_Config(SystemCoreClock/1000);	//Trigger a interrupt every 1ms using 1kHz
    /*
     * Assume base board in zero-g position when reading first value.
     */
    acc_read(&x, &y, &z);
    xoff = 0-x;
    yoff = 0-y;
    zoff = 64-z;

    /* ---- Speaker ------> */

    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn

    /* <---- Speaker ------ */

    moveBar(1, dir);
    oled_clearScreen(OLED_COLOR_BLACK);
    temp_init(&getTicks);

    while (1)
    {


        /* ####### Accelerometer and LEDs  ###### */
        /* # */

        acc_read(&x, &y, &z);
        x = x+xoff;
        y = y+yoff;
        z = z+zoff;

        if (y < 0)         	{dir = 1; y = -y;}
        else 				{dir = -1;}

        if (y > 1 && wait++ > (40 / (1 + (y/10)))) {
            moveBar(1, dir);
            wait = 0;
        }

        /* # */
        /* ############################################# */

        /* ####### Joystick and OLED  ###### */
        /* # */

        state = joystick_read();
        if (state != 0)
            drawOled(state);

        /* # */
        /* ############################################# */
        led7seg_setChar('P', FALSE);

        btn1 = (GPIO_ReadValue(1) >> 31) & 1;
        btn2 = (GPIO_ReadValue(2) >> 10) & 1;
        printf("btn2 = %u\n", btn2);
        /* ############ Trimpot and RGB LED  ########### */
        /* # */


        if (btn2 == 0)
        {
        	my_light = light_read();
        	temperature = temp_read();
        	GPIO_SetValue( 2, 1);		//turn on red led
        	GPIO_SetValue( 0, (1<<26) );	//turn on blue led
        	led7seg_setChar('5', FALSE);
        	sprintf(value, "LUX is %u", my_light);
        	oled_putString(1, 01, value,OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        	sprintf(value, "T: %.1f             ", (temperature/10.0));
        	oled_putString(1, 10, value, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        	sprintf(value, "AX: %d               ", x);
			oled_putString(1, 20, value, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			sprintf(value, "AY: %d                ", y);
			oled_putString(1, 30, value, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			sprintf(value, "AZ: %d               ", z);
			oled_putString(1, 40, value, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        	Timer0_Wait(500); //Timer will wait for 1 second (1000 miliseconds)
//            playSong(song);
        }
        else
        {
        	GPIO_ClearValue( 2, 1);			//turn off red led
        	GPIO_ClearValue( 0, (1<<26) );	//turn off blue led
        	oled_clearScreen(OLED_COLOR_BLACK);
        }



 //    	sprintf(value, "L: %u               ", light_value);
//		oled_putString(1, 01, value, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
//		sprintf(value, "T: %.1f             ", temp_value);
//		oled_putString(1, 10, value, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
     	//sprintf(temperature, "Test", (temp_read()/10));
     	//oled_putString(20, 40, (uint8_t*)temperature,OLED_COLOR_WHITE, OLED_COLOR_BLACK);
      	printf("Temperature = %d or %.2f\n", temp_read(), (float) temp_read()/10);


        Timer0_Wait(1);
    }


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}

