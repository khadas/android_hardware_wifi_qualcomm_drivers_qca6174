
/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/  

#include <basetyps.h>
#include <stdlib.h>
#include <wtypes.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <Windows.h>
#include <winioctl.h>
#include <Dbt.h>
#include <process.h>
#include <tchar.h>

#include "qdev_if.h"

#define WM_CREATE_INIT WM_USER + 101

static HANDLE gRegHandle1 = INVALID_HANDLE_VALUE;
static HANDLE gRegHandle2 = INVALID_HANDLE_VALUE;
static LPTSTR gWndName = "DeviceMsgWindow";
static HWND   gHwnd = NULL;
static DWORD  gThreadId = 0;

HANDLE ghDevArrivalEvent = NULL;
HANDLE DevHandle = NULL;

#define ATH_XIOCTL_UNIFIED_UTF_CMD      0x1000
#define ATH_XIOCTL_UNIFIED_UTF_RSP      0x1001

#define CMD_TIMEOUT   (8 *1000)

typedef struct {
    int sock;

    void (*rx_cb)(void *buf);
    unsigned char initialized;
    unsigned char timeout;

} INIT_STRUCT;

static INIT_STRUCT initCfg;
static unsigned char responseBuf[2048+8];
static UINT timerId = 0;
static UINT gUtfCmdEvtDumpEnabled = 0;

extern HANDLE DevHandle;
extern HANDLE ghDevArrivalEvent;

BOOL IsDeviceOpened()
{
	return(DevHandle?TRUE:FALSE);
}

void CALLBACK timer_expire(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    printf("Timer Expired..\n");
    initCfg.timeout = 1;
}

int cmd_set_timer()
{
	timerId = SetTimer(NULL, 0, CMD_TIMEOUT, timer_expire);
	if (timerId == 0)
	{
		printf ("No timer is available.\n");
		return -1;
	}
    return 0;
}

int cmd_stop_timer()
{
	KillTimer(NULL, timerId);
    return 0;
}

DEVDRVIF_API void *cmd_getDevHandle ()
{
	return DevHandle;
}


DEVDRVIF_API DWORD probe_device()
{
	DWORD dwRet;

	ghDevArrivalEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if( NULL == ghDevArrivalEvent )
	{
		printf("Failed to create device arrival event object.\n");
		return -1;
	}

	dwRet = OpenArt2Device( &DevHandle );
	if( ERROR_SUCCESS == dwRet )
	{
		SetEvent( ghDevArrivalEvent );
	}

	InitDevMsg(NULL);

	return dwRet;
}

DEVDRVIF_API int cmd_init (char *ifname, void (*rx_cb)(void *buf))
{
	DWORD				dwRet = ERROR_SUCCESS;
	PART2_IO_REQ_HEADER pIoReqHeader = NULL;

    if ( initCfg.initialized )
        return -1;

	WaitForDeviceReady();

	// device is open successfully
	initCfg.initialized = 1;

	initCfg.rx_cb = rx_cb;

    return 0;
}

DEVDRVIF_API int cmd_end ()
{
	DWORD dwRet = ERROR_SUCCESS;

	initCfg.initialized = 0;
	return dwRet;
}


