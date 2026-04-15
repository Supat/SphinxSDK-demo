// ConsoleDemo.cpp 
//
#include "stdafx.h"

#include <Winsock2.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include <math.h>
#include <iphlpapi.h>


#include <math.h>
#include <conio.h>

#include <string>
#include <exception>

#include "SphinxLib.h"   // include SphinxLib header file
#include "darknoise.h"
#include "bayer.h"
#include "wrist.h"
#include "tcp_server.h"

#define RING_BUFFER  1
#define BUFFER_COUNT 4

// Wrist-estimation + broadcast configuration.
static const char* WRIST_MODEL_PATH        = "hand_landmark_full.onnx";
static const float WRIST_FOREARM_AXIS_DEG  = 0.0f;   // direction of forearm in image (deg, +X = 0)
static const float WRIST_NEUTRAL_THRESH_DEG = 5.0f;  // |angle| <= this -> "neutral"
static const bool  WRIST_FLIP_SIGN         = false;
static const bool  WRIST_SHOW_PREVIEW      = true;   // OpenCV imshow window with overlay
static const uint16_t TCP_PORT             = 5555;

typedef struct
{
	BOOL Kill;
  BYTE Device;

}DEVICE_PARAMS, *PDEVICE_PARAMS;

DEVICE_PARAMS DeviceParams;
HANDLE ThreadAcquisition;
DWORD WINAPI ThreadProc(LPVOID lpv);

ULONGLONG last_img_timestamp;

void get_error(BYTE camera, WORD error, char *err_str);
unsigned short save_bmp(char *fname, DWORD w, DWORD h, BYTE bpp, BYTE *ppixel);
BYTE WINAPI error_callback_func(BYTE cam_nr, char *error_str);
BYTE WINAPI msg_callback_func(BYTE cam_nr, MESSAGECHANNEL_PARAMETER mcparam);

