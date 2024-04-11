/*
 * ECE 3849 Lab2 starter project
 *
 * Gene Bogdanov    9/13/2017
 */
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

#include <stdint.h>
#include <stdbool.h>
#include "driverlib/interrupt.h"

uint32_t gSystemClock = 120000000; // [Hz] system clock frequency

#include <stdint.h>
#include <stdbool.h>
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "buttons.h"
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "sampling.h"
#include "driverlib/timer.h"
#define PWM_FREQUENCY 20000 // PWM frequency = 20 kHz

uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 12345; // time in hundredths of a second

tContext sContext;
uint32_t  count_loaded  = 0;
uint32_t  count_unloaded = 0;
float cpu_load = 0.0;
uint32_t cpu_load_count(void);
//volatile bool waveaphore = true;
//volatile bool processaphore = false;
//volatile bool displayaphore = false;

uint32_t curry = 0;
uint16_t processbuff[LCD_VERTICAL_MAX];
uint16_t sampbuff[LCD_VERTICAL_MAX];
//this is probably a bad idea
volatile char buttmailbox;
int prevy;
int y;
int trigger;
int voltsPerDiv = 4;
float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1, 2};
int tSlope = 1;
/*
 *  ======== main ========
 */
int main(void)

{
    IntMasterDisable();

    // hardware initialization goes here
    // Enable the Floating Point Unit, and permit ISRs to use it
       FPUEnable();
       FPULazyStackingEnable();

       // Initialize the system clock to 120 MHz
       gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

       // configure M0PWM2, at GPIO PF2, BoosterPack 1 header C1 pin 2
       SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
       GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
       GPIOPinConfigure(GPIO_PF2_M0PWM2);
       GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
       // configure the PWM0 peripheral, gen 1, outputs 2 and 3
       SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
       // use system clock without division
       PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_1);
       PWMGenConfigure(PWM0_BASE, PWM_GEN_1,
       PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
       PWMGenPeriodSet(PWM0_BASE, PWM_GEN_1,
       roundf((float)gSystemClock/PWM_FREQUENCY));
       PWMPulseWidthSet(PWM0_BASE, PWM_OUT_2,
       roundf((float)gSystemClock/PWM_FREQUENCY*0.4f));
       PWMOutputState(PWM0_BASE, PWM_OUT_2_BIT, true);
       PWMGenEnable(PWM0_BASE, PWM_GEN_1);

       // initialize timer 3 in one-shot mode for polled timing
       SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
       TimerDisable(TIMER3_BASE, TIMER_BOTH);
       TimerConfigure(TIMER3_BASE, TIMER_CFG_ONE_SHOT);
       TimerLoadSet(TIMER3_BASE, TIMER_A, (gSystemClock - 1)/100); // 10 msec interval

       count_unloaded = cpu_load_count();



    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation


    ButtonInit();

//    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
//    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font
    // code for keeping track of CPU load




        /* Start BIOS */
        BIOS_start();


    return (0);
}

uint32_t cpu_load_count(void)
{
    uint32_t i = 0;
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer
    while (!(TimerIntStatus(TIMER3_BASE, false) & TIMER_TIMA_TIMEOUT))
        i++;
    return i;
}

void task0_func(UArg arg1, UArg arg2)
{
    IntMasterEnable();

//    while (true) {
//        // do nothing
//    }
}

void waveformtask(UArg arg1, UArg arg2){
    IntMasterEnable();
    while(1){
    Semaphore_pend(waveaphore, BIOS_WAIT_FOREVER);
        trigger = tSlope ? RisingTrigger(): FallingTrigger();
        int i;
        for(i = 0; i<LCD_VERTICAL_MAX; i++){
            sampbuff[i] = gADCBuffer[ADC_BUFFER_WRAP(trigger - LCD_HORIZONTAL_MAX / 2 + i)];
        }
        Semaphore_post(processaphore);
    }
}

void processingtask(UArg arg1, UArg arg2){
    while(1){
    Semaphore_pend(processaphore, BIOS_WAIT_FOREVER);
        float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[voltsPerDiv]);
        curry = curry+1;
        processbuff[curry] = LCD_VERTICAL_MAX/2 - (int)roundf(fScale * ((int)sampbuff[curry] - ADC_OFFSET));

        Semaphore_post(displayaphore);
    }
}

