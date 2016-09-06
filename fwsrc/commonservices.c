//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

#include "commonservices.h"
#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include <mem.h>
#include "ets_sys.h"
#include "osapi.h"
#include "espconn.h"
#include "esp82xxutil.h"
#include "ip_addr.h"
#include "http.h"
#include "spi_flash.h"
#include "esp8266_rom.h"
#include <gpio.h>
#include "flash_rewriter.h"

#define buffprint(M, ...) buffend += ets_sprintf( buffend, M, ##__VA_ARGS__)

uint8_t printed_ip = 0;
static uint8_t attached_to_mdns = 0;

static struct espconn *pUdpServer;
static struct espconn *pHTTPServer;
struct espconn *pespconn;
uint16_t g_gpiooutputmask = 0;

int BrowseClientQuantity;
struct BrowseClient * FoundBrowseClients;
int time_since_last_browse = -1; //for expiring.

static char BrowsingService[11];
static char ServiceName[17];
static int  BrowseRequestTimeout = 0;
static int  BrowseSearchCount = 0; //Number of times to search for other ESPs when searching.
static uint32_t BrowseRespond = 0;
static uint16_t BrowseRespondPort = 0;
static uint32_t thisfromip;
static uint16_t thisfromport;
static int wifi_fail_connects;
int ets_str2macaddr(void *, void *);

uint8_t need_to_switch_opmode = 0; //0 = no, 1 = will need to. 2 = do it now.


void ICACHE_FLASH_ATTR SetServiceName( const char * myservice )
{
	int sl = ets_strlen( myservice );
	if( sl > 10 ) sl = 10;

	ets_memcpy( ServiceName, myservice, sl );
	ServiceName[sl] = 0;
}


#define MAX_STATIONS 20
struct totalscan_t
{
	char name[32];
	char mac[18]; //string
	int8_t rssi;
	uint8_t channel;
	uint8_t encryption;
};

struct totalscan_t ** scanarray;

int scanplace = 0;

static void ICACHE_FLASH_ATTR free_scan_array()
{
	if( !scanarray ) return;
	int i;
	for( i = 0; i < MAX_STATIONS; i++ )
	{
		if( scanarray[i] ) os_free( scanarray[i] );
	}
	os_free( scanarray );
	scanarray = 0;
}

static void ICACHE_FLASH_ATTR scandone(void *arg, STATUS status)
{
	free_scan_array();
	scanarray = (struct totalscan_t **)os_malloc( sizeof(struct totalscan_t *) * MAX_STATIONS ); 

	scaninfo *c = arg;
	struct bss_info *inf;

	if( need_to_switch_opmode == 1 )
		need_to_switch_opmode = 2;

	if( !c->pbss ) { scanplace = -1;  return;  }
	scanplace = 0;

	printf( "ISCAN\n" );

	STAILQ_FOREACH(inf, c->pbss, next) {
		struct totalscan_t * t = scanarray[scanplace++] = (struct totalscan_t *)os_malloc( sizeof(struct totalscan_t) );

		printf( "%s\n", inf->ssid );
		ets_memcpy( t->name, inf->ssid, 32 );
		ets_sprintf( t->mac, MACSTR, MAC2STR( inf->bssid ) );
		t->rssi = inf->rssi;
		t->channel = inf->channel;
		t->encryption = inf->authmode;
		inf = (struct bss_info *) &inf->next;
		if( scanplace == MAX_STATIONS - 1 ) break;
	}
}


//Service name can be the title of the service, or can be "esp8266" to list all ESP8266's.
void ICACHE_FLASH_ATTR BrowseForService( const char * servicename )
{
	int sl = ets_strlen( servicename );
	if( sl > 10 ) sl = 10;

	ets_memcpy( BrowsingService, servicename, sl );
	BrowsingService[sl] = 0;

	BrowseClientQuantity = 0;
	os_free( FoundBrowseClients );
	FoundBrowseClients = 0;
	time_since_last_browse = 0;

	BrowseRequestTimeout = 1;
	BrowseSearchCount = 5;
}


static void ICACHE_FLASH_ATTR EmitWhoAmINow( )
{
	char etsend[64];
	ets_sprintf( etsend, "BR%s\t%s\t%s", ServiceName, SETTINGS.DeviceName, SETTINGS.DeviceDescription );
	uint32_to_IP4(BrowseRespond,pUdpServer->proto.udp->remote_ip);
	pUdpServer->proto.udp->remote_port = BrowseRespondPort;
	espconn_sent( (struct espconn *)pUdpServer, etsend, ets_strlen( etsend ) );
	BrowseRespond = 0;
	printf( "Emitting WhoAmI\n" );
}