int main(int argc, char* argv[])
{
  WORD error;
  DISCOVERY dis;
  int i;
  CONNECTION con;
  char l_str[50];
	BYTE camera = 1;
  struct sockaddr_in addr;
  DWORD dw;

  // discovery devices
  error = GEVDiscovery(&dis,NULL,200,0);
	if(error)
	{
		printf("[ERROR] - GEVDiscovery error: %04X\n",error);
    return(0);
	}

	if(dis.Count == 0)
  {
    printf("[INFO] - No CANCamGige found.\n");
    return(0);
  }

  printf("[INFO] - Count: %d\n", dis.Count);

  // print out device info
  for(i = 0;i < dis.Count;i++)
  {
    printf("\n\n");
    printf("[INFO] - Device: %d\n",i+1);

    printf("[INFO] - %s\n",dis.param[i].manuf);
    printf("[INFO] - %s\n",dis.param[i].model);
    printf("[INFO] - %s\n",dis.param[i].version);

    addr.sin_addr.s_addr = dis.param[i].IP;
    printf("[INFO] - IP: %s\n", inet_ntoa(addr.sin_addr));

    addr.sin_addr.s_addr = dis.param[i].AdapterIP;
    printf("[INFO] - Adapter-IP: %s\n",inet_ntoa(addr.sin_addr));

    addr.sin_addr.s_addr = dis.param[i].AdapterMask;
    printf("[INFO] - Adapter-Mask: %s\n",inet_ntoa(addr.sin_addr));

    printf("[INFO] - Adapter-Name: %s\n", dis.param[i].adapter_name);
  }
  if(dis.Count > 1)
  {
    printf("Select Device: ");
    scanf("%d", &i);
    camera = i;
    printf("camera: %d\n",camera);
    fflush(stdin);
  }
  con.AdapterIP = dis.param[camera-1].AdapterIP;
  con.AdapterMask = dis.param[camera-1].AdapterMask;
 	con.IP_CANCam = dis.param[camera-1].IP;
  con.PortCtrl = 49149;    // set 0 to port than automatic port is use
  con.PortData = 49150;    // set 0 to port than automatic port is use
  strcpy(con.adapter_name,dis.param[camera-1].adapter_name);  

  // init GigE device
  // error_callback_func, this function gets information, warning and error strings from SphinxLib 
  error = GEVInit(camera, &con, error_callback_func, 0,EXCLUSIVE_ACCESS);
	if(error)
	{
		get_error(camera, error,l_str);
		printf("[ERROR] - GEVInit: %s\n",l_str);
		return(0);
	}

//  GEVSetDetailedLog(camera, DETAILED_LOG_INFO);

#ifdef _DEBUG
    // if debug set heartbeat to 10 seconds
    error = GEVSetHeartbeatRate(camera, 10000);
  	if(error)
	  {
		  get_error(camera, error,l_str);
		  printf("GEVSetHeartbeatRate error: %s\n",l_str);
		  goto END;
	  }
#endif

  // init xml 
  error = GEVInitXml(camera);
	if(error)
	{
		get_error(camera, error,l_str);
		printf("[ERROR] - GEVInitXml error: %s\n",l_str);
		goto END;
	}

  // set message channel callback function
  error = GEVSetMessageChannelCallback(camera, msg_callback_func);
  if(error == GEV_STATUS_NOT_SUPPORTED)
  {
    printf("[INFO] - Message channel not Supported\n");
  }

  // open stream channel
  error = GEVOpenStreamChannel(camera, con.AdapterIP, con.PortData,0 );
	if(error)
	{
		get_error(camera, error,l_str);
		printf("[ERROR] - GEVOpenStreamChannel error: %s\n",l_str);
		goto END;

	}
    // find the maxium possible packetsize
    WORD val64;
    error = GEVTestFindMaxPacketSize(camera, &val64,1400, 9000,4);
    if (error)
    {
      get_error(camera, error, l_str); 
      printf("[ERROR] -  GEVTestFindMaxPacketSize(): %s\n", l_str);
    }

  // disable packet resend
  error = GEVSetPacketResend(camera, 0);
	if(error)
	{
		get_error(camera, error,l_str);
		printf("[ERROR] - GEVSetPacketResend: %s\n",l_str);
		//goto END;
	}

  printf("[INFO] - Start acquisition continuous mode with any key.\n");
  printf("[INFO] - Cancel acquisition with any key.\n");
  _getch();

  // start thread
  DeviceParams.Kill = FALSE;
  DeviceParams.Device = camera;
  ThreadAcquisition = CreateThread(NULL,0,ThreadProc, &DeviceParams,0,&dw);
  if(ThreadAcquisition == NULL)
    goto END;

  // wait of key
	while(!_kbhit())
  		Sleep(0);
  _getch();

  DeviceParams.Kill = TRUE;

  // wait end thread
  if(WaitForSingleObject(ThreadAcquisition, 2000) != WAIT_OBJECT_0)
    printf("timeout close thread 1\n");
  CloseHandle( ThreadAcquisition );

END:

  printf("[INFO] - Exit application with any key.\n");
  _getch();

  // close stream channel  
  GEVCloseStreamChannel(camera);
  // close GigE device
 	GEVClose(camera);
	return 0;
}

