

// Assumptions:
// - Plant output Y connected to PA1 (ADC input).
// - Setpoint R is fixed (e.g., 2048 = half of 12-bit ADC range) - you can tune via UART later.
// - PWM output on PA8 (TIM1 CH1) to drive the plant (e.g., via RC filter for analog signal).
// - Control loop at 1kHz (Ts = 0.001s) using TIM2 interrupt.
// - Rate feedback: Digital derivative of Y (velocity = (y_k - y_{k-1}) / Ts).
// - PI params: Start with Kp=7.5, Ti=0.156s, Td=0.76s (from earlier design) - but scaled for digital.
// - UART2 (PA2 TX) for debug prints (simple polling).
// - Clock: HSI 16MHz, no PLL for simplicity (system clock = 16MHz).
// - ADC: Single channel (CH1=PA1), software trigger for simplicity.
// - PWM: TIM1 at 10kHz, 16-bit resolution.
// Compile with: arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O0 -g -o main.elf main.c -T STM32F411RETx_FLASH.ld
// (You'll need a linker script and startup code - assume you have them).

#include <stdint.h>  //

// Define STM32 registers (base addresses from reference manual).
#define RCC_BASE    0x40023800  // RCC peripheral base.
#define GPIOA_BASE  0x40020000  // GPIOA base.
#define ADC1_BASE   0x40012000  // ADC1 base.
#define TIM1_BASE   0x40010000  // TIM1 base (for PWM).
#define TIM2_BASE   0x40000000  // TIM2 base (for control loop timer).
#define USART2_BASE 0x40004400  // USART2 base (for debug).

// RCC registers (offsets from base).
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00))  // Clock control.
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x08))  // Clock config.
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30))  // AHB1 enable.
#define RCC_APB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x40))  // APB1 enable (TIM2).
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x44))  // APB2 enable (ADC, TIM1, USART2).

// GPIOA registers.
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00))  // Mode register.
#define GPIOA_AFRL  (*(volatile uint32_t *)(GPIOA_BASE + 0x20))  // Alternate function low.

// ADC1 registers.
#define ADC1_SR     (*(volatile uint32_t *)(ADC1_BASE + 0x00))  // Status.
#define ADC1_CR1    (*(volatile uint32_t *)(ADC1_BASE + 0x04))  // Control 1.
#define ADC1_CR2    (*(volatile uint32_t *)(ADC1_BASE + 0x08))  // Control 2.
#define ADC1_SMPR2  (*(volatile uint32_t *)(ADC1_BASE + 0x10))  // Sample time 2.
#define ADC1_SQR3   (*(volatile uint32_t *)(ADC1_BASE + 0x30))  // Regular sequence 3.
#define ADC1_DR     (*(volatile uint32_t *)(ADC1_BASE + 0x4C))  // Data register.

// TIM1 registers (for PWM).
#define TIM1_CR1    (*(volatile uint32_t *)(TIM1_BASE + 0x00))  // Control 1.
#define TIM1_SMCR   (*(volatile uint32_t *)(TIM1_BASE + 0x08))  // Slave mode.
#define TIM1_CCMR1  (*(volatile uint32_t *)(TIM1_BASE + 0x18))  // Capture/compare mode 1.
#define TIM1_CCER   (*(volatile uint32_t *)(TIM1_BASE + 0x20))  // Capture/compare enable.
#define TIM1_PSC    (*(volatile uint32_t *)(TIM1_BASE + 0x28))  // Prescaler.
#define TIM1_ARR    (*(volatile uint32_t *)(TIM1_BASE + 0x2C))  // Auto-reload.
#define TIM1_CCR1   (*(volatile uint32_t *)(TIM1_BASE + 0x34))  // Capture/compare 1 (duty).
#define TIM1_BDTR   (*(volatile uint32_t *)(TIM1_BASE + 0x44))  // Break and dead-time.

// TIM2 registers (for control loop).
#define TIM2_CR1    (*(volatile uint32_t *)(TIM2_BASE + 0x00))  // Control 1.
#define TIM2_DIER   (*(volatile uint32_t *)(TIM2_BASE + 0x0C))  // DMA/Interrupt enable.
#define TIM2_SR     (*(volatile uint32_t *)(TIM2_BASE + 0x10))  // Status.
#define TIM2_PSC    (*(volatile uint32_t *)(TIM2_BASE + 0x28))  // Prescaler.
#define TIM2_ARR    (*(volatile uint32_t *)(TIM2_BASE + 0x2C))  // Auto-reload.

// USART2 registers.
#define USART2_SR   (*(volatile uint32_t *)(USART2_BASE + 0x00))  // Status.
#define USART2_DR   (*(volatile uint32_t *)(USART2_BASE + 0x04))  // Data.
#define USART2_BRR  (*(volatile uint32_t *)(USART2_BASE + 0x08))  // Baud rate.
#define USART2_CR1  (*(volatile uint32_t *)(USART2_BASE + 0x0C))  // Control 1.

// NVIC (Interrupt Vector) - for TIM2 IRQ.
#define NVIC_ISER0  (*(volatile uint32_t *)0xE000E100)  // Interrupt set-enable 0.
#define TIM2_IRQ    28  // TIM2 global interrupt position.

