/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include "hw_config.h"
#include "nrf52840.h"
#include "service_debug.h"
/* This is a legacy header */
#include "nrf_drv_clock.h"
/* This is a legacy header */
#include "nrf_drv_power.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#endif

#include "rgbled.h"
#include "rgbled_hal_impl.h"

uint8_t USE_SYSTEM_FLAGS;
uint16_t tempFlag;

button_config_t HAL_Buttons[] = {
    {
        .active = 0,
        .pin = BUTTON1_GPIO_PIN,
        .mode = BUTTON1_GPIO_MODE,
        .pupd = BUTTON1_GPIO_PUPD,
        .debounce_time = 0,

        .event_in = BUTTON1_GPIOTE_EVENT_IN,
        .event_channel = BUTTON1_GPIOTE_EVENT_CHANNEL,
        .int_mask = BUTTON1_GPIOTE_INT_MASK,
        .int_trigger = BUTTON1_GPIOTE_TRIGGER,
        .nvic_irqn = BUTTON1_GPIOTE_IRQn,
        .nvic_irq_prio = BUTTON1_GPIOTE_IRQ_PRIORITY
    },
    {
        .active = 0,
        .debounce_time = 0
    }
};

const led_config_t HAL_Leds_Default[] = {
    {
        .version = 0x00,
        .pin = LED1_GPIO_PIN,
        .mode = LED1_GPIO_MODE,
        .is_inverted = 1
    },
    {
        .version = 0x00,
        .pin = LED2_GPIO_PIN,
        .mode = LED2_GPIO_MODE,
        .is_inverted = 1
    },
    {
        .version = 0x00,
        .pin = LED3_GPIO_PIN,
        .mode = LED3_GPIO_MODE,
        .is_inverted = 1
    },
    {
        .version = 0x00,
        .pin = LED4_GPIO_PIN,
        .mode = LED4_GPIO_MODE,
        .is_inverted = 1
    },
};
static nrf_pwm_values_wave_form_t rgb_wave_form_values;

static void DWT_Init(void)
{
    // DBGMCU->CR |= DBGMCU_CR_SETTINGS;
    // DBGMCU->APB1FZ |= DBGMCU_APB1FZ_SETTINGS;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief  Configures Main system clocks & power.
 * @param  None
 * @retval None
 */
void Set_System(void)
{
    ret_code_t ret = nrf_drv_clock_init();
    SPARK_ASSERT(ret == NRF_SUCCESS || ret == NRF_ERROR_MODULE_ALREADY_INITIALIZED);

    nrf_drv_clock_hfclk_request(NULL);
    while (!nrf_drv_clock_hfclk_is_running())
    {
        /* Waiting */
    }

    nrf_drv_clock_lfclk_request(NULL);
    while (!nrf_drv_clock_lfclk_is_running())
    {
        /* Waiting */
    }

    ret = nrf_drv_power_init(NULL);
    SPARK_ASSERT(ret == NRF_SUCCESS || ret == NRF_ERROR_MODULE_ALREADY_INITIALIZED);

    DWT_Init();

    /* Configure RTC0 for BUTTON-DEBOUNCE usage */
    UI_Timer_Configure();

    /* Configure the LEDs and set the default states */
    int LEDx;
    for(LEDx = 0; LEDx < LEDn; ++LEDx)
    {
        LED_Init(LEDx);
    }

    /* Configure the Button */
    BUTTON_Init(BUTTON1, BUTTON_MODE_EXTI);
}

void SysTick_Configuration(void) {
    /* Setup SysTick Timer for 1 msec interrupts */
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        /* Capture error */
        while (1)
        {
        }
    }

    /* Configure the SysTick Handler Priority: Preemption priority and subpriority */
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_IRQ_PRIORITY);   //OLD: NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0x03, 0x00)
}

void Set_RGB_LED_Values(uint16_t r, uint16_t g, uint16_t b)
{
    // TBD: Change the polarity for inverted RGB connection
    rgb_wave_form_values.channel_0 = r;
    rgb_wave_form_values.channel_1 = g;
    rgb_wave_form_values.channel_2 = b;

    // Starts the PWM generation
    nrf_pwm_task_trigger(NRF_PWM0, NRF_PWM_TASK_SEQSTART0);
}

uint16_t Get_RGB_LED_Max_Value(void)
{
    return rgb_wave_form_values.counter_top;
}

void Set_User_LED(uint8_t state)
{
    if ((!state && HAL_Leds_Default[LED_USER].is_inverted) || \
        (state && !HAL_Leds_Default[LED_USER].is_inverted))
    {
        nrf_gpio_pin_set(HAL_Leds_Default[LED_USER].pin);
    }
    else
    {
        nrf_gpio_pin_clear(HAL_Leds_Default[LED_USER].pin);
    }
}

void Toggle_User_LED(void)
{
    nrf_gpio_pin_toggle(HAL_Leds_Default[LED_USER].pin);
}

void Get_RGB_LED_Values(uint16_t* values)
{
    values[0] = rgb_wave_form_values.channel_0;
    values[1] = rgb_wave_form_values.channel_1;
    values[2] = rgb_wave_form_values.channel_2;
}

void UI_Timer_Configure(void)
{
    nrf_rtc_prescaler_set(NRF_RTC0, RTC_FREQ_TO_PRESCALER(UI_TIMER_FREQUENCY));

    nrf_rtc_event_clear(NRF_RTC0, NRF_RTC_EVENT_TICK);

    nrf_rtc_event_enable(NRF_RTC0, NRF_RTC_EVENT_TICK);

    nrf_rtc_task_trigger(NRF_RTC0, NRF_RTC_TASK_START);
}