void get_error(BYTE camera, WORD error, char *err_str)
{
  switch(error)
  {
      case GEV_STATUS_NOT_IMPLEMENTED: strcpy(err_str,"STATUS_NOT_IMPLEMENTED"); break;
      case GEV_STATUS_INVALID_PARAMETER: strcpy(err_str,"STATUS_INVALID_PARAMETER"); break;
      case GEV_STATUS_INVALID_ADDRESS: strcpy(err_str,"STATUS_INVALID_ADDRESS"); break;
      case GEV_STATUS_WRITE_PROTECT: strcpy(err_str,"STATUS_WRITE_PROTECT"); break;
      case GEV_STATUS_BAD_ALIGNMENT: strcpy(err_str,"STATUS_BAD_ALIGNMENT"); break;
      case GEV_STATUS_ACCESS_DENIED: strcpy(err_str,"STATUS_ACCESS_DENIED"); break;
      case GEV_STATUS_BUSY: strcpy(err_str,"STATUS_BUSY"); break;
      case GEV_STATUS_LOCAL_PROBLEM: strcpy(err_str,"STATUS_LOCAL_PROBLEM"); break;
      case GEV_STATUS_MSG_MISMATCH: strcpy(err_str,"STATUS_MSG_MISMATCH"); break;
      case GEV_STATUS_INVALID_PROTOCOL: strcpy(err_str,"STATUS_INVALID_PROTOCOL"); break;
      case GEV_STATUS_NO_MSG: strcpy(err_str,"STATUS_NO_MSG"); break;
      case GEV_STATUS_PACKET_UNAVAILABLE: strcpy(err_str,"STATUS_PACKET_UNAVAILABLE"); break;
      case GEV_STATUS_DATA_OVERRUN: strcpy(err_str,"STATUS_DATA_OVERRUN"); break;
      case GEV_STATUS_INVALID_HEADER: strcpy(err_str,"STATUS_INVALID_HEADER"); break;
      case GEV_STATUS_WRONG_CONFIG: strcpy(err_str,"STATUS_WRONG_CONFIG"); break;
      case GEV_STATUS_PACKET_NOT_YET_AVAILABLE: strcpy(err_str,"STATUS_PACKET_NOT_YET_AVAILABLE"); break;
      case GEV_STATUS_PACKET_AND_PREV_REMOVED_FROM_MEMORY: strcpy(err_str,"STATUS_PACKET_AND_PREV_REMOVED_FROM_MEMORY"); break;
      case GEV_STATUS_PACKET_REMOVED_FROM_MEMORY: strcpy(err_str,"STATUS_PACKET_REMOVED_FROM_MEMORY"); break;
      case GEV_STATUS_ERROR: strcpy(err_str,"STATUS_ERROR"); break;

      case GEV_STATUS_CAMERA_NOT_INIT: strcpy(err_str,"STATUS_CAMERA_NOT_INIT"); break;
      case GEV_STATUS_CAMERA_ALWAYS_INIT: strcpy(err_str,"STATUS_CAMERA_ALWAYS_INIT"); break;
      case GEV_STATUS_CANNOT_CREATE_SOCKET: strcpy(err_str,"STATUS_CANNOT_CREATE_SOCKET"); break;
      case GEV_STATUS_SEND_ERROR: strcpy(err_str,"STATUS_SEND_ERROR"); break;
      case GEV_STATUS_RECEIVE_ERROR: strcpy(err_str,"STATUS_RECEIVE_ERROR"); break;
      case GEV_STATUS_CANNOT_ALLOC_MEMORY: strcpy(err_str,"STATUS_CANNOT_ALLOC_MEMORY"); break;
      case GEV_STATUS_TIMEOUT: strcpy(err_str,"STATUS_TIMEOUT"); break;
      case GEV_STATUS_SOCKET_ERROR: strcpy(err_str,"STATUS_SOCKET_ERROR"); break;
      case GEV_STATUS_INVALID_ACK: strcpy(err_str,"STATUS_INVALID_ACK"); break;
      case GEV_STATUS_CANNOT_START_THREAD: strcpy(err_str,"STATUS_CANNOT_START_THREAD"); break;
      case GEV_STATUS_CANNOT_SET_SOCKET_OPT: strcpy(err_str,"STATUS_CANNOT_SET_SOCKET_OPT"); break;
      case GEV_STATUS_CANNOT_OPEN_DRIVER: strcpy(err_str,"STATUS_CANNOT_OPEN_DRIVER"); break;
      case GEV_STATUS_HEARTBEAT_READ_ERROR: strcpy(err_str,"STATUS_HEARTBEAT_READ_ERROR"); break;
      case GEV_STATUS_EVALUATION_EXPIRED: strcpy(err_str,"STATUS_EVALUATION_EXPIRED"); break;
      case GEV_STATUS_GRAB_ERROR: strcpy(err_str,"STATUS_GRAB_ERROR"); break;
      case GEV_STATUS_XML_READ_ERROR: strcpy(err_str,"STATUS_XML_READ_ERROR"); break;
      case GEV_STATUS_XML_OPEN_ERROR: strcpy(err_str,"STATUS_XML_OPEN_ERROR"); break;
      case GEV_STATUS_XML_FEATURE_ERROR: strcpy(err_str,"STATUS_XML_FEATURE_ERROR"); break;
      case GEV_STATUS_XML_COMMAND_ERROR: strcpy(err_str,"STATUS_XML_COMMAND_ERROR"); break;
      case GEV_STATUS_GAIN_NOT_SUPPORTED: strcpy(err_str,"STATUS_GAIN_NOT_SUPPORTED"); break;
      case GEV_STATUS_EXPOSURE_NOT_SUPPORTED: strcpy(err_str,"STATUS_EXPOSURE_NOT_SUPPORTED"); break;
      case GEV_STATUS_CANNOT_GET_ADAPTER_INFO: strcpy(err_str,"STATUS_CANNOT_GET_ADAPTER_INFO"); break;
      case GEV_STATUS_ERROR_INVALID_HANDLE: strcpy(err_str,"STATUS_ERROR_INVALID_HANDLE"); break;
      case GEV_STATUS_CLINK_SET_BAUD: strcpy(err_str,"STATUS_CLINK_SET_BAUD"); break;
      case GEV_STATUS_CLINK_SEND_BUFFER_FULL: strcpy(err_str,"STATUS_CLINK_SEND_BUFFER_FULL"); break;
      case GEV_STATUS_CLINK_RECEIVE_BUFFER_NO_DATA: strcpy(err_str,"STATUS_CLINK_REVEICE_BUFFER_NO_DATA"); break;
      case GEV_STATUS_FEATURE_NOT_AVAILABLE: strcpy(err_str,"STATUS_FEATURE_NOT_AVAILABLE"); break;
      case GEV_STATUS_MATH_PARSER_ERROR: strcpy(err_str,"STATUS_MATH_PARSER_ERROR"); break;
      case GEV_STATUS_FEATURE_ITEM_NOT_AVAILABLE: strcpy(err_str,"STATUS_FEATURE_ITEM_NOT_AVAILABLE"); break;
      case GEV_STATUS_NOT_SUPPORTED: strcpy(err_str,"STATUS_NOT_SUPPORTED"); break;
      case GEV_STATUS_GET_URL_ERROR: strcpy(err_str,"STATUS_GET_URL_ERROR"); break;
      case GEV_STATUS_READ_XML_MEM_ERROR: strcpy(err_str,"STATUS_READ_XML_MEM_ERROR"); break;
      case GEV_STATUS_XML_SIZE_ERROR: strcpy(err_str,"STATUS_XML_SIZE_ERROR"); break;
      case GEV_STATUS_XML_ZIP_ERROR: strcpy(err_str,"STATUS_XML_ZIP_ERROR"); break;
      case GEV_STATUS_XML_ROOT_ERROR: strcpy(err_str,"STATUS_XML_ROOT_ERROR"); break;
      case GEV_STATUS_XML_FILE_ERROR: strcpy(err_str,"STATUS_XML_FILE_ERROR"); break;
      case GEV_STATUS_DIFFERENT_IMAGE_HEADER: strcpy(err_str,"STATUS_DIFFERENT_IMAGE_HEADER"); break;
      case GEV_STATUS_XML_SCHEMA_ERROR: strcpy(err_str,"STATUS_XML_SCHEMA_ERROR"); break;
      case GEV_STATUS_XML_STYLESHEET_ERROR: strcpy(err_str,"STATUS_XML_STYLESHEET_ERROR"); break;
      case GEV_STATUS_FEATURE_LIST_ERROR: strcpy(err_str,"STATUS_FEATURE_LIST_ERROR"); break;
      case GEV_STATUS_ALREADY_OPEN: strcpy(err_str,"STATUS_ALLREADY_OPEN"); break;
      case GEV_STATUS_TEST_PACKET_DATA_ERROR: strcpy(err_str,"STATUS_TEST_PACKET_DATA_ERROR"); break;
      case GEV_STATUS_FEATURE_NOT_FLOAT: strcpy(err_str,"STATUS_FEATURE_NOT_FLOAT"); break;
      case GEV_STATUS_XML_DLL_NOT_FOUND: strcpy(err_str,"STATUS_XML_DLL_NOT_FOUND"); break;
      case GEV_STATUS_XML_NOT_INIT: strcpy(err_str,"STATUS_XML_NOT_INIT"); break;
      default: strcpy(err_str,"UNKNOWN");  printf("error: %X",error); break;
  }
}

