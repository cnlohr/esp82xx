//Unless what else is individually marked, all code in this file is
//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

#ifndef _ESP82XXUTIL_H
#define _ESP82XXUTIL_H


#include <mem.h>
#include <c_types.h>
#include <user_interface.h>
#include <ets_sys.h>
#include <espconn.h>
#include <c_types.h>
#include <stdio.h>
#include <esp8266_rom.h>


//XXX WARNING As of 1.3.0, "cansend" doesn't work.
//the SDK seems to misbehave when trying to send without a full
//response packet.
#define SAFESEND


#define HTONS(x) ((((uint16_t)(x))>>8)|(((x)&0xff)<<8))

#define IP4_to_uint32(x) (((uint32_t)x[3]<<24)|((uint32_t)x[2]<<16)|((uint32_t)x[1]<<8)|x[0])
#define uint32_to_IP4(x,y) {y[0] = (uint8_t)(x); y[1] = (uint8_t)((x)>>8); y[2] = (uint8_t)((x)>>16); y[3] = (uint8_t)((x)>>24);}

extern const char * enctypes[6];// = { "open", "wep", "wpa", "wpa2", "wpa_wpa2", 0 };

#define printf( ... ) { char generic_print_buffer[384]; ets_sprintf( generic_print_buffer, __VA_ARGS__ );  uart0_sendStr( generic_print_buffer ); }

char ICACHE_FLASH_ATTR tohex1( uint8_t i );
int8_t ICACHE_FLASH_ATTR fromhex1( char c ); //returns -1 if not hex char.

int32 ICACHE_FLASH_ATTR safe_atoi( const char * in ); //If valid number, paramcount increments

void ICACHE_FLASH_ATTR Uint32To10Str( char * out, uint32 dat );

void ICACHE_FLASH_ATTR NixNewline( char * str ); //If there's a newline at the end of this string, make it null.

//For holding TX packet buffers
extern char * generic_ptr;
int8_t ICACHE_FLASH_ATTR  TCPCanSend( struct espconn * conn, int size );
int8_t ICACHE_FLASH_ATTR  TCPDoneSend( struct espconn * conn );
void  ICACHE_FLASH_ATTR  EndTCPWrite( struct espconn * conn );


#define PushByte( c ) { *(generic_ptr++) = c; }

void ICACHE_FLASH_ATTR PushString( const char * buffer );
void ICACHE_FLASH_ATTR PushBlob( const uint8 * buffer, int len );

//Utility functions for dealing with packets on the stack.
#define START_PACK char generic_buffer[1500] __attribute__((aligned (32))); generic_ptr=generic_buffer;
#define PACK_LENGTH (generic_ptr-&generic_buffer[0])
#define END_TCP_WRITE( c ) if(generic_ptr!=generic_buffer) { int r = espconn_sent(c,generic_buffer,generic_ptr-generic_buffer);	}

//As much as it pains me, we shouldn't be using the esp8266's base64_encode() function
//as it does stuff with dynamic memory.
void ICACHE_FLASH_ATTR my_base64_encode(const unsigned char *data, size_t input_length, uint8_t * encoded_data );


void ICACHE_FLASH_ATTR SafeMD5Update( MD5_CTX * md5ctx, uint8_t*from, uint32_t size1 );

char * ICACHE_FLASH_ATTR strdup( const char * src );
char * ICACHE_FLASH_ATTR strdupcaselower( const char * src );

//data: Give pointer to tab-deliminated string.
//returns pointer to *data
//Searches for a \t in *data.  Once found, sets to 0, advances dat to next.
//I.e. this pops tabs off the beginning of a string efficiently.
//These are used in the flash re-writer and cannot be ICACHE_FLASH_ATTR'd out.
//WARNING: These functions are NOT threadsafe.
extern char * parameters;
extern uint8_t paramcount;
char *  ICACHE_FLASH_ATTR ParamCaptureAndAdvance( ); //Increments intcount if good.
int32_t ICACHE_FLASH_ATTR ParamCaptureAndAdvanceInt( ); //Do the same, but we're looking for an integer.

uint32_t ICACHE_FLASH_ATTR GetCurrentIP( );

#define PIN_OUT       ( *((uint32_t*)0x60000300) )
#define PIN_OUT_SET   ( *((uint32_t*)0x60000304) )
#define PIN_OUT_CLEAR ( *((uint32_t*)0x60000308) )
#define PIN_DIR       ( *((uint32_t*)0x6000030C) )
#define PIN_DIR_OUTPUT ( *((uint32_t*)0x60000310) )
#define PIN_DIR_INPUT ( *((uint32_t*)0x60000314) )
#define PIN_IN        ( *((volatile uint32_t*)0x60000318) )
#define _BV(x) ((1)<<(x))


//For newer SDKs
const unsigned char * ICACHE_FLASH_ATTR memchr(const unsigned char *s, int c, size_t n);

#endif
