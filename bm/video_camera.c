/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "usb_device_video.h"

#include "usb_device_ch9.h"
#include "usb_device_descriptor.h"

#include "video_camera.h"

#include "flexio_ov7670.h"

#include "fsl_device_registers.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_debug_console.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
#include "usb_phy.h"
#endif

#include "pin_mux.h"
#include "fsl_port.h"
#include <stdbool.h>
/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
extern void FLEXIO_Ov7670AsynCallback(void);
void BOARD_InitHardware(void);

static void USB_DeviceVideoPrepareVideoData(void);
static usb_status_t USB_DeviceVideoIsoIn(usb_device_handle deviceHandle,
                                         usb_device_endpoint_callback_message_struct_t *event,
                                         void *arg);
static void USB_DeviceVideoApplicationSetDefault(void);
static usb_status_t USB_DeviceVideoProcessClassVsProbeRequest(usb_device_handle handle,
                                                              usb_setup_struct_t *setup,
                                                              uint32_t *length,
                                                              uint8_t **buffer);
static usb_status_t USB_DeviceVideoProcessClassVsCommitRequest(usb_device_handle handle,
                                                               usb_setup_struct_t *setup,
                                                               uint32_t *length,
                                                               uint8_t **buffer);
static usb_status_t USB_DeviceVideoProcessClassVsStillProbeRequest(usb_device_handle handle,
                                                                   usb_setup_struct_t *setup,
                                                                   uint32_t *length,
                                                                   uint8_t **buffer);
static usb_status_t USB_DeviceVideoProcessClassVsStillCommitRequest(usb_device_handle handle,
                                                                    usb_setup_struct_t *setup,
                                                                    uint32_t *length,
                                                                    uint8_t **buffer);
static usb_status_t USB_DeviceVideoProcessClassVsStillImageTriggerRequest(usb_device_handle handle,
                                                                          usb_setup_struct_t *setup,
                                                                          uint32_t *length,
                                                                          uint8_t **buffer);
static usb_status_t USB_DeviceVideoProcessClassVsRequest(usb_device_handle handle,
                                                         usb_setup_struct_t *setup,
                                                         uint32_t *length,
                                                         uint8_t **buffer);
static usb_status_t USB_DeviceProcessClassVcInterfaceRequest(usb_device_handle handle,
                                                             usb_setup_struct_t *setup,
                                                             uint32_t *length,
                                                             uint8_t **buffer);
static usb_status_t USB_DeviceProcessClassVcCameraTerminalRequest(usb_device_handle handle,
                                                                  usb_setup_struct_t *setup,
                                                                  uint32_t *length,
                                                                  uint8_t **buffer);
static usb_status_t USB_DeviceProcessClassVcInputTerminalRequest(usb_device_handle handle,
                                                                 usb_setup_struct_t *setup,
                                                                 uint32_t *length,
                                                                 uint8_t **buffer);
static usb_status_t USB_DeviceProcessClassVcOutputTerminalRequest(usb_device_handle handle,
                                                                  usb_setup_struct_t *setup,
                                                                  uint32_t *length,
                                                                  uint8_t **buffer);
static usb_status_t USB_DeviceProcessClassVcProcessingUintRequest(usb_device_handle handle,
                                                                  usb_setup_struct_t *setup,
                                                                  uint32_t *length,
                                                                  uint8_t **buffer);
static usb_status_t USB_DeviceProcessClassVcRequest(usb_device_handle handle,
                                                    usb_setup_struct_t *setup,
                                                    uint32_t *length,
                                                    uint8_t **buffer);
static void USB_DeviceApplicationInit(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

usb_video_flexio_camera_struct_t g_UsbDeviceVideoFlexioCamera;

extern const unsigned char g_UsbDeviceVideoMjpegData[];
extern const uint32_t g_UsbDeviceVideoMjpegLength;
extern uint8_t g_UsbDeviceCurrentConfigure;
extern uint8_t g_UsbDeviceInterface[USB_VIDEO_CAMERA_INTERFACE_COUNT];
extern volatile uint8_t g_FlexioCameraFrameBuffer[2][OV7670_FRAME_BYTES + 32];

/*******************************************************************************
 * Code
 ******************************************************************************/



void paintXY(volatile uint8_t *buffer,uint8_t x,uint8_t y,uint8_t r,uint8_t g,uint8_t b)
{
	int cor = (y*160 + x)*2;
	buffer[cor] = (b&0x1F) + (g<<5);
	buffer[cor+1] = (g>>3) +(r<<3);

/*	int cor = (y*160 + x)*2;
	buffer[cor] = (r<<3) + (g>>3);
	buffer[cor+1] = (g<<5) +b;*/


}


//char TO_ASCII[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
char *TO_ASCII[16] = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};


void paintSquare(volatile uint8_t *buffer,int startx, int starty, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
	int x,y;
	for (x=startx;x<startx+width;x++)
	{
		for (y=starty;y<starty+height;y++)
		{
			paintXY(buffer,x,y,r,g,b);
		}
	}
}

void paintSymbols(volatile uint8_t *buffer,int left,int stop,int right,int state)
{
	if (left) {
		paintSquare(buffer,60,90,8,8,0,63,0);
	}
	if (stop) {
		paintSquare(buffer,80,90,8,8,31,0,0);
	}
	if (right) {
		paintSquare(buffer,100,90,8,8,0,63,0);
	}

	int i,x;

	for (i=0;i<15;i++)
	{
		x = 10+i*10;
		if (state==i) {
			paintSquare(buffer,x,110,6,6,0,63,0);
		} else {
			paintSquare(buffer,x,110,6,6,20,40,20);
		}
	}

}


void paintLines(void)
{
	int i;
	for (i=0;i<100;i++) {
		paintXY(g_FlexioCameraFrameBuffer[0]+32,80,i,31,0,0);
		paintXY(g_FlexioCameraFrameBuffer[1]+32,80,i,31,0,0);

		paintXY(g_FlexioCameraFrameBuffer[0]+32,90,i,0,0x3F,0);
		paintXY(g_FlexioCameraFrameBuffer[1]+32,90,i,0,0x3F,0);

		paintXY(g_FlexioCameraFrameBuffer[0]+32,100,i,0,0,31);
		paintXY(g_FlexioCameraFrameBuffer[1]+32,100,i,0,0,31);

	}
}


void paintLineX(volatile uint8_t *buffer,int xcor)
{
	int i;
	for (i=0;i<120;i++) {
		paintXY(buffer,xcor,i,31,0,0);
		paintXY(buffer,xcor,i,31,0,0);
	}
}

void paintLineY(volatile uint8_t *buffer,int ycor)
{
	int i;
	for (i=0;i<160;i++) {
		paintXY(buffer,i,ycor,31,0,0);
		paintXY(buffer,i,ycor,31,0,0);
	}
}



void convertToRGB(volatile uint8_t *buffer)
{
	int byte;
	uint8_t Y = 0;
	for (byte=0;byte<OV7670_FRAME_BYTES;byte+=2)
	{
		Y = buffer[byte+1];
		buffer[byte] = Y;//(Y>>3);
		buffer[byte+1] = 0;

		//buffer[byte] = (Y>>3) + (Y<<3);
		//buffer[byte+1] = (Y>>3) + (Y<<3);
	}
}


void convertToRGBTest(volatile uint8_t *buffer)
{
	int byte;
	uint8_t Y = 0;
	int limit = 160*60*2;
	for (byte=0;byte<limit;byte+=2)
	{
		Y = buffer[byte];
		buffer[byte] = (Y>>3);
		buffer[byte+1] = 0;

		buffer[byte] = (Y>>3) | (Y<<5);
		buffer[byte+1] = (Y>>3) | (Y<<3);

		//buffer[byte] = (Y>>3) + (Y<<3);
		//buffer[byte+1] = (Y>>3) + (Y<<3);
	}
}