unsigned short save_bmp(char *fname, DWORD w, DWORD h, BYTE bpp, BYTE *ppixel)
{
	BITMAPFILEHEADER bmpfilehdr;
	BITMAPINFOHEADER bmpinfohdr;
	int i,bpp_h,h_off;
	int size, width, height;
	FILE *hfile;
	BYTE *phelp,lbpp;
  RGBQUAD pal1;

	if ((hfile = fopen(fname,"w+b" )) == NULL)
		return(1);

	if(bpp == 16)	
		lbpp = 8;
	else
		lbpp = bpp;

  bpp_h = lbpp / 8;
  if(lbpp == 8)
   h_off = 1024;
  else
   h_off = 0;

  width  = (int)w;
  height = (int)h;
  size   = (width * bpp_h) * height;

	// build bmp headers
	bmpfilehdr.bfType = 0x4D42;
	bmpfilehdr.bfSize = size + sizeof(BITMAPINFOHEADER) + (sizeof(BITMAPFILEHEADER)) + h_off;
	bmpfilehdr.bfReserved1 = 0;
	bmpfilehdr.bfReserved2 = 0;
	bmpfilehdr.bfOffBits = sizeof(BITMAPINFOHEADER) + (sizeof(BITMAPFILEHEADER)) + h_off;

	bmpinfohdr.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfohdr.biWidth = width;
	bmpinfohdr.biHeight = height;
	bmpinfohdr.biPlanes = 1;
	bmpinfohdr.biBitCount = lbpp;
	bmpinfohdr.biCompression = 0;
	bmpinfohdr.biSizeImage = size;
	bmpinfohdr.biXPelsPerMeter = 0;
	bmpinfohdr.biYPelsPerMeter = 0;

   if(bpp == 8)
   {
	  bmpinfohdr.biClrUsed = 256;
      bmpinfohdr.biClrImportant = 256;
   }
   else
   {
      bmpinfohdr.biClrUsed = 0;
      bmpinfohdr.biClrImportant = 0;
   }

	if(fwrite( (unsigned char *)&bmpfilehdr, sizeof(BITMAPFILEHEADER),1,hfile) == 0)
		return(2);

	if(fwrite( (unsigned char *)&bmpinfohdr, sizeof(BITMAPINFOHEADER),1,hfile) == 0)
		return(2);

   if(lbpp == 8)
   {
      // Palette init.
      for(i = 0;i < 256;i++)
      {
         pal1.rgbRed = (unsigned char)i;
         pal1.rgbGreen = (unsigned char)i;
         pal1.rgbBlue = (unsigned char)i;
         pal1.rgbReserved = 0;
         if(fwrite(&pal1, sizeof(RGBQUAD),1,hfile) == 0)
            return(2);
      }
   }
   
   for(i = height - 1; i >= 0 ;i--)
   {	
	  phelp = ppixel + ((width * bpp_h) * i);
	  if(fwrite(phelp,(width * bpp_h),1,hfile) == 0)
		  return(2);
   }
   fclose(hfile);

   return(0);
}