// PI Controller parameters (from earlier design, but scaled for digital - floats for precision).
float Kp = 7.5f;         // Proportional gain.
float Ti = 0.156f;       // Integral time (seconds).
float Td = 0.76f;        // Derivative time (seconds) for rate feedback.
float Ts = 0.001f;       // Sampling time (1ms = 1kHz control loop).
float integral = 0.0f;   // Integrator state.
uint16_t y_prev = 0;     // Previous Y for rate feedback derivative.
uint16_t setpoint = 2048; // Fixed reference R (half of 4095 max ADC) - tune via UART.

// Function prototypes.
void SystemClock_Config(void);  // Clock setup.
void GPIO_Init(void);           // GPIO setup.
void ADC_Init(void);            // ADC setup.
void TIM2_Control_Init(void);   // Control timer setup.
void TIM1_PWM_Init(void);       // PWM timer setup.
void USART2_Init(void);         // UART for debug.
void USART2_SendChar(char c);   // Simple polling send.
void USART2_SendString(const char* str); // Send string.
uint16_t ADC_Read(void);        // Read ADC value.

// Main function - entry point.
int main(void) {
    // Initialize clock - first thing, as everything depends on it.
    SystemClock_Config();

    // Initialize GPIO - set pin modes.
    GPIO_Init();

    // Initialize ADC - for reading plant output Y.
    ADC_Init();

    // Initialize UART - for prints and tuning.
    USART2_Init();
    USART2_SendString("PI Controller Started\r\n");

    // Initialize PWM timer - for output to plant.
    TIM1_PWM_Init();

    // Initialize control timer - starts the loop via interrupt.
    TIM2_Control_Init();

    // Main loop (outside ISR) - can add UART tuning here.
    while (1) {
        // Example: Print status every second or so (pseudo-delay).
        // For real, use another timer or counter.
        // Here, just idle - tuning can be added via UART RX if you expand.
    }
}

// Clock configuration - use HSI 16MHz for simplicity.
void SystemClock_Config(void) {
    // Enable HSI oscillator.
    RCC_CR |= (1 << 0);  // HSION = 1.

    // Wait for HSI ready.
    while (!(RCC_CR & (1 << 1)));  // Wait HSIRDY.

    // Select HSI as system clock.
    RCC_CFGR &= ~(3 << 0);  // SW = 00 (HSI).

    // Enable clocks for peripherals (as per your spec: AHB1 GPIOA, APB2 ADC/TIM1/USART2, APB1 TIM2).
    RCC_AHB1ENR |= (1 << 0);  // GPIOAEN = 1.
    RCC_APB2ENR |= (1 << 8) | (1 << 0) | (1 << 17);  // ADC1EN=1, TIM1EN=1, USART2EN=1.
    RCC_APB1ENR |= (1 << 0);  // TIM2EN=1.
}

// GPIO initialization - PA1 analog for ADC, PA8 AF for TIM1 PWM, PA2 AF for USART2 TX.
void GPIO_Init(void) {
    // PA1: Analog mode for ADC (MODER bits 3:2 = 11).
    GPIOA_MODER |= (3 << 2);  // Analog.

    // PA8: Alternate function mode for TIM1 (MODER bits 17:16 = 10).
    GPIOA_MODER |= (2 << 16);  // AF mode.

    // Set AF for PA8: TIM1_CH1 is AF1 (AFRL bits 3:0 for pin8? Wait, AF high for pin8: AFRH offset 0x24).
    // Actually, AFRH (0x24) for pins 8-15.
#define GPIOA_AFRH  (*(volatile uint32_t *)(GPIOA_BASE + 0x24))
    GPIOA_AFRH &= ~(0xF << 0);  // Clear AF for PA8.
    GPIOA_AFRH |= (1 << 0);     // AF1 for TIM1.

    // PA2: AF for USART2 TX (MODER 5:4=10, AFRL bits 11:8=7 for AF7).
    GPIOA_MODER |= (2 << 4);    // AF mode.
    GPIOA_AFRL &= ~(0xF << 8);  // Clear.
    GPIOA_AFRL |= (7 << 8);     // AF7 for USART2.
}

// ADC initialization - Channel 1 (PA1), 12-bit, software trigger.
void ADC_Init(void) {
    // Set sample time for CH1: 480 cycles for accuracy (SMPR2 bits 5:3=111).
    ADC1_SMPR2 |= (7 << 3);  // Max sample time.

    // Regular sequence: CH1 first (SQR3 bits 4:0=1).
    ADC1_SQR3 |= (1 << 0);

    // CR1: 12-bit resolution (RES=00).
    ADC1_CR1 &= ~(3 << 24);  // RES=00.

    // CR2: Enable ADC (ADON=1), continuous? No, single for now.
    ADC1_CR2 |= (1 << 0);  // ADON=1.

    // Wait for ADC ready (not strictly needed, but good).
    for (volatile int i=0; i<1000; i++);
}