static void ICACHE_FLASH_ATTR EmitBrowseNow( )
{
	char etsend[32];
	ets_sprintf( etsend, "BQ%s", BrowsingService );
	uint32_to_IP4( ((uint32_t)0xffffffff), pUdpServer->proto.udp->remote_ip );
	pUdpServer->proto.udp->remote_port = BACKEND_PORT;
	espconn_sent( (struct espconn *)pUdpServer, etsend, ets_strlen( etsend ) );
}


#ifndef CMD_RET_TYPE
	#define CMD_RET_TYPE int ICACHE_FLASH_ATTR
#endif

CMD_RET_TYPE cmd_Browse(char * buffer, char *pusrdata, unsigned short len, char * buffend)
{
	if( len <= 1 ) return -1;
	char * srv = ParamCaptureAndAdvance();
	char * nam = ParamCaptureAndAdvance();
	char * des = ParamCaptureAndAdvance();

	NixNewline( srv );  NixNewline( nam );  NixNewline( des );
	int fromip = thisfromip;

	remot_info * ri = 0;
	espconn_get_connection_info( pUdpServer, &ri, 0);
	if( !ri ) return 0;

	switch( pusrdata[1] ) {
		case 'q': case 'Q': //Probe
			//Make sure it's either a wildcard, to our service or to us.
			if( srv && strcmp(srv,ServiceName) && strcmp(srv,SETTINGS.DeviceName) ) break;
			//Respond at a random time in the future (to prevent congestion)

			//Unless there's already a pending thing, then respond to that client.
			if( BrowseRespond ) EmitWhoAmINow();

			BrowseRespond = fromip;
			BrowseRespondPort = thisfromport;
		break;

		//Response
		case 'r': case 'R':
			if( srv && nam && des && time_since_last_browse < 0 ) break;
			//Find in list.
			int i = -1,   last_empty = -1;

			for( i = 0; i < BrowseClientQuantity; i++ ) {
				uint32_t ip = FoundBrowseClients[i].ip;
				if( last_empty == -1 && ip == 0 ) last_empty = i;
				if( fromip == ip ) break;
			}

			if( BrowseClientQuantity == 0 )	{
				FoundBrowseClients = (struct BrowseClient *)os_malloc( sizeof( struct BrowseClient ) );
				i = last_empty = 0;
				BrowseClientQuantity = 1;
			}

			if( i == BrowseClientQuantity ) {
				//Not in list.
				if( i == BROWSE_CLIENTS_LIST_SIZE_MAX )
					i = -1; //Can't add anymore.
				else {
					BrowseClientQuantity++;
					FoundBrowseClients = (struct BrowseClient *)os_realloc( FoundBrowseClients, sizeof( struct BrowseClient )*(BrowseClientQuantity) );
				}
			}

			//Update this entry.
			if( i != -1 ) {
				struct BrowseClient * bc = &FoundBrowseClients[i];
				int sl = ets_strlen( srv );   if( sl > 10 ) sl = 10;
				int nl = ets_strlen( nam );   if( nl > 10 ) nl = 10;
				int dl = ets_strlen( des );   if( dl > 16 ) dl = 16;
				ets_memcpy( bc->service, srv, sl ); bc->service[sl] = 0;
				ets_memcpy( bc->devicename, nam, nl ); bc->devicename[nl] = 0;
				ets_memcpy( bc->description, des, dl ); bc->description[dl] = 0;
				bc->ip = fromip;
			}
		break; // END: case 'r': case 'R': // Response

		case 's': case 'S':
			BrowseForService( nam ? nam : "" );
			buffprint( "BS\r\n" );
		break;

		case 'l': case 'L':	{
			int i, found = 0;
			for( i = 0; i < BrowseClientQuantity; i++ )
			    if( FoundBrowseClients[i].ip ) found++;

			buffprint( "BL%d\t%d\n", found, BrowseSearchCount );

			for( i = 0; i < BrowseClientQuantity; i++ ) {
				if( FoundBrowseClients[i].ip ) {
					struct BrowseClient * bc = &FoundBrowseClients[i];
					buffprint( "%08x\t%s\t%s\t%s\n",
						bc->ip, bc->service, bc->devicename, bc->description );
				}
			}
		} break;
	}

	return buffend - buffer;
} // END: cmd_Browse(...)


