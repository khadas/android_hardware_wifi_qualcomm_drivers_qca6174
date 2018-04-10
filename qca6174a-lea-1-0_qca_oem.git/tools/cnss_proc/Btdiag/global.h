/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/
#include "uart.h"
#define TCP_BUF_LENGTH         256

#define HCI_MAX_EVENT_SIZE     260
#define HCI_CMD_IND            (1)
#define HCI_COMMAND_HDR_SIZE   3

typedef unsigned char BYTE;

typedef enum 
{
    SERIAL,
    USB,
	ETHERNET,
    INVALID
} ConnectionType;