/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the license.txt file.
 */
/*
 * See README.md for a description of the application. 
 */ 

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_util.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "nrf51_bitfields.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "boards.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "ble_debug_assert_handler.h"
#include "nrf_soc.h"

#define DEBUG_APP  /* for debug, adds about 1000 bytes of RAM usage */

#define ARRAY_SIZE(s) (sizeof(s) / sizeof(*s))

#ifdef DEBUG_APP
#include "simple_uart.h"
#endif

#define IS_SRVC_CHANGED_CHARACT_PRESENT     0          /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/
#define ADV_INTERVAL_IN_MS          1001

#define ADV_INTERVAL                MSEC_TO_UNITS(ADV_INTERVAL_IN_MS, UNIT_0_625_MS) /**< The advertising interval (in units of 0.625 ms. */
#define ADV_TIMEOUT_IN_SECONDS      0                  /**< 0=disabled, The advertising timeout (in units of seconds). */

#define APP_TIMER_PRESCALER         0                  /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE     4                  /**< Size of timer operation queues. */
#define ADVDATA_UPDATE_INTERVAL     APP_TIMER_TICKS(ADV_INTERVAL_IN_MS, APP_TIMER_PRESCALER)

#define STOVE_RATE_INTERVAL         APP_TIMER_TICKS(999, APP_TIMER_PRESCALER)  /**< stove interval (ticks). */

#define ADC_SAMPLING_INTERVAL       APP_TIMER_TICKS(1000 / ADC_SAMPLES_PER_SEC, APP_TIMER_PRESCALER) /**< Sampling rate for the ADC */

#define DEAD_BEEF                   0xDEADBEEF         /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

/* pins used: */
/* UART RX_PIN = 16 TX_PIN = 17 */
#define STOVE_ALARM_NOT_PIN_NUMBER 18  /* Pin number stove alarm (negative logic). */
typedef enum {
	ADC_STATE_INIT,  /* initial state, ground zero */
	ADC_STATE_ALARM_ON,  /* we are alarming */
	ADC_STATE_WAITING,  /* waiting between alarms */
	}adc_state_t;

typedef enum {
	STOVE_RATE_TIMER,  /* Stove rate timer */
	ADC_SAMPLING_TIMER,  /* ADC timer */
	ADVDATA_UPDATE_TIMER,
	APP_TIMER_MAX_TIMERS,  /* placeholder */
	}timers_t;

#define MAX_HALF_CYCLES 80
#define MINS_PER_HR 60
#define SECS_PER_MIN 60  /* make equal to 2 to speed things up for debug */
#define SECS_PER_HR (MINS_PER_HR * SECS_PER_MIN)
struct DATA
	{
	uint32_t adc_accum;
	uint32_t adc_result;
#ifdef DEBUG_APP
#define ADC_THRESHOLD 7  /* default threshold for stove being on (0 watts = 5, 75 watts=10, 1500 watts=97) */
#else
#define ADC_THRESHOLD (5 + 500 / 15)  /* set for 500 watts, don't want light bulbs setting it off */
#endif
	uint32_t threshold;
	uint16_t adc_sec_timer;
#define ADC_ALARM_ON_WAIT_SECS (5 * SECS_PER_MIN)  /* 5 min's */
#define ADC_ALARM_ON_TIME 1 /* 1 sec */
#define ADC_ALARM_OFF_TIME 1 /* 1 sec */
#define ADC_ALARM_ON_CYCLES 5 /* 5 times */
	uint16_t adc_cntr;
	uint16_t head_dx;  /* head index */
#define ADC_SAMPLES_PER_SEC 10
#define BITS_PER_BYTE 8
	uint8_t samplebuf[2 * SECS_PER_HR / BITS_PER_BYTE];  /* each bit is equal to one sec, set for 2 hours */
#define ADC_PERCENT_THRESHOLD 7  /* percent of ticks threshold */ 
	adc_state_t     adc_state;

	app_timer_id_t         timer_ids[APP_TIMER_MAX_TIMERS];         /**< timers. */
	
	/* temp stuff */
	};

