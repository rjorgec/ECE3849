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
#include <math.h>
#include "kiss_fft.h"
#include "_kiss_fft_guts.h"
#define PI 3.14159265358979
#define NFFT 1024 // FFT length
#define KISS_FFT_CFG_SIZE (sizeof(struct kiss_fft_state)+sizeof(kiss_fft_cpx)*(NFFT-1))


#define PWM_FREQUENCY 20000 // PWM frequency = 20 kHz

uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 12345; // time in hundredths of a second

uint32_t  count_loaded  = 0;
uint32_t  count_unloaded = 0;
float cpu_load = 0.0;
uint32_t cpu_load_count(void);

bool fftmode;
uint32_t curry = 0;
uint16_t processbuff[LCD_VERTICAL_MAX];
uint16_t sampbuff[LCD_VERTICAL_MAX];
uint16_t bigbadbuff[NFFT]; //probably wrong type

char buttmailbox;
int prevy;
int y;
int trigger;
int voltsPerDiv = 4;
float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1, 2};
int tSlope = 1;
float out_db[LCD_VERTICAL_MAX];

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

    ADCInit();
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
        if(fftmode == 0){
            for(i = 0; i< NFFT; i++){
                bigbadbuff[i] = gADCBuffer[ADC_BUFFER_WRAP(trigger - LCD_HORIZONTAL_MAX / 2 + i)];
            }
        }
        else{
            for(i = 0; i<LCD_VERTICAL_MAX; i++){
                sampbuff[i] = gADCBuffer[ADC_BUFFER_WRAP(trigger - LCD_HORIZONTAL_MAX / 2 + i)];
            }
        }
        Semaphore_post(processaphore);
    }
}

void processingtask(UArg arg1, UArg arg2){
    // Kiss FFT config memory
    static char kiss_fft_cfg_buffer[KISS_FFT_CFG_SIZE];
    size_t buffer_size = KISS_FFT_CFG_SIZE;
    kiss_fft_cfg cfg; // Kiss FFT config
    // complex waveform and spectrum buffers
    static kiss_fft_cpx in[NFFT], out[NFFT];
    int i;
    int u;
    // init Kiss FFT
    cfg = kiss_fft_alloc(NFFT, 0, kiss_fft_cfg_buffer, &buffer_size);
    static float w[NFFT]; // window function
    while(1){
    Semaphore_pend(processaphore, BIOS_WAIT_FOREVER);

        for (i = 0; i < NFFT; i++) { // generate an input waveform
            in[i].r = bigbadbuff[i]; // real part of waveform CHANGED SINF AWAY
            in[i].i = 0; // imaginary part of waveform
        }
        kiss_fft(cfg, in, out); // compute FFT
        // convert first 128 bins of out[] to dB for display
        for(u = 0; u<LCD_VERTICAL_MAX; u++){
            out_db[u] = 10 * log10f(out[u].r * out[u].r + out[u].i * out[u].i);
        }

        float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[voltsPerDiv]);
//        curry = curry+1;
        if (fftmode){
            for(curry = 0; curry < LCD_VERTICAL_MAX; curry++){
//                for (i = 0; i < NFFT; i++) {
//                // Blackman window
//                w[i] = 0.42f - 0.5f * cosf(2*PI*i/(NFFT-1)) + 0.08f * cosf(4*PI*i/(NFFT-1));
//                } //laggy af
                processbuff[curry] = LCD_VERTICAL_MAX/2 - (int)roundf(fScale * ((int)sampbuff[curry] - ADC_OFFSET));
            }
        }
        else {
            for(curry = 0; curry < LCD_VERTICAL_MAX; curry++){
                processbuff[curry] = LCD_VERTICAL_MAX/2 - (int)roundf(fScale * ((int)out_db[curry] - ADC_OFFSET));
            }
        }
        Semaphore_post(displayaphore);
//        Semaphore_post(waveaphore);
    }
}

void displaytask(UArg arg1, UArg arg2){
    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};

    const char * const gVoltageScaleStr[] = {
    "100 mV", "200 mV", "500 mV", " 1 V", " 2 V"
    };
    char str[50];   // string buffer
    count_loaded = cpu_load_count();
    cpu_load = 1.0f - (float)count_loaded/count_unloaded; // compute CPU load




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
            if (fftmode == 1){
                for (curry = 0; curry< LCD_VERTICAL_MAX-1; curry++){
                    GrLineDraw(&sContext, curry, processbuff[curry], curry+1, processbuff[curry+1]);
                }
            }
            else{
                for(curry = 0; curry < LCD_VERTICAL_MAX-1; curry++){
                    GrLineDraw(&sContext, curry, out_db[curry], curry+1, out_db[curry+1]);
                }
            }
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
            case 'j':
                fftmode = !fftmode;
            case 'f':
                tSlope = !tSlope;
                break;
        }

    }
}



void buttclock(UArg arg1, UArg arg2){
    Semaphore_post(buttaphore);
}

void Buttontask(UArg arg1, UArg arg2) {
//    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // clear interrupt flag
    char buttmailbox;
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

               if (presses & 4) {
                   buttmailbox = 'j';
                   Mailbox_post(mailbox0, &buttmailbox, BIOS_WAIT_FOREVER);
               }

               if (presses & 8){  // EK-TM4C1294XL button pressed
                   buttmailbox = 'f';
                   Mailbox_post(mailbox0, &buttmailbox, BIOS_WAIT_FOREVER);
               }

    }
}

