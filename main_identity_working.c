#include <stdint.h>

#define RCC_BASE        0x40023800UL
#define GPIOA_BASE      0x40020000UL
#define GPIOB_BASE      0x40020400UL
#define SPI1_BASE       0x40013000UL

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14))

#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint32_t *)(SPI1_BASE + 0x0C))

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm volatile ("nop");
    }
}

static uint8_t spi_xfer(uint8_t b)
{
    while (!(SPI1_SR & (1U << 1))) {
    }

    *(volatile uint8_t *)&SPI1_DR = b;

    while (!(SPI1_SR & (1U << 0))) {
    }

    return *(volatile uint8_t *)&SPI1_DR;
}

static uint8_t st25_read_reg(uint8_t reg)
{
    uint8_t value;

    GPIOB_ODR &= ~(1U << 6);

    spi_xfer(0x40U | reg);
    value = spi_xfer(0x00);

    while (SPI1_SR & (1U << 7)) {
    }

    GPIOB_ODR |= (1U << 6);

    return value;
}

int main(void)
{
    volatile uint8_t identity;

    RCC_AHB1ENR |= (1U << 0) | (1U << 1);
    RCC_APB2ENR |= (1U << 12);

    GPIOB_MODER &= ~(3U << (6 * 2));
    GPIOB_MODER |=  (1U << (6 * 2));

    GPIOA_MODER &= ~((3U << (5 * 2)) |
                     (3U << (6 * 2)) |
                     (3U << (7 * 2)));

    GPIOA_MODER |=  ((2U << (5 * 2)) |
                     (2U << (6 * 2)) |
                     (2U << (7 * 2)));

    GPIOA_AFRL &= ~((0xFU << (5 * 4)) |
                    (0xFU << (6 * 4)) |
                    (0xFU << (7 * 4)));

    GPIOA_AFRL |=  ((5U << (5 * 4)) |
                    (5U << (6 * 4)) |
                    (5U << (7 * 4)));

    GPIOB_ODR |= (1U << 6);

    SPI1_CR1 =
        (1U << 2) |     /* Master */
        (3U << 3) |     /* /16 */
        (1U << 8) |
        (1U << 9) |
        (1U << 0) |     /* CPHA = 1 */
        (1U << 6);

    while (1) {
        identity = st25_read_reg(0x3F);
        (void)identity;
        delay(800000);
    }
}