static void RGB_PWM_Config(void)
{
    uint32_t output_pins[NRF_PWM_CHANNEL_COUNT];
    static nrf_pwm_sequence_t rgb_seq;

    output_pins[0] = HAL_Leds_Default[LED_RED].pin;
    output_pins[1] = HAL_Leds_Default[LED_GREEN].pin;
    output_pins[2] = HAL_Leds_Default[LED_BLUE].pin;
    output_pins[3] = NRF_PWM_PIN_NOT_CONNECTED;

    nrf_pwm_pins_set(NRF_PWM0, output_pins);

    // Base clock: 500KHz, Count mode: up counter, COUNTERTOP: 0(since we use the wave form load mode).
    nrf_pwm_configure(NRF_PWM0, NRF_PWM_CLK_500kHz, NRF_PWM_MODE_UP, 0);

    // Load mode: wave form, Refresh mode: auto
    nrf_pwm_decoder_set(NRF_PWM0, NRF_PWM_LOAD_WAVE_FORM, NRF_PWM_STEP_AUTO);

    // Configure the RGB PWM sequence, use sequence0 only
    rgb_wave_form_values.counter_top = 255;
    rgb_seq.values.p_wave_form       = &rgb_wave_form_values;
    rgb_seq.length                   = NRF_PWM_VALUES_LENGTH(rgb_wave_form_values);
    rgb_seq.repeats                  = 0;
    rgb_seq.end_delay                = 0;
    nrf_pwm_sequence_set(NRF_PWM0, 0, &rgb_seq);

    nrf_pwm_enable(NRF_PWM0);
}

/**
 * @brief  Configures LED GPIO.
 * @param  Led: Specifies the Led to be configured.
 *   This parameter can be one of following parameters:
 *     @arg LED1, LED2, LED3, LED4
 * @retval None
 */
void LED_Init(Led_TypeDef Led)
{
    nrf_gpio_cfg(
        HAL_Leds_Default[Led].pin,
        HAL_Leds_Default[Led].mode,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0S1,
        NRF_GPIO_PIN_NOSENSE);

    if (HAL_Leds_Default[Led].is_inverted)
    {
        nrf_gpio_pin_set(HAL_Leds_Default[Led].pin);
    }
    else
    {
        nrf_gpio_pin_clear(HAL_Leds_Default[Led].pin);
    }

    RGB_PWM_Config();
}

/**
 * @brief  Configures Button GPIO, EXTI Line and DEBOUNCE Timer.
 * @param  Button: Specifies the Button to be configured.
 *   This parameter can be one of following parameters:
 *     @arg BUTTON1: Button1
 * @param  Button_Mode: Specifies Button mode.
 *   This parameter can be one of following parameters:
 *     @arg BUTTON_MODE_GPIO: Button will be used as simple IO
 *     @arg BUTTON_MODE_EXTI: Button will be connected to EXTI line with interrupt
 *                     generation capability
 * @retval None
 */
void BUTTON_Init(Button_TypeDef Button, ButtonMode_TypeDef Button_Mode)
{
    nrf_gpio_cfg(
        HAL_Buttons[Button].pin,
        HAL_Buttons[Button].mode,
        NRF_GPIO_PIN_INPUT_CONNECT,
        HAL_Buttons[Button].pupd,
        NRF_GPIO_PIN_S0S1,
        NRF_GPIO_PIN_NOSENSE);

    if (Button_Mode == BUTTON_MODE_EXTI)
    {
        /* Disable RTC0 tick Interrupt */
        nrf_rtc_int_disable(NRF_RTC0, NRF_RTC_INT_TICK_MASK);

        BUTTON_EXTI_Config(Button, ENABLE);

        NVIC_SetPriority(HAL_Buttons[Button].nvic_irqn, HAL_Buttons[Button].nvic_irq_prio);
        NVIC_ClearPendingIRQ(HAL_Buttons[Button].nvic_irqn);
        NVIC_EnableIRQ(HAL_Buttons[Button].nvic_irqn);

        /* Enable the RTC0 NVIC Interrupt */
        NVIC_SetPriority(RTC0_IRQn, RTC0_IRQ_PRIORITY);
        NVIC_ClearPendingIRQ(RTC0_IRQn);
        NVIC_EnableIRQ(RTC0_IRQn);
    }
}

void BUTTON_EXTI_Config(Button_TypeDef Button, FunctionalState NewState)
{
    nrf_gpiote_event_configure(HAL_Buttons[Button].event_channel,
                               HAL_Buttons[Button].pin,
                               HAL_Buttons[Button].int_trigger);

    nrf_gpiote_event_clear(HAL_Buttons[Button].event_in);

    nrf_gpiote_event_enable(HAL_Buttons[Button].event_channel);

    if (NewState == ENABLE)
    {
        nrf_gpiote_int_enable(HAL_Buttons[Button].int_mask);
    }
    else
    {
        nrf_gpiote_int_disable(HAL_Buttons[Button].int_mask);
    }
}

/**
 * @brief  Returns the selected Button non-filtered state.
 * @param  Button: Specifies the Button to be checked.
 *   This parameter can be one of following parameters:
 *     @arg BUTTON1: Button1
 * @retval Actual Button Pressed state.
 */
uint8_t BUTTON_GetState(Button_TypeDef Button)
{
    return nrf_gpio_pin_read(HAL_Buttons[Button].pin);
}