// Read ADC value - start conversion, wait, read.
uint16_t ADC_Read(void) {
    // Start conversion (SWSTART=1).
    ADC1_CR2 |= (1 << 30);

    // Wait for EOC (end of conversion).
    while (!(ADC1_SR & (1 << 1)));

    // Read data.
    return (uint16_t)ADC1_DR;
}

// Control timer init - TIM2 at 1kHz interrupt (Ts=1ms).
void TIM2_Control_Init(void) {
    // Prescaler: For 16MHz clock, PSC=15 (0-based: divides by 16) -> 1MHz counter clock.
    TIM2_PSC = 15;  // 16MHz / 16 = 1MHz.

    // Auto-reload: 1000 counts for 1ms (1MHz / 1000 = 1kHz).
    TIM2_ARR = 999;  // 0-999 = 1000 ticks.

    // Enable update interrupt.
    TIM2_DIER |= (1 << 0);  // UIE=1.

    // Enable NVIC for TIM2.
    NVIC_ISER0 |= (1 << TIM2_IRQ);  // Enable IRQ28.

    // Start timer (CEN=1).
    TIM2_CR1 |= (1 << 0);
}

// PWM timer init - TIM1 CH1 on PA8, 10kHz, initial duty 50%.
void TIM1_PWM_Init(void) {
    // Prescaler: For 16MHz APB2, PSC=1 (div by 2) -> 8MHz counter (wait, APB2 is 16MHz).
    // Actually, TIM1 clock is 16MHz (APB2 prescaler=1 for timers).
    TIM1_PSC = 15;  // 16MHz / 16 = 1MHz.

    // Auto-reload: For 10kHz PWM, ARR=99 (1MHz / 100 = 10kHz).
    TIM1_ARR = 99;

    // CCR1 initial: 50% duty (50).
    TIM1_CCR1 = 50;

    // CCMR1: PWM mode 1 (OC1M=110), preload enable.
    TIM1_CCMR1 |= (6 << 4) | (1 << 3);  // OC1M=110, OC1PE=1.

    // CCER: Enable CH1 output.
    TIM1_CCER |= (1 << 0);  // CC1E=1.

    // BDTR: Main output enable (for advanced TIM1).
    TIM1_BDTR |= (1 << 15);  // MOE=1.

    // Start timer.
    TIM1_CR1 |= (1 << 0);  // CEN=1.
}

// UART init - USART2, 9600 baud, TX only.
void USART2_Init(void) {
    // Baud rate: For 16MHz PCLK, 9600 = 16e6 / (16 * BRR mantissa).
    // Approx: 16e6 / 9600 ≈ 1666.67, so BRR=104 * 16 + 11 (0x068B? Wait calc: 16e6/9600=1666.666, BRR=1667=0x683).
    USART2_BRR = 0x683;  // For 16MHz / 9600.

    // CR1: TE=1 (transmit enable), UE=1 (USART enable).
    USART2_CR1 |= (1 << 3) | (1 << 13);  // TE=1, UE=1.
}

// Send single char via polling.
void USART2_SendChar(char c) {
    // Wait TXE=1.
    while (!(USART2_SR & (1 << 7)));

    // Send data.
    USART2_DR = c;
}

// Send string.
void USART2_SendString(const char* str) {
    while (*str) {
        USART2_SendChar(*str++);
    }
}

// TIM2 IRQ Handler - the control loop.
void TIM2_IRQHandler(void) {
    // Clear update flag.
    TIM2_SR &= ~(1 << 0);  // UIF=0.

    // Read ADC (Y from plant).
    uint16_t y = ADC_Read();

    // Compute error: e = setpoint - y (your R - Y).
    float error = (float)(setpoint - y);

    // Normalize? Optional: error /= 4095.0f; but skip for now (keep in counts).

    // Compute integral (simple Euler: integral += error * Ts / Ti).
    integral += (error * Ts) / Ti;

    // PI output: proportional + integral.
    float pi_out = Kp * (error + integral);

    // Rate feedback: velocity ≈ (y - y_prev) / Ts (note: positive y increase means positive velocity).
    // But in feedback, subtract Td * velocity (negative feedback).
    float velocity = (float)(y - y_prev) / Ts;
    y_prev = y;  // Update previous.

    float u = pi_out - (Td * velocity);

    // Clamp output: Say to -4095..4095, but for PWM duty 0-100%.
    // Assume u positive, scale to duty: duty = (u / some_max) * ARR.
    // Simple: Clamp u to 0-99 (for ARR=99).
    if (u < 0) u = 0;
    if (u > 99) u = 99;

    // Update PWM duty.
    TIM1_CCR1 = (uint16_t)u;

    // Optional: Print via UART? But avoid in ISR - use flag or buffer.
}

// Notes:
// - To tune params: In main loop, add UART RX to change Kp/Ti/Td/setpoint.
// - Anti-windup: Add if (u saturated) don't integrate.
// - Scaling: ADC is 0-4095 (3.3V), PWM duty 0-ARR (100%).
// - For real plant, adjust gains - this is starting point.
// - Interrupt priority? Default is fine.
// - Add startup_stm32f411xx.s and linker for full build.