CMD_RET_TYPE cmd_GPIO(char * buffer, char *pusrdata, char * buffend)
{
	static const uint32_t AFMapper[16] = {
		0, PERIPHS_IO_MUX_U0TXD_U, 0, PERIPHS_IO_MUX_U0RXD_U,
		0, 0, 1, 1,
		1, 1, 1, 1,
		PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_MTCK_U, PERIPHS_IO_MUX_MTMS_U, PERIPHS_IO_MUX_MTDO_U
	};

	int nr = ParamCaptureAndAdvanceInt();

	if( AFMapper[nr] == 1 ) {
		buffprint( "!G%c%d\n", pusrdata[1], nr );
		return buffend - buffer;
	} else if( AFMapper[nr] )
		PIN_FUNC_SELECT( AFMapper[nr], 3);  //Select AF pin to be GPIO.

	switch( pusrdata[1] ) {
		case '0':
		case '1':  //Turn "on" or "off"
			GPIO_OUTPUT_SET(GPIO_ID_PIN(nr), pusrdata[1]-'0' );
			buffprint( "G%c%d", pusrdata[1], nr );
			g_gpiooutputmask |= (1<<nr);
		break;

		case 'i': case 'I': //Make Input
			GPIO_DIS_OUTPUT(GPIO_ID_PIN(nr));
			buffprint( "GI%d\n", nr );
			g_gpiooutputmask &= ~(1<<nr);
		break;

		case 'f': case 'F': {  //Toggle
			int on = GPIO_INPUT_GET( GPIO_ID_PIN(nr) );
			on = !on;
			GPIO_OUTPUT_SET(GPIO_ID_PIN(nr), on );
			g_gpiooutputmask |= (1<<nr);
			buffprint( "GF%d\t%d\n", nr, on );
		} break;

		case 'g': case 'G':
			buffprint( "GG%d\t%d\n", nr, GPIO_INPUT_GET( GPIO_ID_PIN(nr) ) );
		break;

		case 's': case 'S': {
			uint32_t rmask = 0;
			int i;
			for( i = 0; i < 16; i++ )
				rmask |= GPIO_INPUT_GET( GPIO_ID_PIN(i) )?(1<<i):0;
			buffprint( "GS\t%d\t%d\n", g_gpiooutputmask, rmask );
		} break;
	}
	return buffend - buffer;
} // END: cmd_GPIO(...)


CMD_RET_TYPE cmd_Echo(char * pursdata, int retsize, unsigned short len, char * buffend)
{
	if( retsize <= len ) return -1;
	ets_memcpy( buffend, pursdata, len );
	return len;
}


CMD_RET_TYPE cmd_Info(char * buffer, int retsize, char * pusrdata, char * buffend)
{
	int i;
	switch( pusrdata[1] ) {
		case 'b': case 'B': system_restart(); break;
		case 's': case 'S': CSSettingsSave();    buffprint( "IS\r\n" ); break;
		case 'l': case 'L': CSSettingsLoad( 0 ); buffprint( "IL\r\n" ); break;
		case 'r': case 'R': CSSettingsLoad( 1 ); buffprint( "IR\r\n" ); break;
		case 'f': case 'F': break; //Start finding devices, or return list of found devices.

		//Name
		case 'n': case 'N':
		{
			char * dn = ParamCaptureAndAdvance();
			for( i = 0; i < sizeof( SETTINGS.DeviceName )-1; i++ ) {
				char ci = *(dn++);
				if( ci >= 33 && ci <= 'z' )
					SETTINGS.DeviceName[i] = ci;
			}
			SETTINGS.DeviceName[i] = 0;
			buffprint( "\r\n" );
			return buffend - buffer;
		}
		//Description
		case 'd': case 'D':
		{
			char * dn = ParamCaptureAndAdvance();
			for( i = 0; i < sizeof( SETTINGS.DeviceDescription )-1; i++ ) {
				char ci = *(dn++);
				if( ci >= 32 && ci <= 'z' )
					SETTINGS.DeviceDescription[i] = ci;
			}
			SETTINGS.DeviceDescription[i] = 0;
			buffprint( "\r\n" );
		}
		break;

		// General Info
		default:
			buffprint( "I" );
			for( i = 0; i < 2 ;i++ ) {
				struct ip_info ipi;
				wifi_get_ip_info( i, &ipi );
				buffprint( "\t"IPSTR, IP2STR(&ipi.ip) );
			}
			buffprint(
				"\t%s" 				"\t%s" 						"\t%s" 		 "\t%d",
				SETTINGS.DeviceName, SETTINGS.DeviceDescription, ServiceName, system_get_free_heap_size()
			);
		break;
	}
	return buffend - buffer;
} // END: cmd_Info(...)