void convertToRGBTest2(volatile uint8_t *buffer)
{
	int byte;
	int limit = 160*40*2;
	for (byte=0;byte<limit;byte+=2)
	{
		buffer[byte] = 0;
		buffer[byte+1] = 0;

		//buffer[byte] = (Y>>3) + (Y<<3);
		//buffer[byte+1] = (Y>>3) + (Y<<3);
	}
}

void testColorDetect(volatile uint8_t *buffer)
{
	int byte;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		uint8_t b = buffer[byte] & 0x1F;
		uint8_t g = ((buffer[byte] & 0xE0)>>5)|((buffer[byte+1] & 0x7)<<3);
		uint8_t r = (buffer[byte+1]) >> 3;

		if ((r>16) && (g<32) & (b<16)) {
			buffer[byte] = 0xFF;
			buffer[byte+1] = 0xFF;

		}
	}
}

/* Not working, too slow */
void RGB2BW_RGB(volatile uint8_t *buffer)
{
	int byte;
	uint8_t Y = 0;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		uint8_t b = buffer[byte] & 0x1F;
		uint8_t g = ((buffer[byte] & 0xE0)>>5)|((buffer[byte+1] & 0x7)<<3);
		uint8_t r = (buffer[byte+1]) >> 3;

		Y = 0.59 *g + 0.31*r + 0.11*b;

		buffer[byte] = (Y&0x1F) + (Y<<5);
		buffer[byte+1] = (Y>>3) +(Y<<3);
	}
}


/* Not working, too slow */
void RGB2BW_RGB2(volatile uint8_t *buffer)
{
	int byte;
	uint16_t Y = 0;
	uint8_t Y5 = 0;
	uint8_t Y6 = 0;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		uint8_t b = buffer[byte] & 0x1F;
		uint8_t g = ((buffer[byte] & 0xE0)>>5)|((buffer[byte+1] & 0x7)<<3);
		uint8_t r = (buffer[byte+1]) >> 3;

		Y = g + r + b;
		Y5 = Y >> 11;
		Y6 = Y >> 10;


		buffer[byte] = Y5 + (Y6<<5);
		buffer[byte+1] = (Y6>>3) +(Y5<<3);
	}
}


// Working function that convert YUV Intensity to RGB B&W Image
void convertYUVToBW_RGB(volatile uint8_t *buffer)
{
	uint8_t Y5 = 0;
	uint8_t Y6 = 0;
	int byte;
	uint8_t Y = 0;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		Y = buffer[byte+1];
		Y5 = Y >> 3;
		Y6 = Y >> 2;
		buffer[byte] = Y5 + (Y6<<5);
		buffer[byte+1] = (Y6>>3) +(Y5<<3);
	}
}


// This flag indicates if processed images are modified to show processing
#define MODIFY_IMAGE
// Change in image pixel to consider movement - for noise filtering
#define MOVEMENT_THRESHOLD 10
// Size of kernel for pixel belonging to object detection
#define OBJECT_KERNEL_SIZE 2

// Size of image
#define IMAGE_X_SIZE 160
#define IMAGE_Y_SIZE 120

uint8_t gPrevImage[19200];





void detectMovement(volatile uint8_t *buffer1,volatile uint8_t *buffer2)
{
	if ((buffer1[0]==0xAA)&&(buffer1[1]==0x55)&&(buffer1[2]==0xAA)&&(buffer1[3]==0x55)&&
		(buffer1[38399]==0xAA)&&(buffer1[38398]==0x55)&&(buffer1[38397]==0xAA)&&(buffer1[38396]==0x55))
	{
		return;
	}

	int i=0;
	uint8_t Y5 = 0;
	uint8_t Y6 = 0;
	int byte;
	uint8_t Y = 0;
	int sub;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		//Y = buffer1[byte+1] - buffer2[byte+1];
		//Y = buffer1[byte+1] - prevImage[i];// - buffer2[byte+1];
		sub = buffer1[byte+1] - gPrevImage[i];// - buffer2[byte+1];
		if (sub<0) {
			Y = -sub;
		} else {
			Y = sub;
		}
		//Y = prevImage[i];// - buffer2[byte+1];
		gPrevImage[i]=buffer1[byte+1];
		Y5 = Y >> 3;
		Y6 = Y >> 2;
		buffer1[byte] = Y5 + (Y6<<5);
		buffer1[byte+1] = (Y6>>3) +(Y5<<3);
		i++;
	}
	buffer1[0]=0xAA;
	buffer1[1]=0x55;
	buffer1[2]=0xAA;
	buffer1[3]=0x55;
	buffer1[38399]=0xAA;
	buffer1[38398]=0x55;
	buffer1[38397]=0xAA;
	buffer1[38396]=0x55;

}



void detectMovement2(volatile uint8_t *buffer1,volatile uint8_t *buffer2)
{
	if ((buffer1[0]==0xAA)&&(buffer1[1]==0x55)&&(buffer1[2]==0xAA)&&(buffer1[3]==0x55)&&
		(buffer1[38399]==0xAA)&&(buffer1[38398]==0x55)&&(buffer1[38397]==0xAA)&&(buffer1[38396]==0x55))
	{
		return;
	}

	int i=0;
	uint8_t Y5 = 0;
	uint8_t Y6 = 0;
	int byte;
	uint8_t Y = 0;
	int sub;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		Y = buffer1[byte+1];
		sub = buffer1[byte+1] - gPrevImage[i];// - buffer2[byte+1];
		gPrevImage[i]=buffer1[byte+1];

		if ((sub>MOVEMENT_THRESHOLD)||(sub<-MOVEMENT_THRESHOLD))
		{
			buffer1[byte] = 0;
			buffer1[byte+1] = 0xF8;
		} else {
			Y5 = Y >> 3;
			Y6 = Y >> 2;
			buffer1[byte] = Y5 + (Y6<<5);
			buffer1[byte+1] = (Y6>>3) +(Y5<<3);

		}
		//Y = prevImage[i];// - buffer2[byte+1];
/*
		Y5 = Y >> 3;
		Y6 = Y >> 2;
		buffer1[byte] = Y5 + (Y6<<5);
		buffer1[byte+1] = (Y6>>3) +(Y5<<3);*/
		i++;
	}


	buffer1[0]=0xAA;
	buffer1[1]=0x55;
	buffer1[2]=0xAA;
	buffer1[3]=0x55;
	buffer1[38399]=0xAA;
	buffer1[38398]=0x55;
	buffer1[38397]=0xAA;
	buffer1[38396]=0x55;

}





void convert2RGBandSavePrevious(volatile uint8_t *buffer)
{

	int i=0;
	uint8_t Y5 = 0;
	uint8_t Y6 = 0;
	int byte;
	uint8_t Y = 0;
	int limit = 160*120*2;
	for (byte=0;byte<limit;byte+=2)
	{
		Y = buffer[byte+1];
		gPrevImage[i]=buffer[byte+1];

		Y5 = Y >> 3;
		Y6 = Y >> 2;
		buffer[byte] = Y5 + (Y6<<5);
		buffer[byte+1] = (Y6>>3) +(Y5<<3);

		i++;
	}

}




