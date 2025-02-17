#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"

#include "ssd1306.h"
#include "ws2818b.pio.h"

// Buzzer
#define BUZZER_PIN 21

// Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

// OLED
#define I2C_PORT i2c1 // i2c1 é utilizado por padrão na placa
#define SDA_PIN 14
#define SCL_PIN 15

// Botões
#define BOTAO_A_PIN 5
#define BOTAO_B_PIN 6

// LED RGB
#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED 13

volatile bool botao_a_pressed = false, botao_b_pressed = false, a_and_b_pressed = false;

// Definição de pixel GRB
typedef struct
{
    uint8_t G, R, B;
} npLED_t;

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Inicializa a máquina PIO para controle da matriz de LEDs
void npInit(uint pin)
{
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0)
    {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i)
        leds[i] = (npLED_t){0, 0, 0};
}

// Atribui uma cor RGB a um LED.
void npSetLED(uint index, uint8_t r, uint8_t g, uint8_t b) { leds[index] = (npLED_t){g, r, b}; }

// Limpa o buffer de pixels.
void npClear()
{
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

// Escreve os dados do buffer nos LEDs.
void npWrite()
{
    for (uint i = 0; i < LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

// Função de interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events)
{
    if (gpio == BOTAO_A_PIN)
        botao_a_pressed = !gpio_get(BOTAO_A_PIN);
    if (gpio == BOTAO_B_PIN)
        botao_b_pressed = !gpio_get(BOTAO_B_PIN);
    if (botao_a_pressed && botao_b_pressed)
        a_and_b_pressed = true;
}

void init_pwm(uint gpio_pin)
{
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(pwm_gpio_to_slice_num(gpio_pin), &config, true);
}

void set_led_brightness(uint gpio_pin, int brightness_porc)
{
    pwm_set_gpio_level(gpio_pin, (uint16_t)(65535 * brightness_porc / 100.0));
}

// Toca uma nota com a frequência e duração especificadas
void play_tone(uint pin, uint frequency, uint duration_ms, float volume) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top * volume); // 0.3% de duty cycle - No meu bitDogLab ficou muito alto +1%

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
    sleep_ms(50); // Pausa entre notas
}

// Função para som de acerto
void play_success_tone() {
    float volume = 0.005;
    play_tone(BUZZER_PIN, 800, 100, volume);
    play_tone(BUZZER_PIN, 1000, 100, volume);
    play_tone(BUZZER_PIN, 1200, 100, volume);
}

// Função para som de erro
void play_error_tone() {
    float volume = 0.001;
    play_tone(BUZZER_PIN, 400, 300, volume);
    play_tone(BUZZER_PIN, 300, 300, volume);
}

int main()
{
    stdio_init_all();

    // Inicializa buzzer
    pwm_init_buzzer(BUZZER_PIN);

    // Inicializa matriz de LEDs NeoPixel.
    npInit(LED_PIN);
    npClear();
    npWrite(); // Escreve os dados nos LEDs.

    // Configuração dos botões
    gpio_init(BOTAO_A_PIN);
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN); // Ativa pull-up interno

    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_B_PIN); // Ativa pull-up interno

    // Configura interrupções nos botões
    gpio_set_irq_enabled_with_callback(BOTAO_A_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    // Inicializa o PWM nos LEDs - Eles eram muito fortes na minha placa
    init_pwm(LED_RED);
    init_pwm(LED_GREEN);
    init_pwm(LED_BLUE);

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000); // Configura I2C para 400 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    // Inicializa o display OLED
    ssd1306_init(I2C_PORT);
    ssd1306_clear();

    // Animação para troca de cores
    int led_pin = LED_RED;

    int pont = 0;
    int last_pont = 0;
    int cont_game = 0;
    char pts_String[20];
    char conclusao[20];
    bool start_game = false;

    while (true)
    {
        // Espera ambos os botões serem pressionados ao mesmo tempo
        while (!a_and_b_pressed && !start_game)
        {
            ssd1306_clear();
            snprintf(pts_String, sizeof(pts_String), "Pontos: %d", pont);
            if (pont > 0)
                ssd1306_draw_string(0, 0, pts_String, true);
            ssd1306_draw_string(get_center_x("Pressione A + B"), get_center_y() - 5, "Pressione A + B", true);
            ssd1306_draw_string(get_center_x("para comecar"), get_center_y() + 5, "para comecar", true);
            ssd1306_update(I2C_PORT);
            sleep_ms(100);
        }

        start_game = true;

        // Reseta o estado dos botões e da flag
        botao_a_pressed = botao_b_pressed = a_and_b_pressed = false;

        ssd1306_clear();
        snprintf(pts_String, sizeof(pts_String), "Pontos: %d", pont);
        if (pont > 0)
            ssd1306_draw_string(0, 0, pts_String, true);
        ssd1306_draw_string(20, get_center_y() - 10, "A - Vermelho", true);
        ssd1306_draw_string(20, get_center_y(), "B - Verde", true);
        ssd1306_draw_string(20, get_center_y() + 10, "A + B - Azul", true);
        ssd1306_update(I2C_PORT);

        // Animação de 3 segundos para preparação
        uint32_t start_time = time_us_32();
        uint32_t anim_duration = 3000000;
        while ((time_us_32() - start_time) < anim_duration)
        {
            // LED matriz
            npClear();
            if (led_pin == LED_GREEN)
                npSetLED(10, 0, 120, 0);
            else if (led_pin == LED_BLUE)
                npSetLED(12, 0, 0, 120);
            else
                npSetLED(14, 120, 0, 0);
            npWrite();

            sleep_ms(150);

            npClear();
            npWrite();

            sleep_ms(150);

            led_pin--;
            if (led_pin < 11)
                led_pin = LED_RED;
        }

        // Temporizador de reação - máximo de 700ms
        start_time = time_us_32();
        uint32_t reaction_duration = 700000;

        led_pin = LED_GREEN + (rand() % 3);

        npClear();
        if (led_pin == LED_GREEN)
            npSetLED(10, 0, 120, 0);
        else if (led_pin == LED_BLUE)
            npSetLED(12, 0, 0, 120);
        else
            npSetLED(14, 120, 0, 0);
        npWrite();

        while ((time_us_32() - start_time) < reaction_duration)
        {
            // botao_a_pressed = botao_b_pressed = a_and_b_pressed = false;
            if (led_pin == LED_RED && botao_a_pressed)
            {
                pont += 700 - ((time_us_32() - start_time) / 1000);
                printf("----- Vermelho ------\n");
                printf("pont: %d\n", pont);
                printf("ganhos: %d\n", 700 - ((time_us_32() - start_time) / 1000));
                set_led_brightness(LED_GREEN, 10);
                play_success_tone();
                sleep_ms(500);
                botao_a_pressed = false;
                break;
            }
            if (led_pin == LED_GREEN && botao_b_pressed)
            {
                pont += 700 - ((time_us_32() - start_time) / 1000);
                printf("----- Verde ------\n");
                printf("pont: %d\n", pont);
                printf("ganhos: %d\n", 700 - ((time_us_32() - start_time) / 1000));
                set_led_brightness(LED_GREEN, 10);
                play_success_tone();
                sleep_ms(500);
                botao_b_pressed = false;
                break;
            }
            if (led_pin == LED_BLUE && a_and_b_pressed)
            {
                pont += 700 - ((time_us_32() - start_time) / 1000);
                printf("----- Azul ------\n");
                printf("pont: %d\n", pont);
                printf("ganhos: %d\n", 700 - ((time_us_32() - start_time) / 1000));
                set_led_brightness(LED_GREEN, 10);
                play_success_tone();
                sleep_ms(500);
                botao_a_pressed = botao_b_pressed = a_and_b_pressed = false;
                break;
            }
        }

        if (last_pont != pont)
        {
            last_pont = pont;
        }
        else
        {
            set_led_brightness(LED_RED, 10);
            play_error_tone();
            sleep_ms(500);
        }

        set_led_brightness(LED_RED, 0);
        set_led_brightness(LED_GREEN, 0);

        cont_game++;
        printf("Jogo: %d\n", cont_game);

        npClear();
        npWrite();

        // Game over
        if (cont_game == 10)
        {
            start_game = false;
            cont_game = 0;

            while (!botao_a_pressed && !botao_b_pressed)
            {
                ssd1306_clear();
                ssd1306_draw_string(get_center_x("GAME OVER"), 10, "GAME OVER", true);
                if (pont < 3000)
                {
                    snprintf(conclusao, sizeof(conclusao), "Pontuacao baixa!");
                    ssd1306_draw_string(get_center_x(conclusao), 20, conclusao, true);
                    snprintf(pts_String, sizeof(pts_String), "Pontos: %d", pont);
                    ssd1306_draw_string(get_center_x(pts_String), 30, pts_String, true);
                    snprintf(conclusao, sizeof(conclusao), "Negado");
                    ssd1306_draw_string(get_center_x(conclusao), 40, conclusao, true);
                }
                else if (pont >= 3000)
                {
                    if (pont < 5000) snprintf(conclusao, sizeof(conclusao), "Pontuacao media!");
                    else snprintf(conclusao, sizeof(conclusao), "Pontuacao alta!");
                    ssd1306_draw_string(get_center_x(conclusao), 20, conclusao, true);
                    snprintf(pts_String, sizeof(pts_String), "Pontos: %d", pont);
                    ssd1306_draw_string(get_center_x(pts_String), 30, pts_String, true);
                    snprintf(conclusao, sizeof(conclusao), "Aprovado");
                    ssd1306_draw_string(get_center_x(conclusao), 40, conclusao, true);
                }
                ssd1306_update(I2C_PORT);
            }

            botao_a_pressed = botao_b_pressed = a_and_b_pressed = false;
            pont = 0;
        }
    }
    return 0;
}