DEVDRVIF_API int cmd_send2 (void *buf, int len, unsigned char *returnBuf, unsigned int *returnBufSize )
{
	PART2_IO_REQ_HEADER IoReqHeader;
	DWORD dwRet;
	ULONG bytesRet;
	int waitCounter, nSec=0;
	unsigned int maxReturnBufSize = *returnBufSize;

    if (!initCfg.initialized)
       return -1;

	memset(returnBuf, 0, maxReturnBufSize );
	
	IoReqHeader = (PART2_IO_REQ_HEADER)buf;
	IoReqHeader->RequestId = ART2_REQ_ID_UTF_CMD;
	IoReqHeader->InputContentLength = len;

	dwRet = Art2DeviceIoctl( 
					DevHandle,
					ART2_DEV_IOCTL_REQUEST,
					(PUCHAR)IoReqHeader,
					sizeof(ART2_IO_REQ_HEADER) + IoReqHeader->InputContentLength,
					NULL,
					0,
					&bytesRet
					);
	if ( dwRet != (int) ERROR_SUCCESS )
	{
		printf("ART2_REQ_ID_UTF_CMD failed %X\n",dwRet);
		return 0;
	}
	
	cmd_set_timer();  // Original code is not working ???
    
	IoReqHeader->RequestId = ART2_REQ_ID_UTF_RSP;
	IoReqHeader->InputContentLength = 0;

	waitCounter = 0;
	dwRet = -1;
	while( ( dwRet != (int) ERROR_SUCCESS ) && ( !initCfg.timeout ) && (nSec<(CMD_TIMEOUT/1000)))
	{
		dwRet = Art2DeviceIoctl(
						DevHandle,
						ART2_DEV_IOCTL_REQUEST,
						(PUCHAR)IoReqHeader,
						sizeof(ART2_IO_REQ_HEADER) + IoReqHeader->InputContentLength,
						returnBuf,
						maxReturnBufSize,
						&bytesRet
						);

		if ( bytesRet == 0 )
		{
			waitCounter++;
			if ((waitCounter%100)==0) {
			  if ( ( waitCounter % 800 ) == 0 ) 
				  printf( "\n" );
			  printf( ".waiting.." );  // print at 1 second interval
			  nSec++;
			}
			Sleep( 10 );
		}
			
		if ( initCfg.timeout )
		{
			printf( "Timeout while waiting for response\n" );
			break;
        }
	}

	if(!initCfg.timeout)
	{
		cmd_stop_timer(); 
        initCfg.timeout = 0;
    }

	*returnBufSize = bytesRet; // return/set the number of bytes no matter what ( even if it is 0 ) 

	if ( dwRet != (int) ERROR_SUCCESS )
	{
		return 0;
	}
	else
	{
		{
			extern void DispHexString(unsigned char *pCmdStruct, int cmdSize);
			DispHexString((unsigned char *)returnBuf,bytesRet);
		}
		return 1;
	}
}


// ====================================================================================================================
DWORD WaitForDeviceReady()
{
	DWORD dwRet;
	
	if( ghDevArrivalEvent != NULL )
	{
		dwRet = WaitForSingleObjectEx( ghDevArrivalEvent, INFINITE, TRUE );
		if( WAIT_OBJECT_0 == dwRet )
		{
			return ERROR_SUCCESS;
		}

		printf("Unexpected error status: %08X.\n", dwRet);
		return dwRet;
	}

	/* Should never get here */
	printf("Fatal error: global device arrival event empty!!\n");
	return ERROR_NOT_ENOUGH_MEMORY;
}

BOOL DeviceExists()
{
	DWORD dwRet;

	if( ghDevArrivalEvent != NULL )
	{
		dwRet = WaitForSingleObject( ghDevArrivalEvent, 0 );
		if( WAIT_OBJECT_0 == dwRet )
		{
			printf("%08X returned by WaitForSingleObject\n", dwRet);
			return TRUE;
		}

		printf("Error status: %08X, device unplugged?!\n", dwRet);
		return FALSE;
	}

	/* Should never get here */
	printf("Fatal error: global device arrival event empty!!\n");
	return FALSE;

}


LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DWORD dwRet;

	switch(msg)
	{
	case WM_CREATE_INIT:
		{
			DEV_BROADCAST_DEVICEINTERFACE dbcc1 = {0}, dbcc2 = {0};

			dbcc1.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
			dbcc1.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
			dbcc1.dbcc_classguid  = ART2_DEV_INF_GUID; 	
			gRegHandle1 = RegisterDeviceNotification(wnd, &dbcc1, DEVICE_NOTIFY_WINDOW_HANDLE );

			dbcc2.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
			dbcc2.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
			dbcc2.dbcc_classguid  = NetGuid; 	
			gRegHandle2 = RegisterDeviceNotification(wnd, &dbcc2, DEVICE_NOTIFY_WINDOW_HANDLE);

		}
		break;
	case WM_DEVICECHANGE:
		{
			switch(wParam)
			{
			case DBT_CUSTOMEVENT:
				{
					PDEV_BROADCAST_HDR _temp = (PDEV_BROADCAST_HDR)lParam;
					if (_temp->dbch_devicetype == DBT_DEVTYP_HANDLE)
					{

					}
				}
				break;
			case DBT_DEVICEARRIVAL:
				{
					PDEV_BROADCAST_DEVICEINTERFACE pDevInterface = (PDEV_BROADCAST_DEVICEINTERFACE)lParam;
	
					if ((pDevInterface->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && 
						 IsEqualGUID(&pDevInterface->dbcc_classguid,&NetGuid)) ||
					    (pDevInterface->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && 
						 IsEqualGUID(&pDevInterface->dbcc_classguid,&ART2_DEV_INF_GUID)))
					{
						printf("\nDevice insertion detected!\n");

						if (DevHandle) 
						{
							printf("Device opened before, closing it now...\n");
							CloseArt2Device( );
							DevHandle = NULL;
						}

						dwRet = OpenArt2Device( &DevHandle );
						if( ERROR_SUCCESS != dwRet )
						{
							printf("Failed to open ART2 device, error = %08X\n", dwRet );
							return dwRet;
						}

						if( NULL == DevHandle )
						{
							printf("OpenArt2Device() returns invalid device handle\n");
							return dwRet;
						}

						if( ghDevArrivalEvent != INVALID_HANDLE_VALUE )
						{
							SetEvent( ghDevArrivalEvent );
						}
					}
				}
				break;
			case DBT_DEVICEQUERYREMOVE:
				{

				}
				break;
			case DBT_DEVICEREMOVECOMPLETE:
				{
					PDEV_BROADCAST_DEVICEINTERFACE pDevInterface = (PDEV_BROADCAST_DEVICEINTERFACE)lParam;

					if ((pDevInterface->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && 
						 IsEqualGUID(&pDevInterface->dbcc_classguid,&NetGuid)) ||
					    (pDevInterface->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && 
						 IsEqualGUID(&pDevInterface->dbcc_classguid,&ART2_DEV_INF_GUID)))
					{
						printf("\nDevice removal detected!\n");
						ResetEvent( ghDevArrivalEvent );
						if( DevHandle)
						{
							CloseArt2Device();
							DevHandle = NULL;
						}
					}
				}
				break;

			default:
				break;
			}		
		}
		break;
	case WM_CLOSE:
		{
			if(gRegHandle1 != INVALID_HANDLE_VALUE)
			{
				UnregisterDeviceNotification(gRegHandle1);
			}
			if(gRegHandle2 != INVALID_HANDLE_VALUE)
			{
				UnregisterDeviceNotification(gRegHandle2);
			}
		}
		break;
	default:
		return DefWindowProc(wnd, msg, wParam, lParam);
	}

	return 1;
}

unsigned __stdcall ThreadProc( void * param)
{		
	WNDCLASSEX wcex = {0};
	WORD wrd;
	DWORD err;
	HANDLE hStartEvent;
	MSG _msg;

	//printf("Setting up device notification window object...\n");
	wcex.cbSize = sizeof(wcex);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hInstance = NULL;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = "DeviceMsg";
	wcex.lpszClassName = gWndName;
	wrd = RegisterClassEx(&wcex);
	err = GetLastError();
	gHwnd = CreateWindow(gWndName, "", WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL);
	err = GetLastError();
	if(gHwnd != NULL)
	{
		ShowWindow(gHwnd, SW_HIDE);
		UpdateWindow(gHwnd);
	}
	hStartEvent = (HANDLE)param;
	SetEvent(hStartEvent);
	
	//printf("Entering message loop...\n");
	while(GetMessage(&_msg, 0, 0, 0))
	{
		switch(_msg.message)
		{
		case WM_CLOSE:
			SendMessage(gHwnd, WM_CLOSE, 0, 0);
			return 0;
		default:
			TranslateMessage(&_msg);
			DispatchMessage(&_msg);
			break;
		}
	}

	return 0;
}

