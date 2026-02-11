/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "stm32f769i_discovery_sd.h"

#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"

#include <string.h>
#include <stdint.h>

/* Definitions of physical drive number for each drive */
#define DEV_SD		0	/* Map SD card to physical drive 0 */

#define SECTOR_SIZE_BYTES 512u

static uint8_t s_sector_bounce[SECTOR_SIZE_BYTES] __attribute__((aligned(32)));

static void dcache_clean_by_addr(const void *addr, uint32_t len)
{
	if ((SCB->CCR & (uint32_t)SCB_CCR_DC_Msk) == 0u) {
		return;
	}

	uintptr_t start = ((uintptr_t)addr) & ~((uintptr_t)31u);
	uintptr_t end = ((uintptr_t)addr + (uintptr_t)len + (uintptr_t)31u) & ~((uintptr_t)31u);
	SCB_CleanDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
}

static void dcache_invalidate_by_addr(void *addr, uint32_t len)
{
	if ((SCB->CCR & (uint32_t)SCB_CCR_DC_Msk) == 0u) {
		return;
	}

	uintptr_t start = ((uintptr_t)addr) & ~((uintptr_t)31u);
	uintptr_t end = ((uintptr_t)addr + (uintptr_t)len + (uintptr_t)31u) & ~((uintptr_t)31u);
	SCB_InvalidateDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
}

static int sd_wait_ready(uint32_t timeout_ms)
{
	uint32_t start = HAL_GetTick();
	while (BSP_SD_GetCardState() != SD_TRANSFER_OK) {
		if ((HAL_GetTick() - start) >= timeout_ms) {
			return 0;
		}
	}
	return 1;
}


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	DSTATUS stat = 0;

	switch (pdrv) {
	case DEV_SD :
		if(BSP_SD_IsDetected() != SD_PRESENT)
		{
			stat = STA_NODISK;
		}
		return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Initialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	DSTATUS stat = 0;

	switch (pdrv) {
	case DEV_SD :
		if(BSP_SD_Init() != MSD_OK)
		{
			stat = STA_NOINIT;
		}
		return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res = RES_ERROR;

	switch (pdrv) {
	case DEV_SD :
		if (count == 0) return RES_PARERR;

		for (UINT i = 0; i < count; i++) {
			if (!sd_wait_ready(1000)) {
				return RES_ERROR;
			}
			if (BSP_SD_ReadBlocks((uint32_t*)s_sector_bounce, (uint32_t)(sector + i), 1, 10000) != MSD_OK) {
				return RES_ERROR;
			}
			dcache_invalidate_by_addr(s_sector_bounce, SECTOR_SIZE_BYTES);
			memcpy(buff + (i * SECTOR_SIZE_BYTES), s_sector_bounce, SECTOR_SIZE_BYTES);
		}
		return RES_OK;
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res = RES_ERROR;

	switch (pdrv) {
	case DEV_SD :
		if (count == 0) return RES_PARERR;

		for (UINT i = 0; i < count; i++) {
			if (!sd_wait_ready(1000)) {
				return RES_ERROR;
			}
			memcpy(s_sector_bounce, buff + (i * SECTOR_SIZE_BYTES), SECTOR_SIZE_BYTES);
			dcache_clean_by_addr(s_sector_bounce, SECTOR_SIZE_BYTES);
			if (BSP_SD_WriteBlocks((uint32_t*)s_sector_bounce, (uint32_t)(sector + i), 1, 10000) != MSD_OK) {
				return RES_ERROR;
			}
		}
		return RES_OK;
	}

	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res = RES_ERROR;
	HAL_SD_CardInfoTypeDef CardInfo;

	switch (pdrv) {
	case DEV_SD :
		switch (cmd) {
		case CTRL_SYNC :
			res = sd_wait_ready(1000) ? RES_OK : RES_ERROR;
			break;

		case GET_SECTOR_COUNT :
			BSP_SD_GetCardInfo(&CardInfo);
			*(LBA_t*)buff = CardInfo.LogBlockNbr;
			res = RES_OK;
			break;

		case GET_SECTOR_SIZE :
			BSP_SD_GetCardInfo(&CardInfo);
			*(WORD*)buff = CardInfo.LogBlockSize;
			res = RES_OK;
			break;

		case GET_BLOCK_SIZE :
			*(DWORD*)buff = 1;  /* Erase block size in units of sector */
			res = RES_OK;
			break;

		default:
			res = RES_PARERR;
		}
		return res;
	}

	return RES_PARERR;
}
