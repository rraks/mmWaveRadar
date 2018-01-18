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
        uint32 i = 0;
        uint8 latency = 255;
	uint32 channels;
	uint8 inBuffer[763];
uint8 outBuffer[763] = "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of de Finibus Bonorum et Malorum (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, Lorem ipsum dolor sit amet.., comes from a line in section 1.10.32.";
	//	uint8 outBuffer[93] = "1abcdefghijklmnopqrstuvwxyzDONE2abcdefghijklmnopqrstuvwxyzDONE1abcdefghijklmnopqrstuvwxyzDONE";
	uint32 sizeTransferred;
	uint32 l = 0;
	useconds_t delay = 10;

        channelConf.ClockRate = 30000000;
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


while(1){

     status = SPI_ReadWrite(ftHandle, inBuffer, outBuffer, 700, &sizeTransferred,
     			SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES|
     			SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE|
     			SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE);



printf("status=0x%x sizeTransferred=%u\n", status, sizeTransferred);
                                for(l=0;l<sizeTransferred;l++)
                                        printf("%c",(unsigned)inBuffer[l]);
                                printf("\n");
}

}
