/*
 * spi_memory_addrs.h
 *
 *  Created on: Nov 15, 2018
 *      Author: adam
 */

#ifndef SPI_MEMORY_ADDRS_H_
#define SPI_MEMORY_ADDRS_H_

/**
 * See SETTINGS_ADDR in commonservices.c and
 * SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR in user_main.c
 * system_param_load() requires three sectors (3 * 0x1000)
 */
#define COMMON_SERVICES_SETTINGS_ADDR 0x7C000
#define COMMON_SERVICES_SETTINGS_SIZE  0x3000
/**
 * Settings used in custom_commands.c. Comes 3 sectors after COMMON_SERVICES_SETTINGS_ADDR
 * Currently 0x7F000
 */
#define USER_SETTINGS_ADDR            (COMMON_SERVICES_SETTINGS_ADDR + COMMON_SERVICES_SETTINGS_SIZE)
#define USER_SETTINGS_SIZE            0x1000
/**
 * The webpage data is stored at an address, MFS_PAGE_OFFSET, declared in the
 * makefile, This is so the makefile can burn the file system at the correct
 * address. There should be no space between USER_DATA and MFS_PAGE_OFFSET, so
 * the preprocessor checks that
 */
#if MFS_PAGE_OFFSET == 0
#error FATAL ERROR: MFS_PAGE_OFFSET is 0.
#endif

#if (((USER_SETTINGS_ADDR + USER_SETTINGS_SIZE) != MFS_PAGE_OFFSET) && (0x10000 != MFS_PAGE_OFFSET))
#error "end of USER_DATA doesn't line up with MFS_PAGE_OFFSET from the makefile"
#endif

#endif /* SPI_MEMORY_ADDRS_H_ */
