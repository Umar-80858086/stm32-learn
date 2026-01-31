

#include <stdint.h>
#include <stdio.h>

uint32_t *pAHB1ENR =(uint32_t*)0x40023830; // Declare AHB1 to rnable GPIO a 
uint32_t *pAPB2ENR =(uint32_t*)0x40023844; // Declare Apb2 for ADC enable 
uint32_t *pGIPOMODER =(uint32_t*)0x40020000; // declare GPIO mode to configure GPIO pin to analog 
uint32_t *pADC_SR =(uint32_t*)0x40012000; // Declate statur reg of ADC to monitor convertion flags 
uint32_t *pADC_CR2 =(uint32_t*)0x40012008; // declare pointer to the ADC control register 2 to anage ADc enable and conversation start 
uint32_t *pADC_SQR3 =(uint32_t*)0x40012034; // Declare pointer to ADC reguar seq reg 3 to select adc input channel 
uint32_t *pADC_DR =(uint32_t*)0x4001204c; // declare pounter to ADC data register to read the covernter digital ADC value 
uint16_t analogValue; //define 16 bt variable to store ADC conversion reault since STM32 ADC resolution uplto 12 bit.

void ADC_Init(); // function prototype for initializing ADC hardware and related gpio configuration 
void delay(); s//software delay
void ADC_Init() // delare function delay
{
 *pAHB1ENR |=1 ;//enable the clock access for GPIOA
 *pAPB2ENR |=0X100;//enable the clock access for ADC1 set bit 8 
 *pGIPOMODER |=0XC; //CONFIGURE GPIOA FOR ANALOG 
 *pADC_CR2 =0; // reste control reg to known state 
 *pADC_SQR3=1; // selects ADC channel 1 as first and only conversion in the regular sequence.
 *pADC_CR2 |=1; // sets the ADON bit to power on and enable the ADC module 

}

void delay()
{
	for(uint32_t i=0;i<300000;i++);
}

int main(void)
{
	printf("Display ADC Value \n");
	ADC_Init(); // calss the ADC init function to config hardware before startinh conversations

	while(1)
	{
		//start the adc conversion
		*pADC_CR2|=0x40000000; // setrs the SWSTART bti in ADc_cr2 t trigger a software int ADC conversion.
		while(!(*pADC_SR & 2)){} //wait till the end of conversion {Polls the ADC status register until the End-Of-Conversion (EOC) flag is set.}
		analogValue =*pADC_DR; //READs the ADC ocnvertion results from the data register and store in in 16 bit variable 
		printf("Display ADC Value  %d\n",analogValue); // Outputs the converted ADC digital value for debugging or monitoring purposes.

		delay();
	}




	for(;;); //Creates a redundant infinite loop to prevent program exit, typical in embedded systems.
}