// Function that locates a moving object in the camera frame
// Inputs:
//    buffer: Image in YUV format
//    direction: 1 left to right -1 right to left
// Outputs:
//    xcor: xcoordinate of moving object if -1 means not changed or unabled to retrieve
//    ycor: y coordinate of moving object if -1 means not changed or unabled to retrieve
void locateMovingObject(volatile uint8_t *buffer,int direction,int *pxcor,int *pycor)
{
	if ((buffer[0]==0xAA)&&(buffer[1]==0x55)&&(buffer[2]==0xAA)&&(buffer[3]==0x55)&&
		(buffer[38399]==0xAA)&&(buffer[38398]==0x55)&&(buffer[38397]==0xAA)&&(buffer[38396]==0x55))
	{
		*pxcor = -1;
		*pycor = -1;
		return;
	}

/*
	// This flag indicates if processed images are modified to show processing
	#define MODIFY_IMAGE
	// Change in image pixel to consider movement - for noise filtering
	#define MOVEMENT_THRESHOLD 20
	// Size of kernel for pixel belonging to object detection
	#define MIN_PROX_FOR_OBJECT 2
*/

	int index = 0;
	int prevIndex = 0;
	*pxcor = -1;
	*pycor = -1;
	int x,y;
	int sub;

	for (x=10;x<(IMAGE_X_SIZE - OBJECT_KERNEL_SIZE);x++)
	{
		for(y=10;y<(IMAGE_Y_SIZE - OBJECT_KERNEL_SIZE);y++)
		{
			prevIndex = (y*IMAGE_X_SIZE + x);
			index = (prevIndex<<1) +1;
			sub = buffer[index] - gPrevImage[prevIndex];
			if ((sub>MOVEMENT_THRESHOLD)||(sub<-MOVEMENT_THRESHOLD))
			{
				//usb_echo("MY sub: %d bufferI: %d prevImageI: %d\r\n",sub,buffer[index], gPrevImage[prevIndex]);
				usb_echo("MY sub: %d bufferI: %d prevImageI: %d\r\n",sub,index,prevIndex);
				*pxcor = x;
				break;
			}
		}
		if (*pxcor > -1) break;
	}

	for(y=10;y<(IMAGE_Y_SIZE - OBJECT_KERNEL_SIZE);y++)
	{
		for (x=10;x<(IMAGE_X_SIZE - OBJECT_KERNEL_SIZE);x++)
		{
			prevIndex = (y*IMAGE_X_SIZE + x);
			index = (prevIndex<<1)+1;
			sub = buffer[index] - gPrevImage[prevIndex];
			if ((sub>MOVEMENT_THRESHOLD)||(sub<-MOVEMENT_THRESHOLD))
			{
				usb_echo("MX");
				*pycor = y;
				break;
			}
		}
		if (*pycor > -1) break;
	}


	convert2RGBandSavePrevious(buffer);

	buffer[0]=0xAA;
	buffer[1]=0x55;
	buffer[2]=0xAA;
	buffer[3]=0x55;
	buffer[38399]=0xAA;
	buffer[38398]=0x55;
	buffer[38397]=0xAA;
	buffer[38396]=0x55;

}



void callProgramWithNewImage(volatile uint8_t *buffer)
{
	if ((buffer[0]==0xAA)&&(buffer[1]==0x55)&&(buffer[2]==0xAA)&&(buffer[3]==0x55)&&
		(buffer[38399]==0xAA)&&(buffer[38398]==0x55)&&(buffer[38397]==0xAA)&&(buffer[38396]==0x55))
	{
		return;
	}

	runParkingAssistant(buffer);


	buffer[0]=0xAA;
	buffer[1]=0x55;
	buffer[2]=0xAA;
	buffer[3]=0x55;
	buffer[38399]=0xAA;
	buffer[38398]=0x55;
	buffer[38397]=0xAA;
	buffer[38396]=0x55;

}






void PORTB_IRQHandler(void)
{
    /* Clear the interrupt flag for PTB2 */
    PORT_ClearPinsInterruptFlags(PORTB, 1U << BOARD_FLEXIO_VSYNC_PIN_INDEX);
    FLEXIO_Ov7670AsynCallback();
/*    usb_echo("*");
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][33] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][33] & 0x0F]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][32] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][32] & 0x0F]);
    usb_echo("-");
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][35] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][35] & 0x0F]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][34] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][34] & 0x0F]);
    usb_echo("\r\n");
*/
/*
    usb_echo("*");
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19361] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19361] & 0x0F]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19360] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19360] & 0x0F]);
    usb_echo("-");
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19363] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19363] & 0x0F]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19362] >> 4]);
    usb_echo(TO_ASCII[g_FlexioCameraFrameBuffer[0][19362] & 0x0F]);
    usb_echo("\r\n");
*/


    //convertToRGB(g_FlexioCameraFrameBuffer[0]+32);
    //convertToRGB(g_FlexioCameraFrameBuffer[1]+32);
    //convertToRGBTest(g_FlexioCameraFrameBuffer[0]+32);
    //convertToRGBTest(g_FlexioCameraFrameBuffer[1]+32);

    //testColorDetect(g_FlexioCameraFrameBuffer[0]+32);
    //testColorDetect(g_FlexioCameraFrameBuffer[1]+32);

    //RGB2BW_RGB2(g_FlexioCameraFrameBuffer[0]+32);
    //RGB2BW_RGB2(g_FlexioCameraFrameBuffer[1]+32);

    //convertToRGBTest3(g_FlexioCameraFrameBuffer[0]+32);
    //convertToRGBTest3(g_FlexioCameraFrameBuffer[1]+32);

    // This didn't work. Images contain non converted areas
    //convertToRGBTest3(g_FlexioCameraFrameBuffer[g_UsbDeviceVideoFlexioCamera.fullBufferIndex]+32);

/*    if (g_UsbDeviceVideoFlexioCamera.fullBufferIndex==0) {
    	convertYUVToBW_RGB(g_FlexioCameraFrameBuffer[1]+32);
    } else {
    	convertYUVToBW_RGB(g_FlexioCameraFrameBuffer[0]+32);
    }
*/


/*    // Last Known working to detect movement
	if (g_UsbDeviceVideoFlexioCamera.fullBufferIndex==0) {
		detectMovement2(g_FlexioCameraFrameBuffer[1]+32,g_FlexioCameraFrameBuffer[0]+32);
	} else {
		detectMovement2(g_FlexioCameraFrameBuffer[0]+32,g_FlexioCameraFrameBuffer[1]+32);
	}*/


    int xcor,ycor;

    static int prevxcor=0;
    static int prevycor=0;

    volatile uint8_t *buffer;
	if (g_UsbDeviceVideoFlexioCamera.fullBufferIndex==0) {
		buffer = g_FlexioCameraFrameBuffer[1]+32;
	} else {
		buffer = g_FlexioCameraFrameBuffer[0]+32;
	}



	callProgramWithNewImage(buffer);

/*
	locateMovingObject(buffer,0,&xcor,&ycor);
	if (xcor>0) {
		prevxcor = xcor;
	}
	if (ycor>0) {
		prevycor = ycor;
	}
	paintLineX(buffer,prevxcor);
	paintLineY(buffer,prevycor);

*/


/*
    usb_echo(TO_ASCII[g_UsbDeviceVideoFlexioCamera.fullBufferIndex & 0x0F]);
    if ((g_FlexioCameraFrameBuffer[0][32] == 0xCA) &&
    	(g_FlexioCameraFrameBuffer[0][33] == 0xFE) &&
		(g_FlexioCameraFrameBuffer[0][34] == 0xBA) &&
		(g_FlexioCameraFrameBuffer[0][35] == 0xBE)) {

    	usb_echo(" B0 SD");
    }

    if ((g_FlexioCameraFrameBuffer[0][38428] == 0xCA) &&
    	(g_FlexioCameraFrameBuffer[0][38429] == 0xFE) &&
		(g_FlexioCameraFrameBuffer[0][38430] == 0xBA) &&
		(g_FlexioCameraFrameBuffer[0][38431] == 0xBE)) {

    	usb_echo(" B0 FD");
    }

    if ((g_FlexioCameraFrameBuffer[1][32] == 0xCA) &&
    	(g_FlexioCameraFrameBuffer[1][33] == 0xFE) &&
		(g_FlexioCameraFrameBuffer[1][34] == 0xBA) &&
		(g_FlexioCameraFrameBuffer[1][35] == 0xBE)) {

    	usb_echo(" B1 SD");

    }
    if ((g_FlexioCameraFrameBuffer[1][38428] == 0xCA) &&
    	(g_FlexioCameraFrameBuffer[1][38429] == 0xFE) &&
		(g_FlexioCameraFrameBuffer[1][38430] == 0xBA) &&
		(g_FlexioCameraFrameBuffer[1][38431] == 0xBE)) {

    	usb_echo(" B1 FD");
    }

	usb_echo("\r\n");



    if (g_UsbDeviceVideoFlexioCamera.fullBufferIndex==0) {
    	g_FlexioCameraFrameBuffer[1][32] = 0xCA;
    	g_FlexioCameraFrameBuffer[1][33] = 0xFE;
    	g_FlexioCameraFrameBuffer[1][34] = 0xBA;
    	g_FlexioCameraFrameBuffer[1][35] = 0xBE;

    	g_FlexioCameraFrameBuffer[1][38428] = 0xCA;
    	g_FlexioCameraFrameBuffer[1][38429] = 0xFE;
    	g_FlexioCameraFrameBuffer[1][38430] = 0xBA;
    	g_FlexioCameraFrameBuffer[1][38431] = 0xBE;
   } else {
    	g_FlexioCameraFrameBuffer[0][32] = 0xCA;
    	g_FlexioCameraFrameBuffer[0][33] = 0xFE;
    	g_FlexioCameraFrameBuffer[0][34] = 0xBA;
    	g_FlexioCameraFrameBuffer[0][35] = 0xBE;

    	g_FlexioCameraFrameBuffer[0][38428] = 0xCA;
    	g_FlexioCameraFrameBuffer[0][38429] = 0xFE;
    	g_FlexioCameraFrameBuffer[0][38430] = 0xBA;
    	g_FlexioCameraFrameBuffer[0][38431] = 0xBE;
    }
*/


//	paintSymbols(buffer,1,1,1,10);

    usb_echo(TO_ASCII[g_UsbDeviceVideoFlexioCamera.fullBufferIndex & 0x0F]);
	usb_echo("\r\n");

    //paintLines();

}

