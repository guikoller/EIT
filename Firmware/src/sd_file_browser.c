#include "sd_file_browser.h"
#include "stm32f769i_discovery_sd.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>

static lv_obj_t * text_area;
static char info_buffer[4096];
static FATFS SDFatFs;  /* File system object for SD card */
static uint8_t fs_mounted = 0;

/**
 * Initialize SD card
 */
int sd_card_init(void)
{
    uint8_t sd_state = BSP_SD_Init();
    
    if(sd_state != MSD_OK)
    {
        return -1;
    }
    
    // Try to mount the file system
    FRESULT res = f_mount(&SDFatFs, "0:", 1);
    if(res == FR_OK)
    {
        fs_mounted = 1;
    }
    else
    {
        fs_mounted = 0;
    }
    
    return 0;
}

/**
 * List files in root directory
 */
static void list_files(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    char temp[256];
    int file_count = 0;
    int dir_count = 0;
    
    if(!fs_mounted)
    {
        strcat(info_buffer, "\n\nFILE SYSTEM NOT MOUNTED\n");
        strcat(info_buffer, "=======================\n");
        strcat(info_buffer, "Unable to read file system.\n");
        strcat(info_buffer, "Card may not be formatted with FAT.\n");
        return;
    }
    
    strcat(info_buffer, "\n\nFILES AND DIRECTORIES\n");
    strcat(info_buffer, "=====================\n\n");
    
    res = f_opendir(&dir, "0:");
    if(res != FR_OK)
    {
        snprintf(temp, sizeof(temp), "Error opening directory: %d\n", res);
        strcat(info_buffer, temp);
        return;
    }
    
    for(;;)
    {
        res = f_readdir(&dir, &fno);
        if(res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
        
        if(fno.fattrib & AM_DIR)
        {
            /* Directory */
            snprintf(temp, sizeof(temp), "[DIR]  %s\n", fno.fname);
            strcat(info_buffer, temp);
            dir_count++;
        }
        else
        {
            /* File */
            snprintf(temp, sizeof(temp), "       %s (%lu bytes)\n", fno.fname, fno.fsize);
            strcat(info_buffer, temp);
            file_count++;
        }
        
        /* Check if buffer is getting full */
        if(strlen(info_buffer) > sizeof(info_buffer) - 300)
        {
            strcat(info_buffer, "\n... (truncated, buffer full)\n");
            break;
        }
    }
    
    f_closedir(&dir);
    
    snprintf(temp, sizeof(temp), "\nTotal: %d files, %d directories\n", 
             file_count, dir_count);
    strcat(info_buffer, temp);
}

/**
 * Get SD card information and format as string
 */
static void get_sd_card_info(void)
{
    HAL_SD_CardInfoTypeDef card_info;
    
    memset(info_buffer, 0, sizeof(info_buffer));
    
    // BSP_SD_GetCardInfo returns void, so just call it
    BSP_SD_GetCardInfo(&card_info);
    
    // Calculate capacity in MB
    uint32_t capacity_mb = (uint32_t)((uint64_t)card_info.LogBlockNbr * 
                                      (uint64_t)card_info.LogBlockSize / 
                                      (1024 * 1024));
    
    snprintf(info_buffer, sizeof(info_buffer),
            "SD CARD INFORMATION\n"
            "===================\n\n"
            "Card Type: ");
    
    // Add card type
    switch(card_info.CardType)
    {
        case CARD_SDSC:
            strcat(info_buffer, "SDSC (Standard Capacity)\n");
            break;
        case CARD_SDHC_SDXC:
            strcat(info_buffer, "SDHC/SDXC (High/Extended Capacity)\n");
            break;
        case CARD_SECURED:
            strcat(info_buffer, "Secured Card\n");
            break;
        default:
            strcat(info_buffer, "Unknown\n");
            break;
    }
    
    // Add card version
    char temp[128];
    snprintf(temp, sizeof(temp), "Card Version: %lu.%lu\n",
            (card_info.CardVersion >> 8) & 0xFF,
            card_info.CardVersion & 0xFF);
    strcat(info_buffer, temp);
    
   
    // Add capacity information
    snprintf(temp, sizeof(temp), "\nCapacity: %lu MB\n", capacity_mb);
    strcat(info_buffer, temp);
    
    snprintf(temp, sizeof(temp), "Block Size: %lu bytes\n", card_info.LogBlockSize);
    strcat(info_buffer, temp);
    
    snprintf(temp, sizeof(temp), "Block Count: %lu\n", card_info.LogBlockNbr);
    strcat(info_buffer, temp);
    
    // Get volume label and free space if mounted
    if(fs_mounted)
    {
        char label[24];
        DWORD fre_clust, fre_sect;
        FATFS *fs;
        
        strcat(info_buffer, "\nFILE SYSTEM\n");
        strcat(info_buffer, "===========\n");
        
        if(f_getlabel("0:", label, NULL) == FR_OK)
        {
            if(label[0] == 0)
            {
                strcat(info_buffer, "Label: (none)\n");
            }
            else
            {
                snprintf(temp, sizeof(temp), "Label: %s\n", label);
                strcat(info_buffer, temp);
            }
        }
        
        if(f_getfree("0:", &fre_clust, &fs) == FR_OK)
        {
            DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
            fre_sect = fre_clust * fs->csize;
            
            snprintf(temp, sizeof(temp), "Total: %lu MB\n", tot_sect / 2048);
            strcat(info_buffer, temp);
            snprintf(temp, sizeof(temp), "Free: %lu MB\n", fre_sect / 2048);
            strcat(info_buffer, temp);
        }
    }
    
    // List files
    list_files();
}

/**
 * Button callback to refresh SD card info
 */
static void refresh_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED)
    {
        get_sd_card_info();
        lv_textarea_set_text(text_area, info_buffer);
    }
}

/**
 * Create SD card file browser UI
 */
void sd_file_browser_create(void)
{
    // Create main container
    lv_obj_t * cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(cont);
    
    // Create title label
    lv_obj_t * title = lv_label_create(cont);
    lv_label_set_text(title, "SD Card Browser");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create refresh button
    lv_obj_t * btn = lv_button_create(cont);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_obj_add_event_cb(btn, refresh_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Refresh");
    lv_obj_center(btn_label);
    
    // Create text area for displaying information
    text_area = lv_textarea_create(cont);
    lv_obj_set_size(text_area, LV_HOR_RES - 40, LV_VER_RES - 80);
    lv_obj_align(text_area, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_textarea_set_text(text_area, "Initializing SD Card...\n");
    
    // Get initial SD card information
    get_sd_card_info();
    lv_textarea_set_text(text_area, info_buffer);
}