// error callback funtion
BYTE WINAPI error_callback_func(BYTE cam_nr, char *error_str)
{
  printf("%s\n",error_str);
	return(0);
}

// message channel callback funtion
BYTE WINAPI msg_callback_func(BYTE cam_nr, MESSAGECHANNEL_PARAMETER mcparam)
{
  char l_str[150], id_str[20];
  int i;

  sprintf(l_str,"[INFO] - Message Channel Device %d -> ",cam_nr);
  switch (mcparam.EventID)
  {
    case EVENT_TRIGGER: strcat(l_str,"trigger event");
                 break;
    case EVENT_START_EXPOSUE: strcat(l_str,"start of exposure");
                 break;
    case EVENT_STOP_EXPOSUE: strcat(l_str,"end of exposure");
                 break;
    case EVENT_START_TRANSFER: strcat(l_str,"stream channel start of transfer");
                 break;
    case EVENT_STOP_TRANSFER: strcat(l_str,"stream channel end of transfer");
                 break;
    case EVENT_PRIMARY_APP_SWITCH: strcat(l_str,"primary application switchover has been granted.");
                 break;
    case EVENT_LINK_SPEED_CHANGE: strcat(l_str,"indicates that the link speed has changed.");
                 break;
    case EVENT_ACTION_LATE: strcat(l_str,"execution of a Scheduled Action Command was late");
                 break;
    default:     if ((mcparam.EventID >= EVENT_ERROR_BEGIN) && (mcparam.EventID <= EVENT_ERROR_END))
                 {
                    strcat(l_str,"error event");
                    break;
                 }
                 if (mcparam.EventID >= EVENT_DEVICE_SPECIFIC)
                 {
                    strcat(l_str,"device-specific event: ");
                    sprintf(id_str,"0x%04X",mcparam.EventID);
                    strcat(l_str, id_str);
                    break;
                 }
                 strcpy(l_str,"unknown event");
                 break;
  }

  if(mcparam.DataLength)
  {
    printf("Event: len: %d\n",mcparam.DataLength);
    for(i = 0;i < mcparam.DataLength;i++)
      printf("Event: data: %02X\n",mcparam.Data[i]);
  }

  printf("%s\n", l_str);
	return(0);
}