static struct DATA data =
	{
	.threshold = ADC_THRESHOLD,
	};


/**@brief Function for error handling, which is called when an error has occurred. 
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze 
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name. 
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
	{
	// This call can be used for debug purposes during development of an application.
	// @note CAUTION: Activating this code will write the stack to flash on an error.
	//                This function should NOT be used in a final product.
	//                It is intended STRICTLY for development/debugging purposes.
	//                The flash write will happen EVEN if the radio is active, thus interrupting
	//                any communication.
	//                Use with care. Un-comment the line below to use.
#ifdef DEBUG_APP
	ble_debug_assert_handler(error_code, line_num, p_file_name);
#endif
	// On assert, the system can only recover with a reset.
	//NVIC_SystemReset();
	}


/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze 
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
	{
	app_error_handler(DEAD_BEEF, line_num, p_file_name);
	}

/**@brief Function for initializing buttons.
 */
static void gpio_init(struct DATA *p_data)
	{
	NRF_GPIO->PIN_CNF[STOVE_ALARM_NOT_PIN_NUMBER] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	nrf_gpio_pin_set(STOVE_ALARM_NOT_PIN_NUMBER);  /* set to high impedance */
	}

//ADC initialization
static void adc_init(struct DATA *p_data)
	{	
	/* Enable interrupt on ADC sample ready event*/		
	NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;   
	sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_LOW);  
	sd_nvic_EnableIRQ(ADC_IRQn);
	
	NRF_ADC->CONFIG	= (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos) /* Bits 17..16 : ADC external reference pin selection. */
		| (ADC_CONFIG_PSEL_AnalogInput2 << ADC_CONFIG_PSEL_Pos)					/*!< Use analog input 2 as analog input. */
		| (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)							/*!< Use internal 1.2V bandgap voltage as reference for conversion. */
		| (ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos) /*!< Analog input specified by PSEL with no prescaling used as input for the conversion. */
		| (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos);									/*!< 8bit ADC resolution. */ 
	
	/* Enable ADC*/
	NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
	}

/* Interrupt handler for ADC data ready event */
void ADC_IRQHandler(void)
	{
	struct DATA *p_data = &data;

	/* Clear dataready event */
  NRF_ADC->EVENTS_END = 0;

	p_data->adc_accum += NRF_ADC->RESULT;  /* accumulate samples over time */
	if (++p_data->adc_cntr >= ADC_SAMPLES_PER_SEC)
		{  /* take an average once a sec */
		p_data->adc_result = p_data->adc_accum / p_data->adc_cntr;  /* take average */
		p_data->adc_accum = p_data->adc_cntr = 0;
		if (p_data->adc_result > p_data->threshold)  /* if over then set a bit */
			p_data->samplebuf[p_data->head_dx / BITS_PER_BYTE] |= (1 << (p_data->head_dx % BITS_PER_BYTE));
		if (++p_data->head_dx >= ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE)
			p_data->head_dx = 0;  /* wrap head */
		if (p_data->head_dx % BITS_PER_BYTE == 0)  /* did we go to the next byte? */
			p_data->samplebuf[p_data->head_dx / BITS_PER_BYTE] = 0;  /* start over */
		}

	//Use the STOP task to save current. Workaround for PAN_028 rev1.5 anomaly 1.
  NRF_ADC->TASKS_STOP = 1;
	
	//Release the external crystal
	sd_clock_hfclk_release();
	}	

/**@brief Function for the GAP initialization.
 *
 * @details This function shall be used to setup all the necessary GAP (Generic Access Profile) 
 *          parameters of the device. It also sets the permissions and appearance.
 */