CMD_RET_TYPE cmd_WiFi(char * buffer, int retsize, char * pusrdata, char *buffend)
{
	int aplen = 0, passlen = 0;
	parameters++; //Assume "tab" after command.
	char * apname = ParamCaptureAndAdvance();
	char * password = ParamCaptureAndAdvance();
	char * encr = ParamCaptureAndAdvance(); //Encryption
	char * bssid = ParamCaptureAndAdvance();

	if( apname ) { aplen = ets_strlen( apname ); }
	if( password ) { passlen = ets_strlen( password ); }


	char mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	int bssid_set = 0;

	if( bssid ) {
		bssid_set = ets_str2macaddr( mac, bssid )?1:0;
		#define is_edge(x) ((x)==0x00 || (x)==0xff)
		if( is_edge(mac[0]) && is_edge(mac[1]) && is_edge(mac[2]) &&
			is_edge(mac[3]) && is_edge(mac[4]) && is_edge(mac[5])    )
			bssid_set = 0;
	}

	switch (pusrdata[1]) {
		case '1': //Station mode
		case '2': //AP Mode
			if( apname && password ) {
				if( aplen > 31 ) aplen = 31;
				if( passlen > 63 ) passlen = 63;

				printf( "Switching to: \"%s\"/\"%s\" (%d/%d). BSSID_SET: %d [%c]\n", apname, password, aplen, passlen, bssid_set, pusrdata[1] );

				if( pusrdata[1] == '1' ) {
					struct station_config stationConf;
					wifi_station_get_config(&stationConf);

					os_memcpy(&stationConf.ssid, apname, aplen);
					os_memcpy(&stationConf.password, password, passlen);

					stationConf.ssid[aplen] = 0;
					stationConf.password[passlen] = 0;
					stationConf.bssid_set = bssid_set;
					os_memcpy( stationConf.bssid, mac, 6 );

					printf( "-->'%s'\n" 	   "-->'%s'\n",
						    stationConf.ssid,  stationConf.password  );

					EnterCritical();
					//wifi_station_set_config(&stationConf);
					wifi_set_opmode_current( 1 );
					wifi_set_opmode( 1 );
					wifi_station_set_config(&stationConf);
					wifi_station_connect();
					wifi_station_set_config(&stationConf);  //I don't know why, doing this twice seems to make it store more reliably.
					ExitCritical();
					//wifi_station_get_config( &stationConf );

					buffprint( "W1\r\n" );
					printf( "Switching.\n" );
				} else {
					struct softap_config config;
					char macaddr[6];
					wifi_softap_get_config( &config );

					wifi_get_macaddr(SOFTAP_IF, macaddr);

					os_memcpy( &config.ssid, apname, aplen );
					os_memcpy( &config.password, password, passlen );
					config.ssid_len = aplen;
    				#if 0 //We don't support encryption.
					config.ssid[c1l] = 0;  config.password[c2l] = 0;   config.authmode = 0;
					if( encr ) {
						int k;
						for( k = 0; enctypes[k]; k++ )
							if( strcmp( encr, enctypes[k] ) == 0 )
								config.authmode = k;
					}
    				#endif

					int chan = (bssid) ? safe_atoi(bssid) : config.channel;
					if( chan == 0 || chan > 13 ) chan = 1;
					config.channel = chan;

					//printf( "Mode now. %s %s %d %d %d %d %d\n", config.ssid, config.password, config.ssid_len, config.channel, config.authmode, config.max_connection );
					//printf( "Mode Set. %d\n", wifi_get_opmode() );

					EnterCritical();
					wifi_softap_set_config(&config);
					wifi_set_opmode( 2 );
					ExitCritical();
					printf( "Switching SoftAP: %d %d.\n", chan, config.authmode );

					buffprint( "W2\r\n" );
				}
			}
		break;

		case 'I': case 'i': {
			char macmap[15];
			int mode = wifi_get_opmode();

			buffprint( "WI%d", mode );

			if( mode == 2 ) {
				uint8_t mac[6];
				struct softap_config ap;
				wifi_softap_get_config( &ap );
				wifi_get_macaddr( 1, mac );
				ets_sprintf( macmap, MACSTR, MAC2STR( mac ) );
				buffprint( "\t%s\t%s\t%s\t%d", ap.ssid, ap.password, macmap, ap.channel );
			} else {
				struct station_config sc;
				ets_memset( &sc, 0, sizeof( sc ) );
				wifi_station_get_config( &sc );
				if( sc.bssid_set )
					ets_sprintf( macmap, MACSTR, MAC2STR( sc.bssid ) );
				else
					macmap[0] = 0;
				buffprint( "\t%s\t%s\t%s\t%d", sc.ssid, sc.password, macmap, wifi_get_channel() );
			}
		} break;

		case 'X': case 'x': {
			int rssi = wifi_station_get_rssi();
			if( rssi >= 0 )
				buffprint( "WX-\t%08x", GetCurrentIP() );
			else
				buffprint( "WX%d\t%08x", wifi_station_get_rssi(), GetCurrentIP() );
		} break;

		case 'S': case 's': {
			int r;   struct scan_config sc;

			sc.ssid = 0;  sc.bssid = 0;  sc.channel = 0;  sc.show_hidden = 1;

			EnterCritical();
			if( wifi_get_opmode() == SOFTAP_MODE ) {
				wifi_set_opmode_current( STATION_MODE );
				need_to_switch_opmode = 1;
			}
			r = wifi_station_scan(&sc, scandone );
			ExitCritical();

			buffprint( "WS%d\n", r );
			uart0_sendStr(buffer);
		} break;

		case 'R': case 'r': {
			int i;
			if( !scanarray )
			{
				buffprint( "!WR" );
			}
			else
			{
				buffprint( "WR%d\n", scanplace );
				for( i = 0; i < scanplace && buffend - buffer < retsize - 64; i++ ) {
					buffprint( "#%s\t%s\t%d\t%d\t%s\n",
						scanarray[i]->name, scanarray[i]->mac, scanarray[i]->rssi, scanarray[i]->channel, enctypes[scanarray[i]->encryption] );
				}
				free_scan_array();
			}

		} break;
	}
	return buffend - buffer;
} // END: cmd_WiFi(...)


