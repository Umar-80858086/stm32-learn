

#include <stdint.h>
//AHB1ENR	  0x40023830
//APB1ENR  	  0x40023840
//GPIOA_MDDER 0x40020000
//GPIOA_AFRL  0x40020020
//USART2_SR 0x40004400
//USART2_BRR 0x40004408
//USART2_DR 0x40004404
//USART2_CR1 0x4000440c

uint32_t *pAHB1ENR     =  (uint32_t*)0x40023830;// enable ahb1
uint32_t *pAPB1ENR     =  (uint32_t*)0x40023840;// enable apb1
uint32_t *pGPIOA_MODER =  (uint32_t*)0x40020000;// output mode
uint32_t *pGPIOA_AFRL  =  (uint32_t*)0x40020020;// alternate function
uint32_t *USART2_SR    =  (uint32_t*)0x40004400;// uart status reg
uint32_t *USART2_BRR   =  (uint32_t*)0x40004408;// baud rate reg
uint32_t *USART2_DR    =  (uint32_t*)0x40004404;// data reg
uint32_t *USART2_CR1   =  (uint32_t*)0x4000440c;// control reg




void uSART_init(void){
	// enable the clock for GPIOa
	*pAHB1ENR |= (1U << 0);
	// enable th e clock for UASART 2
	*pAPB1ENR |= (1U<<17);
	// configure GPIO a to ALT function
	*pGPIOA_MODER |= 0x20;
	// configure PA2 TO USART2
	*pGPIOA_AFRL |= 0x700;
	/*/CONFIGURE PA2 TO USART2
		*pGPIOAFRL |=(1U<<8);
		*pGPIOAFRL |=(1U<<9);
		*pGPIOAFRL |=(1U<<10);
		*pGPIOAFRL &=~(1U<<11);*/
	// configure the BRR to 9600
	*USART2_BRR = 0x0683;
	// configure UART
	*USART2_CR1 |=0x2008;  // for enabling TX and UART
	//*USART2_CR1 |=0x2000;

}
void Uart_Write(int ch)
{
	while(!(*USART2_SR & 0x0080)){}
	*USART2_DR =(ch&0xff);


}

int __io_putchar(int ch)
{
	Uart_Write(ch);
	return ch;
}


int main(void)
{

	Uart_Init();
	//Uart_Write();
    /* Loop forever */
	while(1)
	{
		printf("Hello world \r \n");
	}
	for(;;);
}
