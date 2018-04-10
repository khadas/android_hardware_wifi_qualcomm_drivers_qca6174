/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/  

/* tlvCmd_if_Qdart.c - Interface to DevdrvIf.DLL ( DevdrvIf.DLL access device driver ) */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Qcmbr.h"

//  Remote error number and error string
A_INT32 remoteMdkErrNo = 0;
A_CHAR remoteMdkErrStr[SIZE_ERROR_BUFFER];

#ifdef LINUX_X86
#include <ifaddrs.h>
char *ifa_name = NULL;
#endif

// holds the cmd replies sent over channel
static CMD_REPLY cmdReply;
static A_BOOL cmdInitCalled = FALSE;

/**************************************************************************
* receiveCmdReturn - Callback function for calling cmd_init().
*       Note: We are keeping the calling convention.
*       Note: cmd_init() is a library function from DevdrvIf.DLL.
*		Note: Qcmbr does not ( need to ) do anything to the data or care
*			  what is in the data.
*/
void receiveCmdReturn(void *buf)
{
        printf("CallBack-receiveCmdReturn bufAddr[%8.8X]\n",(unsigned int)buf);
	if ( buf == NULL )
	{
	}
	// Dummy call back function
}

#ifdef LINUX_X86
void get_wifi_ifname()
{
    struct ifaddrs *addrs, *tmp;

    getifaddrs(&addrs);
    tmp = addrs;

    while(tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
            memcmp(tmp->ifa_name, "wlan", 4) == 0) {
            ifa_name = malloc(strlen(tmp->ifa_name) + 1);
            memset(ifa_name, 0x0, strlen(tmp->ifa_name) + 1);
            if (ifa_name) {
                memcpy(ifa_name, tmp->ifa_name, strlen(tmp->ifa_name));
                printf("%s (%d)\n", ifa_name, strlen(ifa_name));
                break;
            }
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
}
#endif

/**************************************************************************
* artSendCmd2 - This function sends the TLV command passing from host (caller)
*				to the device interface function.
*				
*/

A_BOOL artSendCmd2( A_UINT8 *pCmdStruct, A_UINT32 cmdSize, unsigned char* responseCharBuf, unsigned int *responseSize )
{
    int		errorNo;
    char buf[MBUFFER + 8];

    extern void receiveCmdReturn1(void *buf);
    extern void DispHexString(A_UINT8 *pCmdStruct,A_UINT32 cmdSize);
    DispHexString(pCmdStruct,cmdSize);

    memset(buf, 0, sizeof(buf));

    if (cmdInitCalled == FALSE)
    {
#ifdef LINUX_X86
        get_wifi_ifname();
        if (ifa_name != NULL)
            errorNo = cmd_init(ifa_name,receiveCmdReturn1);
        else
            errorNo = cmd_init("wlan0",receiveCmdReturn1);
#else
        errorNo = cmd_init("wifi0",receiveCmdReturn1);
#endif

    	cmdInitCalled = TRUE;
    }

    memcpy(&buf[8],pCmdStruct,cmdSize);

    printf( "arSendCmd2->cmd_send2 RspLen [%d]\n", *responseSize );
    cmd_send2( buf, cmdSize, responseCharBuf, responseSize );

    remoteMdkErrNo = 0;
    errorNo = (A_UINT16) (cmdReply.status & COMMS_ERR_MASK) >> COMMS_ERR_SHIFT;
    if (errorNo == COMMS_ERR_MDK_ERROR)
    {
        remoteMdkErrNo = (cmdReply.status & COMMS_ERR_INFO_MASK) >> COMMS_ERR_INFO_SHIFT;
        strncpy(remoteMdkErrStr,(const char *)cmdReply.cmdBytes,SIZE_ERROR_BUFFER);
	printf("Error: COMMS error MDK error for command DONT_CARE\n" );
        return TRUE;
    }

    // check for a bad status in the command reply
    if (errorNo != CMD_OK)
	{
	printf("Error: Bad return status (%d) in client command DONT_CARE response!\n", errorNo);
        return FALSE;
    }

    return TRUE;
}