void InitDevMsg( HANDLE hDevArrivalEvent )
{
	HANDLE hStartEvent = INVALID_HANDLE_VALUE;
	HANDLE hThread;
	
	hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if( INVALID_HANDLE_VALUE == hStartEvent )
	{
		return;
	}

	//printf("Launching device event monitor thread...\n");
	hThread = (HANDLE)_beginthreadex( 
				NULL,				// security
				0,					// stack_size
				ThreadProc,			// unsigned ( __stdcall *start_address )( void * )
				(LPVOID)hStartEvent,// arglist
				0,					// initflag
				NULL				// thrdaddr 
				);
	if(hThread != NULL)
	{
		//printf("Monitor thread handle obtained, resuming thread...\n");
		//ResumeThread(hThread);
		//CloseHandle(hThread);
	}
	else
	{
		printf("Unable to create the thread.");
	}

	if(hStartEvent != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObjectEx(hStartEvent, 10000, TRUE);
		//printf("Monitor thread started.\n");
		if(gHwnd != NULL)
		{
			//printf("Sending initialize message.\n");
			SendMessage(gHwnd, WM_CREATE_INIT, (WPARAM)NULL, (LPARAM)NULL);
		}
	}
	CloseHandle(hStartEvent);
}

void UninitDevMsg()
{
	PostThreadMessage(gThreadId, WM_CLOSE, 0, 0);
}

static DWORD OpenArt2DeviceOld( PHANDLE OutputHandle )
{
	HANDLE		handle = NULL;
	HDEVINFO	hardwareDeviceInfo;
	DWORD		dwRet = ERROR_SUCCESS;
	BOOL		bRet;

	SP_DEVICE_INTERFACE_DATA deviceInterfaceData;


    hardwareDeviceInfo = SetupDiGetClassDevs (
							(LPGUID)&ART2_DEV_INF_GUID,
							NULL, // Define no enumerator (global)
							NULL, // Define no
							(DIGCF_PRESENT |		// Only Devices present
							DIGCF_DEVICEINTERFACE)	// Function class devices.
							); 

    if(INVALID_HANDLE_VALUE == hardwareDeviceInfo)
    {
		dwRet = GetLastError();
        printf("SetupDiGetClassDevs failed: %x\n", dwRet);
        return dwRet;
    }

	deviceInterfaceData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

	bRet = SetupDiEnumDeviceInterfaces (hardwareDeviceInfo,
										0, // No care about specific PDOs
										(LPGUID)&ART2_DEV_INF_GUID,
										0, //
										&deviceInterfaceData 
										);
	if (bRet) 
	{
		bRet = OpenBusInterface(hardwareDeviceInfo, &deviceInterfaceData, OutputHandle);
    } 
	else if (ERROR_NO_MORE_ITEMS == GetLastError()) 
	{
        //printf( "Error:Interface ART2_DEV_INF_GUID is not registered\n" );
		dwRet = ERROR_NO_MORE_ITEMS;
    }

	SetupDiDestroyDeviceInfoList (hardwareDeviceInfo);

	return dwRet;
}

DWORD OpenArt2Device( PHANDLE OutputHandle )
{
	DWORD		dwRet = ERROR_SUCCESS;
    HANDLE      hHandle;
	TCHAR		DevName[ 256 ];
    int         i;

	for (i = 0; i < 16; i++)
	{
		_stprintf_s( DevName, _countof(DevName), _T("\\\\.\\ATH_WIFIDEV.%02d"), i);
    	hHandle = CreateFile(
    		DevName,
    		GENERIC_READ | GENERIC_WRITE,
    		0,
    		NULL,
    		OPEN_EXISTING,
    		0,
    		NULL
    		);
    	if( hHandle == INVALID_HANDLE_VALUE )
    	{
            dwRet = GetLastError();
            //printf("Open device %02d failed: 0x%x\n", i, dwRet);
    	}
        else
        {
            dwRet = ERROR_SUCCESS;
			//printf("WLAN device %02d opened!\n", i);
        	if( OutputHandle )
        	{
        		*OutputHandle = hHandle;
        	}
            break;
        }
	}
    if (dwRet != ERROR_SUCCESS)
		dwRet = OpenArt2DeviceOld(OutputHandle);
	return dwRet;
}