static void gap_params_init(struct DATA *p_data)
	{
	uint32_t                err_code;
	ble_gap_conn_sec_mode_t sec_mode;
    
	char *name_buffer = "Stove Alarm";
	/* sprintf(name_buffer, "%08X", (unsigned int) NRF_FICR->DEVICEID[0]); */
    
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    
	err_code = sd_ble_gap_device_name_set(&sec_mode,
		(const uint8_t *)name_buffer, 
		strlen(name_buffer));
	APP_ERROR_CHECK(err_code);  
	}

static uint32_t temperature_data_get(struct DATA *p_data)
	{
	int32_t temp;
	uint32_t err_code;
    
	err_code = sd_temp_get(&temp);
	APP_ERROR_CHECK(err_code);
    
//	return ((((temp + (32 * 4)) * (9 * 4)) / (5 * 4)) / 4);  /* F = 9/5 (C+32) (everything times 4 */
	return (temp / 4);  /* C */
	}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advdata_update(struct DATA *p_data)
	{
	uint32_t      err_code;
	ble_advdata_t advdata;
	uint8_t       flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
    
	ble_advdata_service_data_t service_data[2];
    
	uint8_t battery_data = temperature_data_get(p_data);  /* put the temp in the battery icon */
	uint32_t temperature_data = (p_data->adc_result - 5) * 15;  /* roughly convert to watts */
    
	service_data[0].service_uuid = BLE_UUID_BATTERY_SERVICE;
	service_data[0].data.size    = sizeof(battery_data);
	service_data[0].data.p_data  = &battery_data;

	service_data[1].service_uuid = BLE_UUID_HEALTH_THERMOMETER_SERVICE;
	service_data[1].data.size    = sizeof(temperature_data);
	service_data[1].data.p_data  = (uint8_t *) &temperature_data;

	// Build and set advertising data
	memset(&advdata, 0, sizeof(advdata));

	advdata.name_type            = BLE_ADVDATA_FULL_NAME;
	advdata.include_appearance   = false;
	advdata.flags.size           = sizeof(flags);
	advdata.flags.p_data         = &flags;
	advdata.service_data_count   = ARRAY_SIZE(service_data);
	advdata.p_service_data_array = service_data;

	err_code = ble_advdata_set(&advdata, NULL);
	APP_ERROR_CHECK(err_code);
	}

static void advdata_update_timer_timeout_handler(void * p_context)
	{
	struct DATA *p_data = p_context;
	advdata_update(p_data);
	}

// ADC timer handler to start ADC sampling
static void adc_sampling_timeout_handler(void *p_context)
	{
	uint32_t p_is_running = 0;

	sd_clock_hfclk_request();
	while(! p_is_running) {  							//wait for the hfclk to be available
		sd_clock_hfclk_is_running((&p_is_running));
		}               
	NRF_ADC->TASKS_START = 1;							//Start ADC sampling
	}

