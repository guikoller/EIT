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

/* Definitions of physical drive number for each drive */
#define DEV_SD		0	/* Map SD card to physical drive 0 */


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
		if(BSP_SD_ReadBlocks((uint32_t*)buff, (uint32_t)sector, count, 10000) == MSD_OK)
		{
			res = RES_OK;
		}
		return res;
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
		if(BSP_SD_WriteBlocks((uint32_t*)buff, (uint32_t)sector, count, 10000) == MSD_OK)
		{
			res = RES_OK;
		}
		return res;
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
			res = RES_OK;
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