CMD_RET_TYPE cmd_Flash(char * buffer, int retsize, char *pusrdata, unsigned short len, char * buffend) {
	flashchip->chip_size = 0x01000000;
	int nr = ParamCaptureAndAdvanceInt();

	switch (pusrdata[1]) {
		//(FE#\n) <- # = sector.
		case 'e': case 'E': {
			if( nr < 16 ) { buffprint( "!FE%d\r\n", nr ); break; }
			EnterCritical();   spi_flash_erase_sector( nr );   ExitCritical();
			buffprint( "FE%d\r\n", nr );
		} break;

		//(FB#\n) <- # = block.
		case 'b': case 'B': {
			if( nr < 1 ) { buffprint( "!FB%d\r\n", nr ); break; }
			EnterCritical();   SPIEraseBlock( nr );   ExitCritical();
			buffprint( "FB%d\r\n", nr );
		} break;

		//Execute the flash re-writer
		case 'm': case 'M': {
			//Tricky: fix up the pointer business.  The flash rewriter expects it all contiguous.
			*(parameters-1) = '\t';

			int r = FlashRewriter( &pusrdata[2], len-2 );
			buffprint( "!FM%d\r\n", r );
		} break;

		//Flash Write (FW#\n) <- # = byte pos.  Reads until end-of-packet.
		case 'w': case 'W':
		{
			int datlen = ParamCaptureAndAdvanceInt();
			if( parameters && nr >= 65536 && datlen > 0 ) {
                debug( "FW%d\r\n", nr );
				ets_memcpy( buffer, parameters, datlen );

				EnterCritical();
				spi_flash_write( nr, (uint32*)buffer, (datlen/4)*4 );
				ExitCritical();

                #ifdef VERIFY_FLASH_WRITE
                #define VFW_SIZE 128
                int jj;
                uint8_t  __attribute__ ((aligned (32))) buf[VFW_SIZE];
                for(jj=0; jj<datlen; jj+=VFW_SIZE) {
                    spi_flash_read( nr+jj, (uint32*)buf, VFW_SIZE );
                    if( ets_memcmp( buf, buffer+jj, jj+VFW_SIZE>datlen ? datlen%VFW_SIZE : VFW_SIZE ) != 0 ) goto failw;
                }
                #endif

				buffprint( "FW%d\r\n", nr );
				break;
			}
            failw:
                buffprint( "!FW\r\n" );
		}
		break;

		//Flash Write Hex (FX#\t#\tDATTAAAAA) <- a = byte pos. b = length (in hex-pairs). Generally used for web-browser.
		case 'x': case 'X':
		{
			int siz = ParamCaptureAndAdvanceInt();
			char * colon2 = ParamCaptureAndAdvance();
			int i;
			//nr = place to write.
			//siz = size to write.
			//colon2 = data start.
			if( colon2 && (nr >= FLASH_PROTECTION_BOUNDARY || ( nr >= 0x10000 && nr < 0x30000 ) ) ) {
				colon2++;
                debug( "FX%d\t%d", nr, siz );
				int datlen = ((int)len - (colon2 - pusrdata))/2;
				if( datlen > siz ) datlen = siz;

				for( i = 0; i < datlen; i++ ) {
					int8_t r1, r2;
					r1 = r2 = fromhex1( *(colon2++) );
					if( r1 == -1 || r2 == -1 ) goto failfx;
					buffend[i] = (r1 << 4) | r2;
				}

				//ets_memcpy( buffer, colon2, datlen );

				EnterCritical();
				spi_flash_write( nr, (uint32*)buffend, (datlen/4)*4 );
				ExitCritical();

                #ifdef VERIFY_FLASH_WRITE
                // uint8_t  __attribute__ ((aligned (32))) buf[1300];
                // spi_flash_read( nr, (uint32*)buf, (datlen/4)*4 );
                // if( ets_memcmp( buf, buffer, (datlen/4)*4 ) != 0 ) break;
                // Rather do it in chunks, to avoid allocationg huge buf
                #define VFW_SIZE 128
                int jj;
                uint8_t  __attribute__ ((aligned (32))) buf[VFW_SIZE];
                for(jj=0; jj<datlen; jj+=VFW_SIZE) {
                    spi_flash_read( nr+jj, (uint32*)buf, VFW_SIZE );
                    if( ets_memcmp( buf, buffer+jj, jj+VFW_SIZE>datlen ? datlen%VFW_SIZE : VFW_SIZE ) != 0 ) goto failfx;
                }
                #endif

				buffprint( "FX%d\t%d\r\n", nr, siz );
				break;
			}
		    failfx:
				buffprint( "!FX\r\n" );
			break;
		}
		//Flash Read (FR#\t#) <- # = sector, #2 = size
		case 'r': case 'R':
			if( paramcount ) {
				int datlen = ParamCaptureAndAdvanceInt();
				datlen = (datlen/4)*4; //Must be multiple of 4 bytes
				if( datlen <= 1280 ) {
					buffprint( "FR%08d\t%04d\t", nr, datlen ); //Caution: This string must be a multiple of 4 bytes.
                    spi_flash_read( nr, (uint32*)buffend, datlen );
                    buffend += datlen;
					break;
				}
			}
			buffprint( "!FR\r\n" );
		break;
	}

	flashchip->chip_size = 0x00080000;
	return buffend - buffer;
} // END: cmd_Flash(...)


