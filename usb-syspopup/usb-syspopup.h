/*
 * usb-syspopup
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SYSPOPUP_APP_H__
#define __SYSPOPUP_APP_H__

#include <vconf.h>
#include <syspopup.h>
#include <ail.h>

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/tmp/usb_server_sock"
#define SOCK_STR_LEN 1542
#define ACC_ELEMENT_LEN 256
#define PKG_NAME_LEN	64
#define ACC_INFO_NUM 6
#define MAX_NUM_OF_MATCHED_APPS 256
#define VCONFKEY_SYSMAN_USBHOST_STATUS "memory/sysman/usbhost_status"

#if !defined(PACKAGE)
#define PACKAGE "usb-syspopup"
#endif

#include <dlog.h>
#undef LOG_TAG
#define LOG_TAG "USB_SYSPOPUP"
#define USB_LOG(fmt, args...)   SLOGD(fmt, ##args)
#define __USB_FUNC_ENTER__      USB_LOG("ENTER")
#define __USB_FUNC_EXIT__       USB_LOG("EXIT")

typedef enum {
	SYSPOPUP_TYPE = 0,
	MAX_NUM_SYSPOPUP_PARAM
	/* When we need to deliver other parameters to USB-syspopup
	 * add the types of parameters */
} SYSPOPUP_PARAM;

typedef enum {
	ERROR_POPUP = 0,
	SELECT_PKG_FOR_ACC_POPUP,
	SELECT_PKG_FOR_HOST_POPUP,
	REQ_ACC_PERM_POPUP,
	REQ_HOST_PERM_POPUP,
	TEST_POPUP,
	MAX_NUM_SYSPOPUP_TYPE
	/* When we need to add other system popup,
	 * Write here the type of popup */
} POPUP_TYPE;

typedef enum {
	USB_DEVICE_CLIENT = 0,
	USB_DEVICE_HOST,
	USB_DEVICE_BOTH,
	USB_DEVICE_UNKNOWN
} IS_CLIENT_OR_HOST;

typedef enum {
	ACC_MANUFACTURER = 0,
	ACC_MODEL,
	ACC_DESCRIPTION,
	ACC_VERSION,
	ACC_URI,
	ACC_SERIAL
} ACC_ELEMENT;

typedef enum {
	/* General */
	ERROR_POPUP_OK_BTN = 0,
	IS_EMUL_BIN,

	/* for Accessory */
	LAUNCH_APP_FOR_ACC = 20,
	REQ_ACC_PERMISSION,
	HAS_ACC_PERMISSION,
	REQ_ACC_PERM_NOTI_YES_BTN,
	REQ_ACC_PERM_NOTI_NO_BTN,
	GET_ACC_INFO,

	/* for Host */
	LAUNCH_APP_FOR_HOST = 40,
	REQ_HOST_PERMISSION,
	HAS_HOST_PERMISSION,
	REQ_HOST_PERM_NOTI_YES_BTN,
	REQ_HOST_PERM_NOTI_NO_BTN,
	REQ_HOST_CONNECTION
} REQUEST_TO_USB_MANGER;

typedef struct _usbAccessory {
	char manufacturer[ACC_ELEMENT_LEN];
	char model[ACC_ELEMENT_LEN];
	char description[ACC_ELEMENT_LEN];
	char version[ACC_ELEMENT_LEN];
	char uri[ACC_ELEMENT_LEN];
	char serial[ACC_ELEMENT_LEN];
} UsbAccessory;

struct appdata {
	Evas_Object		*win;
	Evas_Object		*popup;
	Evas_Object		*lbtn;
	Evas_Object		*rbtn;
	bundle			*b;
	int				noti_fd;
	int				type;
	char			*content;
	int				isClientOrHost;

	UsbAccessory	*usbAcc;
	char			selPkg[PKG_NAME_LEN];

	/* add more variables here */
};

#endif			  /* __SYSPOPUP_APP_H__ */