/* Prepare next transfer payload */
static void USB_DeviceVideoPrepareVideoData(void)
{
    usb_device_video_mjpeg_payload_header_struct_t *payloadHeader;
    uint32_t maxPacketSize;
    uint32_t i;
    uint32_t sendLength;

    g_UsbDeviceVideoFlexioCamera.currentTime += 10000U;

    if (g_UsbDeviceVideoFlexioCamera.imagePosition < OV7670_FRAME_BYTES)
    {
        i = (uint32_t)&g_FlexioCameraFrameBuffer[g_UsbDeviceVideoFlexioCamera.fullBufferIndex]
                                                [g_UsbDeviceVideoFlexioCamera.imagePosition -
                                                 sizeof(usb_device_video_mjpeg_payload_header_struct_t) + 32];
        g_UsbDeviceVideoFlexioCamera.imageBuffer = (uint8_t *)i;
    }
    else
    {
        g_UsbDeviceVideoFlexioCamera.imageBuffer = (uint8_t *)&g_UsbDeviceVideoFlexioCamera.image_header[0];
    }
    payloadHeader = (usb_device_video_mjpeg_payload_header_struct_t *)g_UsbDeviceVideoFlexioCamera.imageBuffer;
    for (i = 0; i < sizeof(usb_device_video_mjpeg_payload_header_struct_t); i++)
    {
        g_UsbDeviceVideoFlexioCamera.imageBuffer[i] = 0x00U;
    }
    payloadHeader->bHeaderLength = sizeof(usb_device_video_mjpeg_payload_header_struct_t);
    payloadHeader->headerInfoUnion.bmheaderInfo = 0U;
    payloadHeader->headerInfoUnion.headerInfoBits.frameIdentifier = g_UsbDeviceVideoFlexioCamera.currentFrameId;
    g_UsbDeviceVideoFlexioCamera.imageBufferLength = sizeof(usb_device_video_mjpeg_payload_header_struct_t);

    if (g_UsbDeviceVideoFlexioCamera.stillImageTransmission)
    {
        payloadHeader->headerInfoUnion.headerInfoBits.stillImage = 1U;
        maxPacketSize = g_UsbDeviceVideoFlexioCamera.stillCommitStruct.dwMaxPayloadTransferSize;
    }
    else
    {
        maxPacketSize = g_UsbDeviceVideoFlexioCamera.commitStruct.dwMaxPayloadTransferSize;
    }
    maxPacketSize = maxPacketSize - sizeof(usb_device_video_mjpeg_payload_header_struct_t);
    if (g_UsbDeviceVideoFlexioCamera.waitForNewInterval)
    {
        if (g_UsbDeviceVideoFlexioCamera.currentTime < g_UsbDeviceVideoFlexioCamera.commitStruct.dwFrameInterval)
        {
            return;
        }
        else
        {
            g_UsbDeviceVideoFlexioCamera.imagePosition = 0U;
            g_UsbDeviceVideoFlexioCamera.currentTime = 0U;
            g_UsbDeviceVideoFlexioCamera.waitForNewInterval = 0U;
            payloadHeader->headerInfoUnion.headerInfoBits.endOfFrame = 1U;
            g_UsbDeviceVideoFlexioCamera.stillImageTransmission = 0U;
            g_UsbDeviceVideoFlexioCamera.currentFrameId ^= 1U;
            if (USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_TRANSMIT_STILL_IMAGE ==
                g_UsbDeviceVideoFlexioCamera.stillImageTriggerControl)
            {
                g_UsbDeviceVideoFlexioCamera.stillImageTriggerControl =
                    USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_NORMAL_OPERATION;
                g_UsbDeviceVideoFlexioCamera.stillImageTransmission = 1U;
            }
            return;
        }
    }

    if (g_UsbDeviceVideoFlexioCamera.imagePosition < OV7670_FRAME_BYTES)
    {
        sendLength = OV7670_FRAME_BYTES - g_UsbDeviceVideoFlexioCamera.imagePosition;
        if (sendLength > maxPacketSize)
        {
            sendLength = maxPacketSize;
        }
        g_UsbDeviceVideoFlexioCamera.imagePosition += sendLength;

        if (g_UsbDeviceVideoFlexioCamera.imagePosition >= OV7670_FRAME_BYTES)
        {
            FLEXIO_Ov7670StartCapture(g_UsbDeviceVideoFlexioCamera.fullBufferIndex + 1);
            g_UsbDeviceVideoFlexioCamera.fullBufferIndex = 1 - g_UsbDeviceVideoFlexioCamera.fullBufferIndex;
            if (g_UsbDeviceVideoFlexioCamera.currentTime < g_UsbDeviceVideoFlexioCamera.commitStruct.dwFrameInterval)
            {
                g_UsbDeviceVideoFlexioCamera.waitForNewInterval = 1U;
            }
            else
            {
                g_UsbDeviceVideoFlexioCamera.imagePosition = 0U;
                g_UsbDeviceVideoFlexioCamera.currentTime = 0U;
                payloadHeader->headerInfoUnion.headerInfoBits.endOfFrame = 1U;
                g_UsbDeviceVideoFlexioCamera.stillImageTransmission = 0U;
                g_UsbDeviceVideoFlexioCamera.currentFrameId ^= 1U;
                if (USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_TRANSMIT_STILL_IMAGE ==
                    g_UsbDeviceVideoFlexioCamera.stillImageTriggerControl)
                {
                    g_UsbDeviceVideoFlexioCamera.stillImageTriggerControl =
                        USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_NORMAL_OPERATION;
                    g_UsbDeviceVideoFlexioCamera.stillImageTransmission = 1U;
                }
            }
        }
        g_UsbDeviceVideoFlexioCamera.imageBufferLength += sendLength;
    }
}

