//Copyright 2015, 2018 <>< Charles Lohr, Adam Feinstein see LICENSE file.

/*==============================================================================
 * Includes
 *============================================================================*/

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "commonservices.h"
#include "vars.h"
#include <mdns.h>

/*==============================================================================
 * Partition Map Data
 *============================================================================*/

#define SYSTEM_PARTITION_OTA_SIZE_OPT2                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR_OPT2               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT2              0xfb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT2            0xfc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT2    0xfd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT2 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT2                        2

#define SYSTEM_PARTITION_OTA_SIZE_OPT3                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR_OPT3               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT3              0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3            0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT3    0x1fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT3 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT3                        3

#define SYSTEM_PARTITION_OTA_SIZE_OPT4                 0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR_OPT4               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT4              0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT4            0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT4    0x3fd000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT4 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT4                        4

#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM SYSTEM_PARTITION_CUSTOMER_BEGIN
#define EAGLE_FLASH_BIN_ADDR                 SYSTEM_PARTITION_CUSTOMER_BEGIN + 1
#define EAGLE_IROM0TEXT_BIN_ADDR             SYSTEM_PARTITION_CUSTOMER_BEGIN + 2

static const partition_item_t partition_table_opt2[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0x60000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT2,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT2,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT2, 0x3000},
};

static const partition_item_t partition_table_opt3[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0x60000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT3,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT3, 0x3000},
};

static const partition_item_t partition_table_opt4[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0x60000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT4,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT4,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT4, 0x3000},
};

/*==============================================================================
 * Process Defines
 *============================================================================*/

#define procTaskPrio        0
#define procTaskQueueLen    1
os_event_t    procTaskQueue[procTaskQueueLen];

/*==============================================================================
 * Variables
 *============================================================================*/

static os_timer_t some_timer;
static struct espconn *pUdpServer;
usr_conf_t * UsrCfg = (usr_conf_t*)(SETTINGS.UserData);

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * This task is called constantly. The ESP can't handle infinite loops in tasks,
 * so this task will post to itself when finished, in essence looping forever
 *
 * @param events unused
 */
static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	CSTick( 0 );

	// Post the task in order to have it called again
	system_os_post(procTaskPrio, 0, 0 );
}

/**
 * This is a timer set up in user_main() which is called every 100ms, forever
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR timer100ms(void *arg)
{
	CSTick( 1 ); // Send a one to uart
}

/**
 * This callback is registered with espconn_regist_recvcb and is called whenever
 * a UDP packet is received
 *
 * @param arg pointer corresponding structure espconn. This pointer may be
 *            different in different callbacks, please don’t use this pointer
 *            directly to distinguish one from another in multiple connections,
 *            use remote_ip and remote_port in espconn instead.
 * @param pusrdata received data entry parameters
 * @param len      received data length
 */
static void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;

	uart0_sendStr("X");
}

/**
 * UART RX handler, called by the uart task. Currently does nothing
 *
 * @param c The char received on the UART
 */
void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

/**
 * This is called on boot for versions ESP8266_NONOS_SDK_v1.5.2 to
 * ESP8266_NONOS_SDK_v2.2.1. system_phy_set_rfoption() may be called here
 */
void user_rf_pre_init(void)
{
	; // nothing
}


void ICACHE_FLASH_ATTR user_pre_init(void)
{
	LoadDefaultPartitionMap(); //You must load the partition table so the NONOS SDK can find stuff.
}

/**
 * The default method, equivalent to main() in other environments. Handles all
 * initialization
 */
void ICACHE_FLASH_ATTR user_init(void)
{
	// Initialize the UART
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	os_printf("\r\nesp82XX Web-GUI\r\n%s\b", VERSSTR);

	//Uncomment this to force a system restore.
	//	system_restore();

	// Load settings and pre-initialize common services
	CSSettingsLoad( 0 );
	CSPreInit();

	// Start a UDP Server
    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = COM_PORT;
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}

	// Initialize common settings
	CSInit( 1 );

	// Start MDNS services
	SetServiceName( "espcom" );
	AddMDNSName(    "esp82xx" );
	AddMDNSName(    "espcom" );
	AddMDNSService( "_http._tcp",    "An ESP82XX Webserver", WEB_PORT );
	AddMDNSService( "_espcom._udp",  "ESP82XX Comunication", COM_PORT );
	AddMDNSService( "_esp82xx._udp", "ESP82XX Backend",      BACKEND_PORT );

	// Set timer100ms to be called every 100ms
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)timer100ms, NULL);
	os_timer_arm(&some_timer, 100, 1);

	os_printf( "Boot Ok.\n" );

	// Set the wifi sleep type
	wifi_set_sleep_type(LIGHT_SLEEP_T);
	wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);

	// Add a process and start it
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);
	system_os_post(procTaskPrio, 0, 0 );
}

/**
 * This will be called to disable any interrupts should the firmware enter a
 * section with critical timing. There is no code in this project that will
 * cause reboots if interrupts are disabled.
 */
void ICACHE_FLASH_ATTR EnterCritical(void)
{
	;
}

/**
 * This will be called to enable any interrupts after the firmware exits a
 * section with critical timing.
 */
void ICACHE_FLASH_ATTR ExitCritical(void)
{
	;
}
