/*
 * sampling.c
 * created 3/21/2024 by Ricardo Croes-Ball and Will-i-am Folan
 * >:(
 */
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "sysctl_pll.h"
#include "driverlib/adc.h"
#include "inc/tm4c1294ncpdt.h"
#include "buttons.h"
#include "sampling.h"
#include "Crystalfontz128x128_ST7735.h"

// latest sample index
volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1;
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile uint32_t gADCErrors = 0; // number of missed ADC deadlines
volatile bool TrigFound= false;

void ADCInit(void){
//all the ADC1 stuff
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0); // GPIO setup for analog input AIN3
  // initialize ADC peripherals
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
  // ADC clock
    uint32_t pll_frequency = SysCtlFrequencyGet(CRYSTAL_FREQUENCY);
    uint32_t pll_divisor = (pll_frequency - 1) / (16 * ADC_SAMPLING_RATE) + 1; // round up
  ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
  ADCClockConfigSet(ADC1_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
  // choose ADC1 sequence 0; disable before configuring
  ADCSequenceDisable(ADC1_BASE, 0);

  ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger

  // in the 0th step, sample channel 3 (AIN3)
  // enable interrupt, and make it the end of sequence
  ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END); //Not sure if CH3 is correct, IF SOMETHING DOESNT WORK LOOK HERE

  // enable the sequence. it is now sampling
  ADCSequenceEnable(ADC1_BASE, 0);
  // enable sequence 0 interrupt in the ADC1 peripheral
  ADCIntEnable(ADC1_BASE, 0);
  IntPrioritySet(INT_ADC1SS0, 0x00); // set ADC1 sequence 0 interrupt priority
  // enable ADC1 sequence 0 interrupt in int. controller
  IntEnable(INT_ADC1SS0);
}


int RisingTrigger(void){ // search for rising edge trigger
    // Step 1
    TrigFound = true;
    int x = gADCBufferIndex - LCD_HORIZONTAL_MAX / 2; // half screen width; don’t use a magic number
    // Step 2
    int x_stop = x - ADC_BUFFER_SIZE / 2;
    for (; x > x_stop; x--) {
    if (gADCBuffer[ADC_BUFFER_WRAP(x)] >= ADC_OFFSET && gADCBuffer[ADC_BUFFER_WRAP(x - 1)] < ADC_OFFSET)
        break;
    }
    // Step 3
    if (x == x_stop) // for loop ran to the end
        x = gADCBufferIndex - LCD_HORIZONTAL_MAX / 2; // reset x back to how it was initialized
    TrigFound = false;
    return x;
}

int FallingTrigger(void){ // search for rising edge trigger
    // Step 1
    TrigFound = true;
    int x = gADCBufferIndex - LCD_HORIZONTAL_MAX / 2; // half screen width; don’t use a magic number
    // Step 2
    int x_stop = x - ADC_BUFFER_SIZE / 2;
    for (; x > x_stop; x--) {
    if (gADCBuffer[ADC_BUFFER_WRAP(x)] < ADC_OFFSET && gADCBuffer[ADC_BUFFER_WRAP(x - 1)] >= ADC_OFFSET)
        break;
    }
    // Step 3
    if (x == x_stop) // for loop ran to the end
        x = gADCBufferIndex - LCD_HORIZONTAL_MAX / 2; // reset x back to how it was initialized
        TrigFound = false;
    return x;
}


void ADC_ISR(void){
    // clear ADC1 sequence0 interrupt flag in the ADCISC register
    ADC1_ISC_R = ADC_ISC_IN0;
    // check for ADC FIFO overflow
    if(ADC1_OSTAT_R & ADC_OSTAT_OV0) {
        gADCErrors++; // count errors
        ADC1_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
    }
    gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1);
    // read sample from the ADC1 sequence 0 FIFO
    gADCBuffer[gADCBufferIndex] = ADC1_SSFIFO0_R;
    return gADCBuffer;
}

uint16_t ADCSAMPLER(int i){
    return gADCBuffer[i];
}