/* USB device video ISO IN endpoint callback */
static usb_status_t USB_DeviceVideoIsoIn(usb_device_handle deviceHandle,
                                         usb_device_endpoint_callback_message_struct_t *event,
                                         void *arg)
{
    if (g_UsbDeviceVideoFlexioCamera.attach)
    {
        USB_DeviceVideoPrepareVideoData();
        return USB_DeviceSendRequest(deviceHandle, USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN,
                                     g_UsbDeviceVideoFlexioCamera.imageBuffer,
                                     g_UsbDeviceVideoFlexioCamera.imageBufferLength);
    }

    return kStatus_USB_Error;
}

/* Set to default state */
static void USB_DeviceVideoApplicationSetDefault(void)
{
    g_UsbDeviceVideoFlexioCamera.speed = USB_SPEED_FULL;
    g_UsbDeviceVideoFlexioCamera.attach = 0U;
    g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize = FS_STREAM_IN_PACKET_SIZE;
    g_UsbDeviceVideoFlexioCamera.fullBufferIndex = 0U;
    g_UsbDeviceVideoFlexioCamera.imagePosition = 0U;

    g_UsbDeviceVideoFlexioCamera.probeStruct.bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoFlexioCamera.probeStruct.bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    g_UsbDeviceVideoFlexioCamera.probeStruct.dwFrameInterval = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_DEFAULT_INTERVAL;
    g_UsbDeviceVideoFlexioCamera.probeStruct.dwMaxPayloadTransferSize =
        g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize;
    g_UsbDeviceVideoFlexioCamera.probeStruct.dwMaxVideoFrameSize = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE;

    g_UsbDeviceVideoFlexioCamera.commitStruct.bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoFlexioCamera.commitStruct.bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    g_UsbDeviceVideoFlexioCamera.commitStruct.dwFrameInterval = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_DEFAULT_INTERVAL;
    g_UsbDeviceVideoFlexioCamera.commitStruct.dwMaxPayloadTransferSize =
        g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize;
    g_UsbDeviceVideoFlexioCamera.commitStruct.dwMaxVideoFrameSize = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE;

    g_UsbDeviceVideoFlexioCamera.probeInfo = 0x03U;
    g_UsbDeviceVideoFlexioCamera.probeLength = 26U;
    g_UsbDeviceVideoFlexioCamera.commitInfo = 0x03U;
    g_UsbDeviceVideoFlexioCamera.commitLength = 26U;

    g_UsbDeviceVideoFlexioCamera.stillProbeStruct.bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoFlexioCamera.stillProbeStruct.bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    g_UsbDeviceVideoFlexioCamera.stillProbeStruct.bCompressionIndex = 0x01U;
    g_UsbDeviceVideoFlexioCamera.stillProbeStruct.dwMaxPayloadTransferSize =
        g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize;
    g_UsbDeviceVideoFlexioCamera.stillProbeStruct.dwMaxVideoFrameSize =
        USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE;

    g_UsbDeviceVideoFlexioCamera.stillCommitStruct.bFormatIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FORMAT_INDEX;
    g_UsbDeviceVideoFlexioCamera.stillCommitStruct.bFrameIndex = USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_INDEX;
    g_UsbDeviceVideoFlexioCamera.stillCommitStruct.bCompressionIndex = 0x01U;
    g_UsbDeviceVideoFlexioCamera.stillCommitStruct.dwMaxPayloadTransferSize =
        g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize;
    g_UsbDeviceVideoFlexioCamera.stillCommitStruct.dwMaxVideoFrameSize =
        USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_FRAME_SIZE;

    g_UsbDeviceVideoFlexioCamera.stillProbeInfo = 0x03U;
    g_UsbDeviceVideoFlexioCamera.stillProbeLength = sizeof(g_UsbDeviceVideoFlexioCamera.stillProbeStruct);
    g_UsbDeviceVideoFlexioCamera.stillCommitInfo = 0x03U;
    g_UsbDeviceVideoFlexioCamera.stillCommitLength = sizeof(g_UsbDeviceVideoFlexioCamera.stillCommitStruct);

    g_UsbDeviceVideoFlexioCamera.currentTime = 0U;
    g_UsbDeviceVideoFlexioCamera.currentFrameId = 0U;
    g_UsbDeviceVideoFlexioCamera.currentStreamInterfaceAlternateSetting = 0U;
    g_UsbDeviceVideoFlexioCamera.imageBufferLength = 0U;
    g_UsbDeviceVideoFlexioCamera.imageIndex = 0U;
    g_UsbDeviceVideoFlexioCamera.waitForNewInterval = 0U;
    g_UsbDeviceVideoFlexioCamera.stillImageTransmission = 0U;
    g_UsbDeviceVideoFlexioCamera.stillImageTriggerControl = USB_DEVICE_VIDEO_STILL_IMAGE_TRIGGER_NORMAL_OPERATION;
}

/* Device callback */
usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Success;
    uint8_t *temp8 = (uint8_t *)param;

    switch (event)
    {
        case kUSB_DeviceEventBusReset:
        {
            USB_DeviceControlPipeInit(g_UsbDeviceVideoFlexioCamera.deviceHandle);
            USB_DeviceVideoApplicationSetDefault();
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
            if (kStatus_USB_Success == USB_DeviceGetStatus(g_UsbDeviceVideoFlexioCamera.deviceHandle,
                                                           kUSB_DeviceStatusSpeed, &g_UsbDeviceVideoFlexioCamera.speed))
            {
                USB_DeviceSetSpeed(g_UsbDeviceVideoFlexioCamera.speed);
            }

            if (USB_SPEED_HIGH == g_UsbDeviceVideoFlexioCamera.speed)
            {
                g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize = HS_STREAM_IN_PACKET_SIZE;
            }
#endif
        }
        break;
        case kUSB_DeviceEventSetConfiguration:
            if (USB_VIDEO_CAMERA_CONFIGURE_INDEX == (*temp8))
            {
                g_UsbDeviceVideoFlexioCamera.attach = 1U;
            }
            break;
        case kUSB_DeviceEventSetInterface:
            if ((g_UsbDeviceVideoFlexioCamera.attach) && param)
            {
                uint8_t interface = (*temp8) & 0xFFU;
                uint8_t alternateSetting = g_UsbDeviceInterface[interface];

                if (g_UsbDeviceVideoFlexioCamera.currentStreamInterfaceAlternateSetting != alternateSetting)
                {
                    if (g_UsbDeviceVideoFlexioCamera.currentStreamInterfaceAlternateSetting)
                    {
                        USB_DeviceDeinitEndpoint(g_UsbDeviceVideoFlexioCamera.deviceHandle,
                                                 USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN |
                                                     (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT));
                    }
                    else
                    {
                        usb_device_endpoint_init_struct_t epInitStruct;
                        usb_device_endpoint_callback_struct_t endpointCallback;

                        endpointCallback.callbackFn = USB_DeviceVideoIsoIn;
                        endpointCallback.callbackParam = handle;

                        epInitStruct.zlt = 0U;
                        epInitStruct.transferType = USB_ENDPOINT_ISOCHRONOUS;
                        epInitStruct.endpointAddress = USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN |
                                                       (USB_IN << USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_SHIFT);
                        if (USB_SPEED_HIGH == g_UsbDeviceVideoFlexioCamera.speed)
                        {
                            epInitStruct.maxPacketSize = HS_STREAM_IN_PACKET_SIZE;
                        }
                        else
                        {
                            epInitStruct.maxPacketSize = FS_STREAM_IN_PACKET_SIZE;
                        }

                        USB_DeviceInitEndpoint(g_UsbDeviceVideoFlexioCamera.deviceHandle, &epInitStruct,
                                               &endpointCallback);
                        USB_DeviceVideoPrepareVideoData();
                        error = USB_DeviceSendRequest(
                            g_UsbDeviceVideoFlexioCamera.deviceHandle, USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN,
                            g_UsbDeviceVideoFlexioCamera.imageBuffer, g_UsbDeviceVideoFlexioCamera.imageBufferLength);
                    }
                    g_UsbDeviceVideoFlexioCamera.currentStreamInterfaceAlternateSetting = alternateSetting;
                }
            }
            break;
        default:
            break;
    }

    return error;
}