int ICACHE_FLASH_ATTR issue_command(char * buffer, int retsize, char *pusrdata, unsigned short len)
{
	parameters = pusrdata+2; //Except "Echo" which takes 1
	paramcount = 0;
	char * buffend = buffer;
	pusrdata[len] = 0;

	switch( pusrdata[0] ) {
		 //Flashing commands (F_)
		case 'f': case 'F':
			return cmd_Flash(buffer, retsize, pusrdata, len, buffend);

		// Browse request
		case 'b': case 'B':
			return cmd_Browse( buffer, pusrdata, len, buffend );

		// (W1:SSID:PASSWORD) (To connect) or (W2) to be own base station.  ...or WI, to get info... or WS to scan.
		case 'w': case 'W':
			return cmd_WiFi( buffer, retsize, pusrdata, buffend );

		// Respond with device info or other general system things
		case 'i': case 'I':
		 	return cmd_Info( buffer, retsize, pusrdata, buffend );

		// Set GPIO pin state
		case 'g': case 'G':
			return cmd_GPIO( buffer, pusrdata, buffend );

		// Echo command. E[data], responds with the same data.
		case 'e': case 'E':
			parameters--; //Echo is the only single-byte command.
			return cmd_Echo( pusrdata, retsize, len, buffend );

		// Issue a custom command defined by the user in ../user/custom_commands.c
		case 'c': case 'C':
			return CustomCommand( buffer, retsize, pusrdata, len);
	}
	return -1;
} // END: issue_command(...)


void ICACHE_FLASH_ATTR issue_command_udp(void *arg, char *pusrdata, unsigned short len)
{
	char  __attribute__ ((aligned (32))) retbuf[1300];
	struct espconn * rc = (struct espconn *)arg;
	remot_info * ri = 0;
	espconn_get_connection_info( rc, &ri, 0);

	thisfromip = IP4_to_uint32(ri->remote_ip);
	thisfromport = ri->remote_port;

	int r = issue_command( retbuf, 1300, pusrdata, len );

	if( r > 0 ) {
		ets_memcpy( rc->proto.udp->remote_ip, ri->remote_ip, 4 );
		rc->proto.udp->remote_port = ri->remote_port;
		espconn_sendto( rc, retbuf, r );
	}
}