void displaytask(UArg arg1, UArg arg2){
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};
    const char * const gVoltageScaleStr[] = {
    "100 mV", "200 mV", "500 mV", " 1 V", " 2 V"
    };
    char str[50];   // string buffer
    count_loaded = cpu_load_count();
    cpu_load = 1.0f - (float)count_loaded/count_unloaded; // compute CPU load
    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font

    const char * triglost[] = {"trigger not found"};
    while(1){
    Semaphore_pend(displayaphore, BIOS_WAIT_FOREVER);
        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black
        GrContextForegroundSet(&sContext, ClrBlue);
        GrLineDrawV(&sContext, 5, 0, 128);
        GrLineDrawV(&sContext, 25, 0, 128);
        GrLineDrawV(&sContext, 45, 0, 128);
        GrLineDrawV(&sContext, 64, 0, 128);
        GrLineDrawV(&sContext, 65, 0, 128);
        GrLineDrawV(&sContext, 66, 0, 128);
        GrLineDrawV(&sContext, 85, 0, 128);
        GrLineDrawV(&sContext, 105, 0, 128);
        GrLineDrawV(&sContext, 125, 0, 128);
        GrLineDrawH(&sContext, 0, 128, 5);
        GrLineDrawH(&sContext, 0, 128, 25);
        GrLineDrawH(&sContext, 0, 128, 45);
        GrLineDrawH(&sContext, 0, 128, 64);
        GrLineDrawH(&sContext, 0, 128, 65);
        GrLineDrawH(&sContext, 0, 128, 66);
        GrLineDrawH(&sContext, 0, 128, 85);
        GrLineDrawH(&sContext, 0, 128, 105);
        GrLineDrawH(&sContext, 0, 128, 125);



            GrContextForegroundSet(&sContext, ClrWhite);
            GrStringDraw(&sContext, gVoltageScaleStr[voltsPerDiv], -1, 50, 0, false);
            if (!TrigFound){
                   GrStringDraw(&sContext, triglost, -1, 1, 120, false);
            }
            if (tSlope){
                GrLineDraw(&sContext, 105, 10, 115, 10);
                GrLineDraw(&sContext, 115, 10, 115, 0);
                GrLineDraw(&sContext, 115, 0, 125, 0);
                GrLineDraw(&sContext, 112, 6, 115, 2);
                GrLineDraw(&sContext, 115, 2, 118, 6);
            }
            if (!tSlope){
                GrLineDraw(&sContext, 105, 0, 115, 0);
                GrLineDraw(&sContext, 115, 10, 115, 0);
                GrLineDraw(&sContext, 115, 10, 125, 10);
                GrLineDraw(&sContext, 112, 3, 115, 7);
                GrLineDraw(&sContext, 115, 7, 118, 3);
            }

            GrContextForegroundSet(&sContext, ClrYellow);
            GrLineDraw(&sContext, curry, processbuff[curry], curry+1, processbuff[curry+1]);

            snprintf(str, sizeof(str), "CPU load = %.1f%%", cpu_load*100);
            GrStringDraw(&sContext, str, -1, 30, 120, false);

              GrFlush(&sContext); // flush the frame buffer to the LCD
              Semaphore_post(waveaphore);
    }
}
void user_input(UArg arg1, UArg arg2){
    while(1){
    Mailbox_pend(mailbox0, &buttmailbox, BIOS_WAIT_FOREVER);
        switch(buttmailbox){
            case 'w':
                voltsPerDiv = voltsPerDiv + 1 > 4 ? 4 : voltsPerDiv + 1;
                break;
            case 't':
                voltsPerDiv = voltsPerDiv - 1 <= 0 ? 0 : voltsPerDiv - 1;
                break;
            case 'f':
                tSlope = !tSlope;
                break;
        }

    }
}



void buttclock(UArg arg1, UArg arg2){
    Semaphore_post(buttaphore);
}

void ButtonISR(void) {
//    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag
    while(1){
        Semaphore_pend(buttaphore, BIOS_WAIT_FOREVER);
            // read hardware button state
            uint32_t gpio_buttons = (~GPIOPinRead(GPIO_PORTJ_BASE, 0xff) & (GPIO_PIN_1 | GPIO_PIN_0)) // EK-TM4C1294XL buttons in positions 0 and 1
                    | ((~GPIOPinRead(GPIO_PORTH_BASE, 0xff) & (GPIO_PIN_1)) << 1) //  S1
                    | ((~GPIOPinRead(GPIO_PORTK_BASE, 0xff) & (GPIO_PIN_6)) >> 3) // S2
                    | ((~GPIOPinRead(GPIO_PORTD_BASE, 0xff) & (GPIO_PIN_4))); //joystick

            uint32_t old_buttons = gButtons;    // save previous button state
            ButtonDebounce(gpio_buttons);       // Run the button debouncer. The result is in gButtons.
            ButtonReadJoystick();               // Convert joystick state to button presses. The result is in gButtons.
            uint32_t presses = ~old_buttons & gButtons;   // detect button presses (transitions from not pressed to pressed)
            presses |= ButtonAutoRepeat();      // autorepeat presses if a button is held long enough


               if (presses & 1) { // EK-TM4C1294XL button 1 pressed
                   buttmailbox = 'w';
                   Mailbox_post(mailbox0, &buttmailbox, BIOS_WAIT_FOREVER);
               }

               if (presses & 2) { // EK-TM4C1294XL button 2 pressed
                   buttmailbox = 't';
                   Mailbox_post(mailbox0, &buttmailbox, BIOS_WAIT_FOREVER);
               }

               if (presses & 8){  // EK-TM4C1294XL button pressed
                   buttmailbox = 'f';
                   Mailbox_post(mailbox0, &buttmailbox, BIOS_WAIT_FOREVER);
               }

    }
}