/* Get the setup buffer */
usb_status_t USB_DeviceGetSetupBuffer(usb_device_handle handle, usb_setup_struct_t **setupBuffer)
{
    static uint32_t setup[2];
    if (NULL == setupBuffer)
    {
        return kStatus_USB_InvalidParameter;
    }
    *setupBuffer = (usb_setup_struct_t *)&setup;
    return kStatus_USB_Success;
}

/* Configure remote wakeup(Enable or disbale) */
usb_status_t USB_DevcieConfigureRemoteWakeup(usb_device_handle handle, uint8_t enable)
{
    return kStatus_USB_Success;
}
/* Configure endpoint status(Idle or stall) */
usb_status_t USB_DevcieConfigureEndpointStatus(usb_device_handle handle, uint8_t ep, uint8_t status)
{
    if (status)
    {
        if ((USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN == (ep & USB_ENDPOINT_NUMBER_MASK)) &&
            (ep & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK))
        {
            return USB_DeviceStallEndpoint(handle, ep);
        }
        else if ((USB_VIDEO_CAMERA_CONTROL_ENDPOINT == (ep & USB_ENDPOINT_NUMBER_MASK)) &&
                 (ep & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK))
        {
            return USB_DeviceStallEndpoint(handle, ep);
        }
        else
        {
        }
    }
    else
    {
        if ((USB_VIDEO_CAMERA_STREAM_ENDPOINT_IN == (ep & USB_ENDPOINT_NUMBER_MASK)) &&
            (ep & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK))
        {
            return USB_DeviceUnstallEndpoint(handle, ep);
        }
        else if ((USB_VIDEO_CAMERA_CONTROL_ENDPOINT == (ep & USB_ENDPOINT_NUMBER_MASK)) &&
                 (ep & USB_DESCRIPTOR_ENDPOINT_ADDRESS_DIRECTION_MASK))
        {
            return USB_DeviceUnstallEndpoint(handle, ep);
        }
        else
        {
        }
    }
    return kStatus_USB_InvalidRequest;
}

/* Get class-specific request buffer */
usb_status_t USB_DeviceGetClassReceiveBuffer(usb_device_handle handle,
                                             usb_setup_struct_t *setup,
                                             uint32_t *length,
                                             uint8_t **buffer)
{
    static uint32_t setupOut[(sizeof(usb_device_video_probe_and_commit_controls_struct_t) >> 2U) + 1U];
    if ((NULL == buffer) || ((*length) > sizeof(setupOut)))
    {
        return kStatus_USB_InvalidRequest;
    }
    *buffer = (uint8_t *)&setupOut[0];
    return kStatus_USB_Success;
}