// function to get the current timestamp from the camera
ULONGLONG getTimeStamp(BYTE device, ULONGLONG last_frame_timestamp)
{
    // start time
    float timecounter;

    INT64 GEV_now_timestamp;
    GEVSetFeatureCommand(device, (char*)"GevTimestampControlLatch", 1);

    // get the timestamp for now
    GEVGetFeatureInteger(device, (char*)"GevTimestampValue", &GEV_now_timestamp);

    // Work around! GevTimestampValue is truncated to 32bit in Sphinxlib >2.0.9?!
    if (GEV_now_timestamp < last_frame_timestamp)
        // GevTimestampValue is truncated to 32bit as the time should not run backwards.
        //so we reconstruct the full timestamp from the last frame timestamp
        if ((last_frame_timestamp % 0x00FFFFFFFFull) <  GEV_now_timestamp)
            GEV_now_timestamp = (last_frame_timestamp & 0xFFFFFFFF00000000ull) | (GEV_now_timestamp % 0x00FFFFFFFFull);
        else
            // we have a 32bit overrun between the 2 timesteps
            GEV_now_timestamp = ((last_frame_timestamp & 0xFFFFFFFF00000000ull) + 0x00000001000000ull) | (GEV_now_timestamp % 0x00FFFFFFFFull);

    return GEV_now_timestamp;
}