/**@brief Function for handling the stove interval timer timeout.
 *
 * @details This function will be called each time the stove interval timer expires.
 *
 * @param[in]   p_context   Pointer used for passing some arbitrary information (context) from the
 *                          app_start_timer() call to the timeout handler.

 By experiment, burner draws current for 3-10 seconds and off for
 40-55 (5.4% - 25%) seconds when on low (83 cycles/hour).  The oven
 draws current for 1.3 mins and off for 40-53 (3.25%) mins when on
 low(1.5 cycles/hour), 84 secs on and 380 secs off (11%) when at 350
 degrees (7.8 cycles/hour).  The algorithm is this: An event occurs
 if the range is drawing current for at least 7 percent of the time
 for the past 120 minutes or there are more than 40 on/off cyles and
 the first time it came on was over 1.5 hour ago.  If there has been
 a gap of no activity of over 45 min's, then ignore everthing before
 that time.  When an event occurs then every time it comes on beep
 for 1 sec on, 1 sec off for 5 times, then wait for 5 min's.

*/
static void stove_rate_timeout_handler(void *p_context)  /* runs once every sec */
	{
	struct DATA *p_data = p_context;
	int cntr, start, head_dx, old_val, val, total, no_activity_cntr, cycles, event = 0;

  /* go over all samples, total = number of sec's over threshold of array */
	start = ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE;
	no_activity_cntr = cycles = old_val = 0;
	total = 0;  /* number of sec's there was current draw over 120 min's */
	head_dx = p_data->head_dx + BITS_PER_BYTE;  /* start with the oldest one */
	if (head_dx >= ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE)
		head_dx -= ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE;
	for (cntr = BITS_PER_BYTE; cntr < ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE; cntr++)
		{
		if (p_data->samplebuf[head_dx / BITS_PER_BYTE] & (1 << (head_dx % BITS_PER_BYTE)))
			val = 1;
		else
			val = 0;
		total += val;
		if ((val && !old_val) || (!val && old_val))
			{
			cycles++;  /* count half cycles */
			old_val = val;
			}
		if (val)
			no_activity_cntr = 0;
		else
			{
			if (++no_activity_cntr >= 30 * SECS_PER_MIN)  /* 30 min's with no activity? */
				{
				total = 0;  /* start over */
				cycles = 0;
				}
			}
		if (val && (start == ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE))
			start = cntr;  /* set start point */
		if (++head_dx >= ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE)
			head_dx = 0;
		}
	if ((total > (ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE) / (100 / ADC_PERCENT_THRESHOLD)) || /* over percent threshold? */
		cycles > MAX_HALF_CYCLES)  /* more than max cycles? */
		if (ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE - start > SECS_PER_HR * 1.5)  /* did the first occurance happen over 1.5 hours ago? */
			if (p_data->adc_result > p_data->threshold)  /* over threshold at this time? */
				event = 1;

#ifdef DEBUG_APP
	static int out_cntr = 0;
	char buf[100];
	sprintf(buf, "cntr=%3d per=%3d noact=%3d cycles=%3d start=%3d adc=%2d state=%d \r\n", 
		out_cntr++, total * 100 / (ARRAY_SIZE(p_data->samplebuf) * BITS_PER_BYTE), no_activity_cntr, cycles, start, (int) p_data->adc_result, (int) p_data->adc_state);
	simple_uart_putstring((const uint8_t *)buf);
#endif

	switch (p_data->adc_state)
		{
		default:  /* fall thru */
		case ADC_STATE_INIT:  /* initial state */
			if (event)
				{
				p_data->adc_state = ADC_STATE_ALARM_ON;
				p_data->adc_sec_timer = 0;
				}
			break;

		case ADC_STATE_ALARM_ON:  /* alarming, beeping N times */
			p_data->adc_sec_timer++;
			if (p_data->adc_sec_timer & 1)
				nrf_gpio_pin_clear(STOVE_ALARM_NOT_PIN_NUMBER);  /* set to low, sound alarm */
			else
				{
				nrf_gpio_pin_set(STOVE_ALARM_NOT_PIN_NUMBER);  /* set to high impedance */
				if (p_data->adc_sec_timer >= ADC_ALARM_ON_CYCLES * 2)
					{
					p_data->adc_state = ADC_STATE_WAITING;  /* go wait for awhile until alarming again */
					p_data->adc_sec_timer = 0;
					}
				}
			break;

		case ADC_STATE_WAITING:  /* we beeped, wait for wait time */
			if (++p_data->adc_sec_timer >= ADC_ALARM_ON_WAIT_SECS)
				{
				if (event)
					p_data->adc_state = ADC_STATE_ALARM_ON;
				else
					p_data->adc_state = ADC_STATE_INIT;
				p_data->adc_sec_timer = 0;
				}
			break;

		}
	}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(struct DATA *p_data)
	{
	uint32_t err_code;

	// Initialize timer module. FIXME why do we need +1 on the timers?
	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS + 1, APP_TIMER_OP_QUEUE_SIZE, false);
    
	// Create timers.
	err_code = app_timer_create(&p_data->timer_ids[ADVDATA_UPDATE_TIMER],
		APP_TIMER_MODE_REPEATED,
		advdata_update_timer_timeout_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&p_data->timer_ids[STOVE_RATE_TIMER],
		APP_TIMER_MODE_REPEATED,
		stove_rate_timeout_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&p_data->timer_ids[ADC_SAMPLING_TIMER],
		APP_TIMER_MODE_REPEATED,
		adc_sampling_timeout_handler);
	APP_ERROR_CHECK(err_code);
	}