/* Precess class-specific VS probe request */
static usb_status_t USB_DeviceVideoProcessClassVsProbeRequest(usb_device_handle handle,
                                                              usb_setup_struct_t *setup,
                                                              uint32_t *length,
                                                              uint8_t **buffer)
{
    usb_device_video_probe_and_commit_controls_struct_t *probe;
    usb_status_t error = kStatus_USB_InvalidRequest;

    if ((NULL == buffer) || (NULL == length))
    {
        return error;
    }

    probe = (usb_device_video_probe_and_commit_controls_struct_t *)(*buffer);

    switch (setup->bRequest)
    {
        case USB_DEVICE_VIDEO_REQUEST_CODE_SET_CUR:
            if ((*buffer == NULL) || (*length == 0U))
            {
                return error;
            }
            if ((probe->dwFrameInterval >= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MIN_INTERVAL) &&
                (probe->dwFrameInterval <= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_INTERVAL))
            {
                g_UsbDeviceVideoFlexioCamera.probeStruct.dwFrameInterval = probe->dwFrameInterval;
            }

            if ((probe->dwMaxPayloadTransferSize) &&
                (probe->dwMaxPayloadTransferSize < g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize))
            {
                g_UsbDeviceVideoFlexioCamera.probeStruct.dwMaxPayloadTransferSize = probe->dwMaxPayloadTransferSize;
            }

            g_UsbDeviceVideoFlexioCamera.probeStruct.bFormatIndex = probe->bFormatIndex;
            g_UsbDeviceVideoFlexioCamera.probeStruct.bFrameIndex = probe->bFrameIndex;

            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_CUR:
            *buffer = (uint8_t *)&g_UsbDeviceVideoFlexioCamera.probeStruct;
            *length = g_UsbDeviceVideoFlexioCamera.probeLength;
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_MIN:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_MAX:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_RES:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_LEN:
            *buffer = &g_UsbDeviceVideoFlexioCamera.probeLength;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.probeLength);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_INFO:
            *buffer = &g_UsbDeviceVideoFlexioCamera.probeInfo;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.probeInfo);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_DEF:
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VS commit request */
static usb_status_t USB_DeviceVideoProcessClassVsCommitRequest(usb_device_handle handle,
                                                               usb_setup_struct_t *setup,
                                                               uint32_t *length,
                                                               uint8_t **buffer)
{
    usb_device_video_probe_and_commit_controls_struct_t *commit;
    usb_status_t error = kStatus_USB_InvalidRequest;

    if ((NULL == buffer) || (NULL == length))
    {
        return error;
    }

    commit = (usb_device_video_probe_and_commit_controls_struct_t *)(*buffer);

    switch (setup->bRequest)
    {
        case USB_DEVICE_VIDEO_REQUEST_CODE_SET_CUR:
            if ((*buffer == NULL) || (*length == 0U))
            {
                return error;
            }
            if ((commit->dwFrameInterval >= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MIN_INTERVAL) &&
                (commit->dwFrameInterval <= USB_VIDEO_CAMERA_UNCOMPRESSED_FRAME_MAX_INTERVAL))
            {
                g_UsbDeviceVideoFlexioCamera.commitStruct.dwFrameInterval = commit->dwFrameInterval;
            }

            if ((commit->dwMaxPayloadTransferSize) &&
                (commit->dwMaxPayloadTransferSize < g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize))
            {
                g_UsbDeviceVideoFlexioCamera.commitStruct.dwMaxPayloadTransferSize = commit->dwMaxPayloadTransferSize;
            }

            g_UsbDeviceVideoFlexioCamera.commitStruct.bFormatIndex = commit->bFormatIndex;
            g_UsbDeviceVideoFlexioCamera.commitStruct.bFrameIndex = commit->bFrameIndex;
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_CUR:
            *buffer = (uint8_t *)&g_UsbDeviceVideoFlexioCamera.commitStruct;
            *length = g_UsbDeviceVideoFlexioCamera.commitLength;
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_LEN:
            *buffer = &g_UsbDeviceVideoFlexioCamera.commitLength;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.commitLength);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_INFO:
            *buffer = &g_UsbDeviceVideoFlexioCamera.commitInfo;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.commitInfo);
            error = kStatus_USB_Success;
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VS STILL probe request */
static usb_status_t USB_DeviceVideoProcessClassVsStillProbeRequest(usb_device_handle handle,
                                                                   usb_setup_struct_t *setup,
                                                                   uint32_t *length,
                                                                   uint8_t **buffer)
{
    usb_device_video_still_probe_and_commit_controls_struct_t *still_probe;
    usb_status_t error = kStatus_USB_InvalidRequest;

    if ((NULL == buffer) || (NULL == length))
    {
        return error;
    }

    still_probe = (usb_device_video_still_probe_and_commit_controls_struct_t *)(*buffer);

    switch (setup->bRequest)
    {
        case USB_DEVICE_VIDEO_REQUEST_CODE_SET_CUR:
            if ((*buffer == NULL) || (*length == 0U))
            {
                return error;
            }
            if ((still_probe->dwMaxPayloadTransferSize) &&
                (still_probe->dwMaxPayloadTransferSize < g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize))
            {
                g_UsbDeviceVideoFlexioCamera.stillProbeStruct.dwMaxPayloadTransferSize =
                    still_probe->dwMaxPayloadTransferSize;
            }

            g_UsbDeviceVideoFlexioCamera.stillProbeStruct.bFormatIndex = still_probe->bFormatIndex;
            g_UsbDeviceVideoFlexioCamera.stillProbeStruct.bFrameIndex = still_probe->bFrameIndex;

            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_CUR:
            *buffer = (uint8_t *)&g_UsbDeviceVideoFlexioCamera.stillProbeStruct;
            *length = g_UsbDeviceVideoFlexioCamera.stillProbeLength;
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_MIN:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_MAX:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_RES:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_LEN:
            *buffer = &g_UsbDeviceVideoFlexioCamera.stillProbeLength;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.stillProbeLength);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_INFO:
            *buffer = &g_UsbDeviceVideoFlexioCamera.stillProbeInfo;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.stillProbeInfo);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_DEF:
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VS STILL commit request */
static usb_status_t USB_DeviceVideoProcessClassVsStillCommitRequest(usb_device_handle handle,
                                                                    usb_setup_struct_t *setup,
                                                                    uint32_t *length,
                                                                    uint8_t **buffer)
{
    usb_device_video_still_probe_and_commit_controls_struct_t *still_commit;
    usb_status_t error = kStatus_USB_InvalidRequest;

    if ((NULL == buffer) || (NULL == length))
    {
        return error;
    }

    still_commit = (usb_device_video_still_probe_and_commit_controls_struct_t *)(*buffer);

    switch (setup->bRequest)
    {
        case USB_DEVICE_VIDEO_REQUEST_CODE_SET_CUR:
            if ((*buffer == NULL) || (*length == 0U))
            {
                return error;
            }
            if ((still_commit->dwMaxPayloadTransferSize) &&
                (still_commit->dwMaxPayloadTransferSize < g_UsbDeviceVideoFlexioCamera.currentMaxPacketSize))
            {
                g_UsbDeviceVideoFlexioCamera.stillCommitStruct.dwMaxPayloadTransferSize =
                    still_commit->dwMaxPayloadTransferSize;
            }

            g_UsbDeviceVideoFlexioCamera.stillCommitStruct.bFormatIndex = still_commit->bFormatIndex;
            g_UsbDeviceVideoFlexioCamera.stillCommitStruct.bFrameIndex = still_commit->bFrameIndex;

            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_CUR:
            *buffer = (uint8_t *)&g_UsbDeviceVideoFlexioCamera.stillCommitStruct;
            *length = g_UsbDeviceVideoFlexioCamera.stillCommitLength;
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_LEN:
            *buffer = &g_UsbDeviceVideoFlexioCamera.stillCommitLength;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.stillCommitLength);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_INFO:
            *buffer = &g_UsbDeviceVideoFlexioCamera.stillCommitInfo;
            *length = sizeof(g_UsbDeviceVideoFlexioCamera.stillCommitInfo);
            error = kStatus_USB_Success;
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VS STILL image trigger request */
static usb_status_t USB_DeviceVideoProcessClassVsStillImageTriggerRequest(usb_device_handle handle,
                                                                          usb_setup_struct_t *setup,
                                                                          uint32_t *length,
                                                                          uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;

    if ((NULL == buffer) || (NULL == length))
    {
        return error;
    }

    switch (setup->bRequest)
    {
        case USB_DEVICE_VIDEO_REQUEST_CODE_SET_CUR:
            if ((*buffer == NULL) || (*length == 0U))
            {
                return error;
            }
            g_UsbDeviceVideoFlexioCamera.stillImageTriggerControl = *(*buffer);
            error = kStatus_USB_Success;
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_CUR:
            break;
        case USB_DEVICE_VIDEO_REQUEST_CODE_GET_INFO:
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VS request */
static usb_status_t USB_DeviceVideoProcessClassVsRequest(usb_device_handle handle,
                                                         usb_setup_struct_t *setup,
                                                         uint32_t *length,
                                                         uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t cs = (setup->wValue >> 0x08U) & 0xFFU;

    switch (cs)
    {
        case USB_DEVICE_VIDEO_VS_PROBE_CONTROL:
            error = USB_DeviceVideoProcessClassVsProbeRequest(handle, setup, length, buffer);
            break;
        case USB_DEVICE_VIDEO_VS_COMMIT_CONTROL:
            error = USB_DeviceVideoProcessClassVsCommitRequest(handle, setup, length, buffer);
            break;
        case USB_DEVICE_VIDEO_VS_STILL_PROBE_CONTROL:
            error = USB_DeviceVideoProcessClassVsStillProbeRequest(handle, setup, length, buffer);
            break;
        case USB_DEVICE_VIDEO_VS_STILL_COMMIT_CONTROL:
            error = USB_DeviceVideoProcessClassVsStillCommitRequest(handle, setup, length, buffer);
            break;
        case USB_DEVICE_VIDEO_VS_STILL_IMAGE_TRIGGER_CONTROL:
            error = USB_DeviceVideoProcessClassVsStillImageTriggerRequest(handle, setup, length, buffer);
            break;
        case USB_DEVICE_VIDEO_VS_STREAM_ERROR_CODE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_VS_GENERATE_KEY_FRAME_CONTROL:
            break;
        case USB_DEVICE_VIDEO_VS_UPDATE_FRAME_SEGMENT_CONTROL:
            break;
        case USB_DEVICE_VIDEO_VS_SYNCH_DELAY_CONTROL:
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VC interface request */
static usb_status_t USB_DeviceProcessClassVcInterfaceRequest(usb_device_handle handle,
                                                             usb_setup_struct_t *setup,
                                                             uint32_t *length,
                                                             uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t cs = (setup->wValue >> 0x08U) & 0xFFU;

    if (USB_DEVICE_VIDEO_VC_VIDEO_POWER_MODE_CONTROL == cs)
    {
    }
    else if (USB_DEVICE_VIDEO_VC_REQUEST_ERROR_CODE_CONTROL == cs)
    {
    }
    else
    {
    }
    return error;
}

/* Precess class-specific VC camera terminal request */
static usb_status_t USB_DeviceProcessClassVcCameraTerminalRequest(usb_device_handle handle,
                                                                  usb_setup_struct_t *setup,
                                                                  uint32_t *length,
                                                                  uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t cs = (setup->wValue >> 0x08U) & 0xFFU;

    switch (cs)
    {
        case USB_DEVICE_VIDEO_CT_SCANNING_MODE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_AE_MODE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_AE_PRIORITY_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_FOCUS_ABSOLUTE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_FOCUS_RELATIVE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_FOCUS_AUTO_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_IRIS_ABSOLUTE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_IRIS_RELATIVE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_ZOOM_ABSOLUTE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_ZOOM_RELATIVE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_PANTILT_ABSOLUTE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_PANTILT_RELATIVE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_ROLL_ABSOLUTE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_ROLL_RELATIVE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_PRIVACY_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_FOCUS_SIMPLE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_WINDOW_CONTROL:
            break;
        case USB_DEVICE_VIDEO_CT_REGION_OF_INTEREST_CONTROL:
            break;
        default:
            break;
    }
    return error;
}

/* Precess class-specific VC input terminal request */
static usb_status_t USB_DeviceProcessClassVcInputTerminalRequest(usb_device_handle handle,
                                                                 usb_setup_struct_t *setup,
                                                                 uint32_t *length,
                                                                 uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;

    if (USB_DEVICE_VIDEO_ITT_CAMERA == USB_VIDEO_CAMERA_VC_INPUT_TERMINAL_TYPE)
    {
        error = USB_DeviceProcessClassVcCameraTerminalRequest(handle, setup, length, buffer);
    }

    return error;
}

/* Precess class-specific VC onput terminal request */
static usb_status_t USB_DeviceProcessClassVcOutputTerminalRequest(usb_device_handle handle,
                                                                  usb_setup_struct_t *setup,
                                                                  uint32_t *length,
                                                                  uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;

    return error;
}

/* Precess class-specific VC processing unit request */
static usb_status_t USB_DeviceProcessClassVcProcessingUintRequest(usb_device_handle handle,
                                                                  usb_setup_struct_t *setup,
                                                                  uint32_t *length,
                                                                  uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t cs = (setup->wValue >> 0x08U) & 0xFFU;

    switch (cs)
    {
        case USB_DEVICE_VIDEO_PU_BACKLIGHT_COMPENSATION_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_BRIGHTNESS_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_CONTRAST_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_GAIN_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_POWER_LINE_FREQUENCY_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_HUE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_SATURATION_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_SHARPNESS_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_GAMMA_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_WHITE_BALANCE_COMPONENT_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_DIGITAL_MULTIPLIER_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_HUE_AUTO_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_ANALOG_VIDEO_STANDARD_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_ANALOG_LOCK_STATUS_CONTROL:
            break;
        case USB_DEVICE_VIDEO_PU_CONTRAST_AUTO_CONTROL:
            break;
        default:
            break;
    }

    return error;
}

/* Precess class-specific VC request */
static usb_status_t USB_DeviceProcessClassVcRequest(usb_device_handle handle,
                                                    usb_setup_struct_t *setup,
                                                    uint32_t *length,
                                                    uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t entityId = (uint8_t)(setup->wIndex >> 0x08U);
    if (0U == entityId)
    {
        error = USB_DeviceProcessClassVcInterfaceRequest(handle, setup, length, buffer);
    }
    else if (USB_VIDEO_CAMERA_VC_INPUT_TERMINAL_ID == entityId)
    {
        error = USB_DeviceProcessClassVcInputTerminalRequest(handle, setup, length, buffer);
    }
    else if (USB_VIDEO_CAMERA_VC_OUTPUT_TERMINAL_ID == entityId)
    {
        error = USB_DeviceProcessClassVcOutputTerminalRequest(handle, setup, length, buffer);
    }
    else if (USB_VIDEO_CAMERA_VC_PROCESSING_UNIT_ID == entityId)
    {
        error = USB_DeviceProcessClassVcProcessingUintRequest(handle, setup, length, buffer);
    }
    else
    {
    }
    return error;
}

/* Precess class-specific request */
usb_status_t USB_DeviceProcessClassRequest(usb_device_handle handle,
                                           usb_setup_struct_t *setup,
                                           uint32_t *length,
                                           uint8_t **buffer)
{
    usb_status_t error = kStatus_USB_InvalidRequest;
    uint8_t interface_index = (uint8_t)setup->wIndex;

    switch (setup->bmRequestType)
    {
        case USB_DEVICE_VIDEO_GET_REQUSET_INTERFACE:
            if (USB_VIDEO_CAMERA_CONTROL_INTERFACE_INDEX == interface_index)
            {
                error = USB_DeviceProcessClassVcRequest(handle, setup, length, buffer);
            }
            else if (USB_VIDEO_CAMERA_STREAM_INTERFACE_INDEX == interface_index)
            {
                error = USB_DeviceVideoProcessClassVsRequest(handle, setup, length, buffer);
            }
            else
            {
            }
            break;
        case USB_DEVICE_VIDEO_SET_REQUSET_INTERFACE:
            if (USB_VIDEO_CAMERA_CONTROL_INTERFACE_INDEX == interface_index)
            {
                error = USB_DeviceProcessClassVcRequest(handle, setup, length, buffer);
            }
            else if (USB_VIDEO_CAMERA_STREAM_INTERFACE_INDEX == interface_index)
            {
                error = USB_DeviceVideoProcessClassVsRequest(handle, setup, length, buffer);
            }
            else
            {
            }
            break;
        default:
            break;
    }

    return error;
}

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
void USBHS_IRQHandler(void)
{
    USB_DeviceEhciIsrFunction(g_UsbDeviceVideoFlexioCamera.deviceHandle);
}
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
void USB0_IRQHandler(void)
{
    USB_DeviceKhciIsrFunction(g_UsbDeviceVideoFlexioCamera.deviceHandle);
}
#endif

static void USB_DeviceApplicationInit(void)
{
    uint8_t irqNumber;
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    uint8_t usbDeviceEhciIrq[] = USBHS_IRQS;
    irqNumber = usbDeviceEhciIrq[CONTROLLER_ID - kUSB_ControllerEhci0];

    CLOCK_EnableUsbhs0Clock(kCLOCK_UsbSrcPll0, CLOCK_GetFreq(kCLOCK_PllFllSelClk));
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ);
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    uint8_t usbDeviceKhciIrq[] = USB_IRQS;
    irqNumber = usbDeviceKhciIrq[CONTROLLER_ID - kUSB_ControllerKhci0];

    SystemCoreClockUpdate();

#if ((defined FSL_FEATURE_USB_KHCI_IRC48M_MODULE_CLOCK_ENABLED) && (FSL_FEATURE_USB_KHCI_IRC48M_MODULE_CLOCK_ENABLED))
    CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);
#else
    CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcPll0, CLOCK_GetFreq(kCLOCK_PllFllSelClk));
#endif /* FSL_FEATURE_USB_KHCI_IRC48M_MODULE_CLOCK_ENABLED */
#endif
#if (defined(FSL_FEATURE_SOC_MPU_COUNT) && (FSL_FEATURE_SOC_MPU_COUNT > 0U))
    MPU->CESR = 0;
#endif /* FSL_FEATURE_SOC_MPU_COUNT */

       /*
        * If the SOC has USB KHCI dedicated RAM, the RAM memory needs to be clear after
        * the KHCI clock is enabled. When the demo uses USB EHCI IP, the USB KHCI dedicated
        * RAM can not be used and the memory can't be accessed.
        */
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U))
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS) && (FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS > 0U))
    for (int i = 0; i < FSL_FEATURE_USB_KHCI_USB_RAM; i++)
    {
        ((uint8_t *)FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS)[i] = 0x00U;
    }
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS */
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM */

    USB_DeviceVideoApplicationSetDefault();

    if (kStatus_USB_Success !=
        USB_DeviceInit(CONTROLLER_ID, USB_DeviceCallback, &g_UsbDeviceVideoFlexioCamera.deviceHandle))
    {
        usb_echo("USB device video camera failed\r\n");
        return;
    }
    else
    {
        usb_echo("USB device video camera demo\r\n");
    }

    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ((IRQn_Type)irqNumber);

    FLEXIO_Ov7670Init();

    USB_DeviceRun(g_UsbDeviceVideoFlexioCamera.deviceHandle);
}

#if defined(__CC_ARM) || defined(__GNUC__)
int main(void)
#else
void main(void)
#endif
{
    gpio_pin_config_t pinConfig;

    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    CLOCK_SetClkOutClock(0x06);
    /* CLOCK_SetFlexio0Clock(0x01);*/
    /* CLOCK_EnableClock(kCLOCK_Flexio0);*/

    /* enable OV7670 Reset output */
    pinConfig.pinDirection = kGPIO_DigitalOutput;
    pinConfig.outputLogic = 1U;

    GPIO_PinInit(GPIOC, 8U, &pinConfig);

    USB_DeviceApplicationInit();

    init();

    while (1U)
    {
    	//run();
#if USB_DEVICE_CONFIG_USE_TASK
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
        USB_DeviceEhciTaskFunction(g_UsbDeviceVideoFlexioCamera.deviceHandle);
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
        USB_DeviceKhciTaskFunction(g_UsbDeviceVideoFlexioCamera.deviceHandle);
#endif
#endif
    }

}