DWORD CloseArt2Device( )
{
	DWORD dwRet = ERROR_SUCCESS;

	CloseHandle( DevHandle );

	return dwRet ;
}

DWORD 
Art2DeviceIoctl( 
	HANDLE 		DevHandle, 
	DWORD 		IoctlCode, 
	PUCHAR 		CustomData, 
	ULONG 		CustomDataLength,
	PUCHAR 		OutputBuffer,
	ULONG 		OutputBufferLength,
	PULONG 		BytesRet
	)
{
	DWORD dwRet = ERROR_SUCCESS;
	BOOL bRet;
	ULONG BytesReturned = 0;

	WaitForDeviceReady();

	if( NULL == DevHandle || 
		0 == IoctlCode ||
		(NULL != CustomData && 0 == CustomDataLength) ||
		(NULL == CustomData && 0 != CustomDataLength) )
	{
		return ERROR_INVALID_PARAMETER;
	}

	bRet = DeviceIoControl( DevHandle, 
							ART2_DEV_IOCTL_REQUEST,
							CustomData,
							CustomDataLength,
							OutputBuffer,
							OutputBufferLength,
							&BytesReturned,
							NULL );

	if( FALSE == bRet )
	{
		dwRet = GetLastError();
		if (dwRet != ERROR_GEN_FAILURE)	//don't know why this errors keeps displayed, but the device still funstions after several tries.
		{
			printf("Failed to perform device I/O control, error = %08X\n", dwRet);
		}
		return dwRet;
	}
#ifdef _DEBUG
	//printf("Device I/O control finished, bytes ret: %d\n", BytesReturned);
#endif

	if( BytesRet )
	{
		*BytesRet = BytesReturned;
	}

	return dwRet;
}

BOOLEAN
OpenBusInterface (
    __in    HDEVINFO                    HardwareDeviceInfo,
    __in	PSP_DEVICE_INTERFACE_DATA   DeviceInterfaceData,
	__out	PHANDLE						OutputHandle
    )
{
    HANDLE                              file;
    PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
    ULONG                               predictedLength = 0;
    ULONG                               requiredLength = 0;

    //
    // Allocate a function class device data structure to receive the
    // information about this particular device.
    //

    SetupDiGetDeviceInterfaceDetail (
            HardwareDeviceInfo,
            DeviceInterfaceData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node

	if( ERROR_INSUFFICIENT_BUFFER != GetLastError() ) 
	{
        printf(
			"Error in SetupDiGetDeviceInterfaceDetail%d\n",
            GetLastError()
			);
        return FALSE;
    }

    predictedLength = requiredLength;

    deviceInterfaceDetailData = malloc (predictedLength);

    if(deviceInterfaceDetailData) {
        deviceInterfaceDetailData->cbSize =
                      sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
    } else {
        printf("Couldn't allocate %d bytes for device interface details.\n", predictedLength);
        return FALSE;
    }


    if (! SetupDiGetDeviceInterfaceDetail (
               HardwareDeviceInfo,
               DeviceInterfaceData,
               deviceInterfaceDetailData,
               predictedLength,
               &requiredLength,
               NULL)) {
        printf("Error in SetupDiGetDeviceInterfaceDetail\n");
        free (deviceInterfaceDetailData);
        return FALSE;
    }

    //printf("Opening %ws\n", deviceInterfaceDetailData->DevicePath);

    file = CreateFile ( deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ, // Only read access
                        0, // FILE_SHARE_READ | FILE_SHARE_WRITE
                        NULL, // no SECURITY_ATTRIBUTES structure
                        OPEN_EXISTING, // No special create flags
                        0, // No special attributes
                        NULL); // No template file

    if (INVALID_HANDLE_VALUE == file) {
        printf("CreateFile failed: 0x%x", GetLastError());
        free (deviceInterfaceDetailData);
        return FALSE;
    }

    //printf("Bus interface opened!!!\n");

	if( OutputHandle )
	{
		*OutputHandle = file;
	}

    free (deviceInterfaceDetailData);
    return TRUE;
}
