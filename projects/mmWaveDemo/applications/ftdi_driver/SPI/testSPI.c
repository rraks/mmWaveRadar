#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include <unistd.h>

/* Include D2XX header*/
#include "ftd2xx.h"

#include "libMPSSE_spi.h"

#define SPI_DEVICE_BUFFER_SIZE          256


/* Handles */
static FT_HANDLE ftHandle;
static uint8 buffer[SPI_DEVICE_BUFFER_SIZE] = {0};



int main()
{
        FT_STATUS status = FT_OK;
        FT_DEVICE_LIST_INFO_NODE devList = {0};
        ChannelConfig channelConf = {0};
        uint8 i = 0;
        uint8 latency = 255;
	uint32 channels;
	uint8 inBuffer[100];
	uint8 outBuffer[93] = "1abcdefghijklmnopqrstuvwxyzDONE2abcdefghijklmnopqrstuvwxyzDONE1abcdefghijklmnopqrstuvwxyzDONE";
	uint8 sizeTransferred;
	uint8 l = 0;
	useconds_t delay = 100;

        channelConf.ClockRate = 4000000;
        channelConf.LatencyTimer = latency;
        channelConf.configOptions = SPI_CONFIG_OPTION_MODE0 | SPI_CONFIG_OPTION_CS_DBUS3 | SPI_CONFIG_OPTION_CS_ACTIVELOW;
        channelConf.Pin = 0x00000000;/*FinalVal-FinalDir-InitVal-InitDir (for dir 0=in, 1=out)*/

        /* init library */
#ifdef _MSC_VER
        Init_libMPSSE();
#endif
        status = SPI_GetNumChannels(&channels);
        printf("Number of available SPI channels = %d\n",(int)channels);
	
	status = SPI_OpenChannel(0,&ftHandle);
        printf("\nhandle=0x%x status=0x%x\n",(unsigned int)ftHandle,status);
        status = SPI_InitChannel(ftHandle,&channelConf);




     status = SPI_ReadWrite(ftHandle, inBuffer, outBuffer, 20, &sizeTransferred,
     			SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|
     			SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE|
     			SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);



printf("status=0x%x sizeTransferred=%u\n", status, sizeTransferred);
                                for(l=0;l<sizeTransferred;l++)
                                        printf("%c",(unsigned)inBuffer[l]);
                                printf("\n");
}