/**@brief Function for starting timers.
 */
static void timers_start(struct DATA *p_data)
	{
	uint32_t err_code;

	err_code = app_timer_start(p_data->timer_ids[ADVDATA_UPDATE_TIMER], ADVDATA_UPDATE_INTERVAL, &data);
	APP_ERROR_CHECK(err_code);

	// Start application timers.
	err_code = app_timer_start(p_data->timer_ids[STOVE_RATE_TIMER], STOVE_RATE_INTERVAL, &data);
	APP_ERROR_CHECK(err_code);

	//ADC timer start
	err_code = app_timer_start(p_data->timer_ids[ADC_SAMPLING_TIMER], ADC_SAMPLING_INTERVAL, &data);
	APP_ERROR_CHECK(err_code);

	}


/**@brief Function for starting advertising.
 */
static void advertising_start(struct DATA *p_data)
	{
	uint32_t             err_code;
	ble_gap_adv_params_t adv_params;
    
	// Start advertising
	memset(&adv_params, 0, sizeof(adv_params));
    
	adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
	adv_params.p_peer_addr = NULL;
	adv_params.fp          = BLE_GAP_ADV_FP_ANY;
	adv_params.interval    = ADV_INTERVAL;
	adv_params.timeout     = ADV_TIMEOUT_IN_SECONDS;

	err_code = sd_ble_gap_adv_start(&adv_params);
	APP_ERROR_CHECK(err_code);
	}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
	{
	uint32_t err_code = NRF_SUCCESS;
    
	switch (p_ble_evt->header.evt_id)
    {
		default:
			break;
    }

	APP_ERROR_CHECK(err_code);
	}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
	{
	on_ble_evt(p_ble_evt);
	}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
	{
	}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(struct DATA *p_data)
	{
	uint32_t err_code;

	// Initialize the SoftDevice handler module.
	NRF_CLOCK->XTALFREQ = 0;
	NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
	NRF_CLOCK->TASKS_HFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
		;
	SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_4000MS_CALIBRATION, false);

#ifdef S110
	// Enable BLE stack 
	ble_enable_params_t ble_enable_params;
	memset(&ble_enable_params, 0, sizeof(ble_enable_params));
	ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
	err_code = sd_ble_enable(&ble_enable_params);
	APP_ERROR_CHECK(err_code);
#endif

	// Register with the SoftDevice handler module for BLE events.
	err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
	APP_ERROR_CHECK(err_code);
    
	// Register with the SoftDevice handler module for BLE events.
	err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
	APP_ERROR_CHECK(err_code);
	}

/**@brief Function for the Power manager.
 */
static void power_manage(struct DATA *p_data)
	{
	uint32_t err_code = sd_app_evt_wait();
	APP_ERROR_CHECK(err_code);
	}


/**@brief Function for application main entry.
 */
int main(void)
	{
	struct DATA *p_data = &data;

	// Initialize
	gpio_init(p_data);
	ble_stack_init(p_data);
	gap_params_init(p_data);
	timers_init(p_data);
	advdata_update(p_data);
    
	adc_init(p_data);         //Initialize ADC

	// Start execution
	timers_start(p_data);
	advertising_start(p_data);
    
#ifdef DEBUG_APP
	simple_uart_config(0, TX_PIN_NUMBER, 0, RX_PIN_NUMBER, false);
	NRF_UART0->BAUDRATE      = (UART_BAUDRATE_BAUDRATE_Baud115200 << UART_BAUDRATE_BAUDRATE_Pos);
#endif

	// Enter main loop
	for (;;)
    {
		power_manage(p_data);
    }
	}
