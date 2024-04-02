/**
 * main.c
 *
 * ECE 3849 Lab 0 Starter Project
 * Gene Bogdanov    10/18/2017
 *
 * This version is using the new hardware for B2017: the EK-TM4C1294XL LaunchPad with BOOSTXL-EDUMKII BoosterPack.
 *
 */

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
#define PWM_FREQUENCY 20000 // PWM frequency = 20 kHz

uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 12345; // time in hundredths of a second

tContext sContext;


int main(void)

    {
    IntMasterDisable();

    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    // configure M0PWM2, at GPIO PF2, BoosterPack 1 header C1 pin 2
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_2);
    GPIOPinConfigure(GPIO_PF2_M0PWM2);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_2,
    GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
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


    char button;


    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation

    uint16_t sampbuff[128];
    int i;
    int trigger;
    int voltsPerDiv = 4;
    float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1, 2};
    float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[voltsPerDiv]);
    int tSlope = 1;
//    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font
    ButtonInit();
    ADCInit();
    IntMasterEnable();
    uint32_t time;  // local copy of gTime
    char str[50];   // string buffer
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};
    str[0] = 'trig missing';
    const char * const gVoltageScaleStr[] = {
    "100 mV", "200 mV", "500 mV", " 1 V", " 2 V"
    };


    while (true) {

        while (fifo_get(&button)){
            switch(button){
                case 'w':
                    voltsPerDiv = ++voltsPerDiv > 4 ? 4 : voltsPerDiv++;
                    break;
                case 't':
                    voltsPerDiv = --voltsPerDiv <= 0 ? 0 : voltsPerDiv--;
                    break;
                case 'f':
                    tSlope = !tSlope;
                    break;
            }
        }


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

        trigger = tSlope ? RisingTrigger(): FallingTrigger();

        //TODO: IMPLEMENT SCALING

        for (i = 0; i < LCD_HORIZONTAL_MAX - 1; i++){
//            sample[i] = gADCBuffer[ADC_BUFFER_WRAP(trigger - LCD_HORIZONTAL/2+i)];

        }
        GrContextForegroundSet(&sContext, ClrWhite);
        GrStringDraw(&sContext, gVoltageScaleStr[voltsPerDiv], -1, 50, 0, false);

        if (!TrigFound){
           for(i = 0; i<13; i++){
               GrStringDraw(&sContext, str[i], -1, 1, 120, false);
           }
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
        if (gButtons == 8){
            for(i = 0; i<129; i++){
                sampbuff[i] = gADCBuffer[i];
            }

        }
        int y;
        int nexty;
        for (i = 0; i<128; i++){
            y = (sampbuff[i]/36);
            nexty = (sampbuff[i+1]/36);
            GrLineDraw(&sContext, i, y, i+1, nexty);
        }
        GrFlush(&sContext); // flush the frame buffer to the LCD


    }
}