DWORD WINAPI ThreadProc(LPVOID lpv)
{
  IMAGE_HEADER img_header;
  INT64 dw64;  
  DWORD width, height, img_size;
  BYTE bpp;
  DWORD pixelFormat;
  WORD error;
  char l_str[150];
  static PDEVICE_PARAMS dparams;  
  BYTE *ppixel[BUFFER_COUNT];
  BYTE index = 0;
#ifdef RING_BUFFER
  int i;
#endif

  BYTE* image_alloc = NULL;
  BYTE* image = NULL;
  BYTE* darknoise_img = NULL;
  BYTE* darknoise_img_alloc = NULL;
  BYTE* rgb_live = NULL;  // demosaiced live frame for inference (Bayer cameras only)
  INT64 old_exposure_time;
  int dark_init_counter = 0;
  bool fpn;
  bool is_bayer = false;
  int bayer_order = 0;

  WristEstimator* wrist = NULL;
  TcpBroadcaster* tcp = NULL;
  try {
    wrist = new WristEstimator(WRIST_MODEL_PATH, WRIST_FOREARM_AXIS_DEG,
                               WRIST_NEUTRAL_THRESH_DEG, WRIST_FLIP_SIGN);
    printf("[INFO] - Wrist model loaded: %s\n", WRIST_MODEL_PATH);
  } catch (const std::exception& e) {
    printf("[ERROR] - Failed to load wrist model (%s): %s\n", WRIST_MODEL_PATH, e.what());
    printf("[INFO] - Continuing without wrist estimation.\n");
  }
  tcp = new TcpBroadcaster(TCP_PORT);
  printf("[INFO] - TCP broadcaster on 127.0.0.1:%u\n", (unsigned)TCP_PORT);

  ppixel[0] = NULL;

  dparams = (PDEVICE_PARAMS)lpv;

  error = GEVGetFeatureInteger(dparams->Device,"Width", &dw64);
  if(error)
  {
		get_error(dparams->Device, error,l_str);
 		printf("[ERROR] - GEVGetFeatureInteger(Width) error: %s\n",l_str);
		return(0);
  }
  width = (DWORD)dw64;

  // get image height
  error = GEVGetFeatureInteger(dparams->Device,"Height", &dw64);
  if(error)
  {
		get_error(dparams->Device, error,l_str);
		printf("[ERROR] - GEVGetFeatureInteger(Height) error: %s\n",l_str);
		return(0);
  }
  height = (DWORD)dw64;

  // get pixel format
  error = GEVGetFeatureInteger(dparams->Device, "PixelFormat", &dw64);
  if(error)
  {
		get_error(dparams->Device, error,l_str);
		printf("[ERROR] - GEVGetFeatureInteger(PixelFormat) error: %s\n",l_str);
		return(0);
  }
	pixelFormat = (DWORD)dw64;
	bpp = (BYTE)((GVSP_PIX_EFFECTIVE_PIXEL_SIZE_MASK & pixelFormat)>>(GVSP_PIX_EFFECTIVE_PIXEL_SIZE_SHIFT + 3)) * 8;

	printf("[INFO] - Width: %d\n",width);
	printf("[INFO] - Height: %d\n",height);
	printf("[INFO] - BPP: %d\n",bpp);
	printf("[INFO] - PixelFormat: %08X\n",pixelFormat);

  error = GEVGetFeatureInteger(dparams->Device,"PayloadSize", &dw64);
  if(error)
  {
		get_error(dparams->Device, error,l_str);
		printf("[ERROR] - GEVGetFeatureInteger(PayloadSize) error: %s\n",l_str);
		return(0);
  }
  img_size = (int)dw64;

  // check model name for FPN subtraction
  char model[255];
  error = GEVGetFeatureString(dparams->Device, "DeviceModelName", model);
  if (error)
  {
      get_error(dparams->Device, error, l_str);
      printf("[ERROR] - GEVGetFeatureString(DeviceModelName) error: %s\n", l_str);
      return(0);
  }
  if (0 == strncmp(model, "GVRD-MRC HighSpeed", 255))
      fpn = true;
  else fpn = false;

  if (pixelFormat == GVSP_PIX_BAYGR8 || pixelFormat == GVSP_PIX_BAYRG8) {
    is_bayer = true;
    bayer_order = (pixelFormat == GVSP_PIX_BAYGR8) ? BAYER_COLOR_FILTER_GBRG
                                                   : BAYER_COLOR_FILTER_BGGR;
    rgb_live = (BYTE*)malloc((size_t)width * height * 3);
  }


#ifdef RING_BUFFER
  // alloc memory for the ring buffer
  for(i = 0;i < BUFFER_COUNT;i++)
  {
    ppixel[i] = (BYTE *)malloc(img_size);
    GEVSetRingBuffer(dparams->Device, i, ppixel[i]);
    printf("[INFO] - Allocate Ringbuffer #%d.\n", i);
  }
#else
  // alloc memory for the image
  ppixel[0] = (BYTE *)malloc(img_size);
#endif

  //allocate result image and allign it
  image_alloc = (BYTE *)malloc(img_size + 16);
  if (image == NULL)
  {
    image_alloc = (BYTE *)malloc(img_size + 16); //extra space for allignment on 128bits
    image = image_alloc + ((int)image_alloc % 16);
  }
  // malloc has failed!
  if (image == NULL)
  {
    printf("[ERROR] - Image-Memory Allocation failed\n");
    return(0);
  }

  //allocate fpn-dark image and allign it 
  if (darknoise_img == NULL)
  {
    darknoise_img_alloc = (BYTE *)malloc(img_size + 16); //extra space for allignment on 128bits
    darknoise_img = darknoise_img_alloc + ((int)darknoise_img_alloc % 16);
  }
  // malloc has failed!
  if (darknoise_img == NULL)
  {
    printf("[ERROR] - FPN-Memory Allocation failed\n");
    return(0);
  }


  // start acquisition
  error = GEVAcquisitionStart(dparams->Device,0);
  if(error)
  {
    get_error(dparams->Device, error,l_str);
    printf("[ERROR] - GEVAcquisitionStart error: %s\n",l_str);
		return(0);
  }

  // get timstamp counter 
  // due to an error in this sphinxLib version we have to guess the high word part form a grabbed image
  // so we grap one at first
  // get image and header info
#ifdef RING_BUFFER
  error = GEVGetImageRingBuffer(dparams->Device, &img_header, &index);
#else
  error = GEVGetImageBuffer(dparams->Device, &img_header, ppixel[0]);
#endif
  if (error)
  {
      get_error(dparams->Device, error, l_str);
      printf("[ERROR] - GEVGetImage: %s\n", l_str);
  }
  else
  {
      printf("[INFO] - Timestampcounter is: %lld\n", getTimeStamp(dparams->Device, img_header.TimeStamp));
  }

  // get timer tick frequency
  error = GEVGetFeatureInteger(dparams->Device, "GevTimestampTickFrequencyValue", &dw64);
  if (error)
  {
      get_error(dparams->Device, error, l_str);
      printf("[ERROR] - GEVGetFeatureInteger(GevTimestampTickFrequencyValue) error: %s\n", l_str);
      return(0);
  }
  printf("[INFO] - TimerTickFrequency: %lld\n",(DWORD)dw64);


  // here starts the grab loop
  while ( !dparams->Kill )
  {
    // get image and header info
#ifdef RING_BUFFER
    error = GEVGetImageRingBuffer(dparams->Device,&img_header, &index);  
#else
    error = GEVGetImageBuffer(dparams->Device,&img_header, ppixel[0]);  
#endif
    if(error)                              
    {
		get_error(dparams->Device, error,l_str);
		printf("[ERROR] - GEVGetImage: %s\n",l_str);
    }
    else
    {
        // process image
        printf("[INFO] - Timestamp: %lld \tImage: %lld\n", img_header.TimeStamp, img_header.FrameCounter);
        if (!fpn)
          memcpy(image, ppixel[index], img_size);
        else
        {
            if (dark_init_counter > 10)
              {
                  darknoise_bw_subtract(image, ppixel[index], darknoise_img, width, height, img_size);
                  // now the actual image after darknoise correction is in image
                  // this image now ready for further processing or storing
              }
              else if (dark_init_counter == 10)
              {
                  //store darknoise image
                  memcpy(darknoise_img, ppixel[index], img_size);
                  GEVSetFeatureInteger(dparams->Device, "ExposureTime", old_exposure_time);
                  dark_init_counter++;
              }
              else if (dark_init_counter == 0)
              {
                  memcpy(image, ppixel[index], img_size);
                  GEVGetFeatureInteger(dparams->Device, "ExposureTime", &old_exposure_time);
                  GEVSetFeatureInteger(dparams->Device, "ExposureTime", 0);
                  dark_init_counter++;
              }
              else if (dark_init_counter < 10)
              {
                  memcpy(image, ppixel[index], img_size);
                  dark_init_counter++;
              }
       }
    }

    // wrist estimation + TCP broadcast + preview
    {
      const BYTE* infer_buf = image;
      int infer_channels = 1;
      if (is_bayer && rgb_live) {
        bayer_Bilinear(image, rgb_live, width, height, bayer_order);
        infer_buf = rgb_live;
        infer_channels = 3;
      }
      WristResult wr{};
      wr.frame = (uint64_t)img_header.FrameCounter;
      wr.timestamp = (uint64_t)img_header.TimeStamp;
      wr.class_name = "neutral";
      wr.src_width = width;
      wr.src_height = height;
      if (wrist) {
        try {
          wr = wrist->Estimate(infer_buf, width, height, infer_channels,
                               (uint64_t)img_header.FrameCounter,
                               (uint64_t)img_header.TimeStamp);
          std::string js = wrist->ToJson(wr);
          if (tcp) tcp->Broadcast(js);
          printf("[WRIST] - %s\n", js.c_str());
        } catch (const std::exception& e) {
          printf("[ERROR] - Wrist inference failed: %s\n", e.what());
        }
      }
      if (WRIST_SHOW_PREVIEW) {
        ShowWristPreview(infer_buf, width, height, infer_channels, wr);
      }
    }

#ifdef RING_BUFFER
    GEVQueueRingBuffer(dparams->Device, index);
#endif
  }

  // stop acquisition
  error = GEVAcquisitionStop(dparams->Device);
  if (error)                              
  {
    get_error(dparams->Device, error,l_str);
    printf("[ERROR] - GEVAcquisitionStop: %s\n",l_str);
  }

  // save image to disk 
  printf("[INFO] - Save last image to disk...\n");
  //
  if (pixelFormat != GVSP_PIX_BAYGR8 && pixelFormat != GVSP_PIX_BAYRG8)
  {
      error = save_bmp("image.bmp", width, height, bpp, image);
  }
  else  // save color image of HiRes camera
  {
      int pixelorder;
      if (pixelFormat == GVSP_PIX_BAYGR8)
          pixelorder = BAYER_COLOR_FILTER_GBRG;
      else
          pixelorder = BAYER_COLOR_FILTER_BGGR;
      BYTE* rgb_image = (BYTE*)malloc(width*height * 3);
      bayer_Bilinear(image, rgb_image, width, height, pixelorder);
      error = save_bmp("image.bmp", width, height, 3 * bpp, rgb_image);
      free(rgb_image);
  }
  if(error)
    printf("[ERROR] - Error %d save image.bmp\n",error);
  else
    printf("[INFO] - Save image ok\n");

  if (darknoise_img_alloc != NULL)
    free(darknoise_img_alloc);

  if (image_alloc != NULL)
    free(image_alloc);

  if (rgb_live != NULL)
    free(rgb_live);

  delete wrist;
  delete tcp;
  
#ifdef RING_BUFFER
  // free ring buffer memory
  if(ppixel[0])
  {
    for(i = 0;i < BUFFER_COUNT;i++)
      free(ppixel[i]);
  }
  GEVReleaseRingBuffer(dparams->Device);
#else
  // free image memory
  if(ppixel[0])
    free(ppixel[0]);
#endif

  return(0);
}