static void ICACHE_FLASH_ATTR SwitchToSoftAP( )
{
	EnterCritical();
	wifi_set_opmode_current( SOFTAP_MODE ); // SOFTAP_MODE = 0x02
	struct softap_config sc;
	wifi_softap_get_config(&sc);
	printed_ip = 0;
	printf( "SoftAP mode: \"%s\":\"%s\" @ %d %d/%d\n", sc.ssid, sc.password, wifi_get_channel(), sc.ssid_len, wifi_softap_dhcps_status() );
	ExitCritical();
}


void ICACHE_FLASH_ATTR CSPreInit()
{
	int opmode = wifi_get_opmode();
	printf( "Opmode: %d\n", opmode );
	if( opmode == 1 ) {
		struct station_config sc;
		wifi_station_get_config(&sc);
		printf( "Station mode: \"%s\":\"%s\" (bssid_set:%d)\n", sc.ssid, sc.password, sc.bssid_set );
		int constat = wifi_station_connect();
//Disables null SSIDs.
//		if( sc.ssid[0] == 0 && !sc.bssid_set )	{ wifi_set_opmode( 2 );	opmode = 2; }
		debug("constat: %d", constat);
	}
	if( opmode == 2 ) {
		struct softap_config sc;
		wifi_softap_get_config(&sc);
		printf( "Default SoftAP mode: \"%s\":\"%s\"\n", sc.ssid, sc.password );
	}
}


void ICACHE_FLASH_ATTR CSInit()
{
    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = BACKEND_PORT;
	espconn_regist_recvcb(pUdpServer, issue_command_udp);
	espconn_create( pUdpServer );

	SetupMDNS();

	pHTTPServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pHTTPServer, 0, sizeof( struct espconn ) );
	espconn_create( pHTTPServer );
	pHTTPServer->type = ESPCONN_TCP;
    pHTTPServer->state = ESPCONN_NONE;
	pHTTPServer->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	pHTTPServer->proto.tcp->local_port = WEB_PORT;
    espconn_regist_connectcb(pHTTPServer, httpserver_connectcb);
    espconn_accept(pHTTPServer);
    espconn_regist_time(pHTTPServer, 15, 0); //timeout


	//Setup GPIO0 and 2 for input.
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U,FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U,FUNC_GPIO0);
	PIN_DIR_INPUT = _BV(2) | _BV(0);
	PIN_OUT_CLEAR = _BV(2);
}

static void ICACHE_FLASH_ATTR RestoreAndReboot( )
{
	PIN_DIR_OUTPUT = _BV(2); //Turn GPIO2 light off.
	system_restore();
	ets_delay_us(1000000);
	system_restart();
}

