//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

#include "flash_rewriter.h"
#include "commonservices.h"
#include <c_types.h>
#include <esp8266_rom.h>
#include <stdio.h>
#include "esp82xxutil.h"

#define SRCSIZE 4096
#define BLKSIZE 65536

static const char * key = "";
static int keylen = 0;

#ifdef QUIET_REFLASH
#define Kets_sprintf
#define Kuart0_sendStr
#else
#define Kets_sprintf ets_sprintf
#define Kuart0_sendStr uart0_sendStr
#endif

void ICACHE_FLASH_ATTR HEX16Convert( char * out, uint8_t * in )
{
	int i;
	for( i = 0; i < 16; i++ )
	{
		ets_sprintf( out+i*2, "%02x", in[i] );
	}
}

//Must reside in iram.

extern void Cache_Read_Disable(void);

static void FinalFlashRewrite( uint32_t from1, uint32_t to1, uint32_t size1, uint32_t from2, uint32_t to2, uint32_t size2, uint32_t firstfour )
{
	uint32 buf[SRCSIZE/4] __attribute__((aligned(32)));
	Kuart0_sendStr( "B\n" );

	int i, j;
	int ipl;
	int p;
	Cache_Read_Disable();

	// I know this syntax looks odd, but it really makes the code smaller!
	for( j = 2; j; j-- )
	{
		Kuart0_sendStr( "C\n" );
		p = to1/SRCSIZE;
		ipl = (size1/SRCSIZE)+1;
		for( i = ipl; i; i-- )
		{
			Kuart0_sendStr( "." );
			SPIRead( from1, buf, SRCSIZE );
			Kuart0_sendStr( "-" );
			if( to1 == 0 )
			{
				//Tricky: if first sector, must overwrite out our flash control bits.  This sets things like din, dio, qio, as well as configuration points.
				buf[0] = firstfour;
			}
			Kuart0_sendStr( "#" );
			//spi_flash_erase_sector( p++ );
			SPIEraseSector( p++ );
			Kuart0_sendStr( "$" );
			SPIWrite( to1, buf, SRCSIZE );
			Kuart0_sendStr( "!\n" );
			to1 += SRCSIZE;
			from1 += SRCSIZE;
		}
		from1 = from2;
		to1 = to2;
		size1 = size2;
		Kuart0_sendStr( "D\n" );
	}

	ets_wdt_enable();  //In case the system restart doesn't hit us... Not sure why it doesn't sometimes.
	Kuart0_sendStr( "E\n" );
	system_restart();  //This seems to not always trigger.

//	void(*rebootme)() = (void(*)())0x40000080;
//	rebootme();
}

void (*LocalFlashRewrite)( uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t ) = FinalFlashRewrite;



int ICACHE_FLASH_ATTR FlashRewriter( char * command, int commandlen )
{
	MD5_CTX md5ctx;
	parameters = command;
	int i, ipl = 0;
	int p;
	//[from_address]\t[to_address]\t[size]\t[MD5(key+data)]\t[from_address]\t[to_address]\t[size]\t[MD5(key+data)]
	command[commandlen] = 0;

	flashchip->chip_size = 0x01000000;

	ets_wdt_disable();

	paramcount = 0;
	uint32_t from1 = ParamCaptureAndAdvanceInt();
	uint32_t to1 =   ParamCaptureAndAdvanceInt();
	int32_t  size1 = ParamCaptureAndAdvanceInt();
	char *   md51  = ParamCaptureAndAdvance();
	uint32_t from2 = ParamCaptureAndAdvanceInt();
	uint32_t to2 =   ParamCaptureAndAdvanceInt();
	int32_t  size2 = ParamCaptureAndAdvanceInt();
	char *   md52  = ParamCaptureAndAdvance();
	uint32_t firstfour = *(uint32_t*)(0x40200000);

	{
		//Keep scope in here so all of these are not on the stack when entering the final call.
		char     md5h1raw[48];
		char     md5h1[48];
		char     md5h2raw[48];
		char     md5h2[48];
		char st[80];

		if( from1 == 0 || from2 == 0 || size1 == 0 || paramcount != 8 )
		{
			return 2;
		}

		if( ( from1 & 0xfff ) || ( from2 & 0xfff ) || ( to1 & 0xfff ) || ( to2 & 0xfff ) )
		{
			return 3;
		}


		//////////////////////////////

		Kets_sprintf( st, "Computing Hash 1: %08x size %d\n", from1, size1 );
		Kuart0_sendStr( st );

		MD5Init( &md5ctx );
		if( keylen )
			MD5Update( &md5ctx, key, keylen );
	//	MD5Update( &md5ctx, (uint8_t*)0x40200000 + from1, size1 );
		SafeMD5Update( &md5ctx, (uint8_t*)0x40200000 + from1, size1 );
		MD5Final( md5h1raw, &md5ctx );

		HEX16Convert( md5h1, md5h1raw );

		debug( "Hash 1: %s\n", md5h1 );

		for( i = 0; i < 32; i++ )
		{
			if( md5h1[i] != md51[i] )
			{
				debug( "File 1 MD5 mismatch: %s - "
					   "Expected: %s (F1MD5 MM)",
					   md5h1, md51                  );
				return 4;
			}
		}

		debug( "CH 2: %08x size %d\n", from2, size2 );

		MD5Init( &md5ctx );
		if( keylen )
			MD5Update( &md5ctx, key, keylen );
		SafeMD5Update( &md5ctx, (uint8_t*)0x40200000 + from2, size2 );
		MD5Final( md5h2raw, &md5ctx );

		HEX16Convert(md5h2, md5h2raw );

		debug("Hash 2: %s", md5h2);

	/*	for( i = 0; i <= size2/4; i++ )
		{
			uint32_t j = ((uint32_t*)(0x40200000 + from2))[i];
			Kets_sprintf( st, "%02x%02x%02x%02x\n", (uint8_t)(j>>0), (uint8_t)(j>>8), (uint8_t)(j>>16), (uint8_t)(j>>24) );
			Kuart0_sendStr( st );
		}*/

		for( i = 0; i < 32; i++ )
		{
			if( md5h2[i] != md52[i] )
			{
				debug( "File 1 MD5 mismatch: %s - "
					   "Expected: %s (F2MD5 MM)",
					   md5h2, md52                  );

				return 5;
			}
		}

		//Need to round sizes up.
		size1 = ((size1-1)&(~0xfff))+1;
		size2 = ((size2-1)&(~0xfff))+1;

		Kets_sprintf( st, "Copy 1: %08x to %08x, size %d\n", from1, to1, size1 );
		Kuart0_sendStr( st );
		Kets_sprintf( st, "Copy 2: %08x to %08x, size %d\n", from2, to2, size2 );
		Kuart0_sendStr( st );

		//Everything checked out... Need to move the flashes.

		//TODO: Disable wifi.s

		ets_delay_us( 1000 );

	}


	//Disable all interrupts.
	//ets_intr_lock();
	//__asm__ __volatile__("rsil %%0," __STRINGIFY(level) : "=a" (state));
	uint32_t interruptsState;
	__asm__ __volatile__("rsil %0,15" : "=a" (interruptsState));
	uart_tx_one_char( 'A' );
	LocalFlashRewrite( from1, to1, size1, from2, to2, size2, firstfour );

	return 0; //Will never get here.

//	 system_upgrade_reboot();
//	software_reset(); //Doesn't seem to boot back in.

}