static void ICACHE_FLASH_ATTR SlowTick( int opm )
{
	static int GPIO0Down = 0;

	if( (PIN_IN & _BV(0)) == 0 )
	{
		if( GPIO0Down++ > (5000 / SLOWTICK_MS) )
		{
			RestoreAndReboot();
		}
	}
	else
	{
		GPIO0Down = 0;
	}

	HTTPTick(1);

	if( BrowseRespond ) EmitWhoAmINow();

	if( BrowseRequestTimeout == 1 ) {
		if( BrowseSearchCount ) {
			//Emit a browse.
			EmitBrowseNow();
			BrowseRequestTimeout = (rand()%20)+30;
			BrowseSearchCount--;
		} else BrowseRequestTimeout = 0;
	}
	else if( BrowseRequestTimeout ) BrowseRequestTimeout--;

	if( time_since_last_browse > KEEP_BROWSE_TIME ) {
		if( FoundBrowseClients ) os_free( FoundBrowseClients );
		FoundBrowseClients = 0;
		BrowseClientQuantity = 0;
		time_since_last_browse = -1;
	}

	if( opm == 1 ) {
		struct station_config wcfg;
		struct ip_info ipi;
		int stat = wifi_station_get_connect_status();

		if( stat == STATION_WRONG_PASSWORD || stat == STATION_NO_AP_FOUND || stat == STATION_CONNECT_FAIL ) {
			wifi_station_disconnect();
			wifi_fail_connects++;
			printf( "Connection failed with code %d... Retrying, try: ", stat, wifi_fail_connects );
#ifdef MAX_CONNECT_FAILURES_BEFORE_SOFTAP
			if( wifi_fail_connects > MAX_CONNECT_FAILURES_BEFORE_SOFTAP )
			{
				RestoreAndReboot();
			}
#endif
			wifi_station_connect();
			printf("\n");
			printed_ip = 0;
		} else if( stat == STATION_GOT_IP && !printed_ip ) {
			wifi_station_get_config( &wcfg );
			wifi_get_ip_info(0, &ipi);
			printf( "STAT: %d\n", stat );
			#define chop_ip(x) (((x)>>0)&0xff), (((x)>>8)&0xff), (((x)>>16)&0xff), (((x)>>24)&0xff)
			printf( "IP: %d.%d.%d.%d\n", chop_ip(ipi.ip.addr)      );
			printf( "NM: %d.%d.%d.%d\n", chop_ip(ipi.netmask.addr) );
			printf( "GW: %d.%d.%d.%d\n", chop_ip(ipi.gw.addr)      );
			printf( "WCFG: /%s/%s/\n"  , wcfg.ssid, wcfg.password  );
			printed_ip = 1;
			wifi_fail_connects = 0;
			CSConnectionChange();
		}
	}

	if(	need_to_switch_opmode > 2 ) {
		if( need_to_switch_opmode == 3 ) {
			need_to_switch_opmode = 0;
			wifi_set_opmode( 1 );
			wifi_station_connect();
		}
		else need_to_switch_opmode--;
	} else if( need_to_switch_opmode == 2 ) {
		need_to_switch_opmode = 0;
		SwitchToSoftAP();
	}
}


void CSTick( int slowtick )
{
	static uint8_t done_first_slowtick = 0;
	static uint8_t tick_flag = 0;
	static uint8_t last_opmode = 0;

	//We're coming in from a timer event: Don't do things.
	if( slowtick ) {  tick_flag = 1;  return;  }
	//Idle Event.

	int opm = wifi_get_opmode();
	if( opm != last_opmode ) {
		last_opmode = opm;
		CSConnectionChange();
	}

	if( !attached_to_mdns )
		if( JoinGropMDNS() == 0 ) attached_to_mdns = 1;

	HTTPTick(0);

	//TRICKY! If we use the timer to initiate this, connecting to people's networks
	//won't work!  I don't know why, so I do it here.  this does mean sometimes it'll
	//pause, though.
	if( tick_flag ) {
		SlowTick( opm );
		tick_flag = 0;

		if( !done_first_slowtick ) {
			done_first_slowtick = 1;
			//Do anything you want only done once, here.
		}
	}
}


void ICACHE_FLASH_ATTR CSConnectionChange()
{
	attached_to_mdns = 0;
}


void ICACHE_FLASH_ATTR CSSettingsLoad(int force_reinit)
{
	ets_memset( &SETTINGS, 0, sizeof( SETTINGS) );
	system_param_load( 0x3A, 0, &SETTINGS, sizeof( SETTINGS ) );
	printf( "Loading Settings: %02x / %d / %d / %d\n", SETTINGS.settings_key, force_reinit, SETTINGS.DeviceName[0], SETTINGS.DeviceName[0] );
	if( SETTINGS.settings_key != 0xAF || force_reinit || SETTINGS.DeviceName[0] == 0x00 || SETTINGS.DeviceName[0] == 0xFF ) {
		ets_memset( &SETTINGS, 0, sizeof( SETTINGS ) );

		uint8_t sysmac[6];
		printf( "Settings uninitialized.  Initializing.\n" );
		if( !wifi_get_macaddr( 0, sysmac ) );
			wifi_get_macaddr( 1, sysmac );

		ets_sprintf( SETTINGS.DeviceName, "ESP_%02X%02X%02X", sysmac[3], sysmac[4], sysmac[5] );
		ets_sprintf( SETTINGS.DeviceDescription, "Default" );
		printf( "Initialized Name: %s\n", SETTINGS.DeviceName );

		CSSettingsSave();
		system_restore();
	}

	wifi_station_set_hostname( SETTINGS.DeviceName );

	printf( "Settings Loaded: %s / %s\n", SETTINGS.DeviceName, SETTINGS.DeviceDescription );
}


void ICACHE_FLASH_ATTR CSSettingsSave()
{
	SETTINGS.settings_key = 0xAF;
	system_param_save_with_protect( 0x3A, &SETTINGS, sizeof( SETTINGS ) );
}


struct CommonSettings SETTINGS;
