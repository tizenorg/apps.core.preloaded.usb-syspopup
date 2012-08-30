/*
 * usb-syspopup
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact: Taeyoung Kim <ty317.kim@samsung.com>
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
 *
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

#if !defined(PACKAGE)
#define PACKAGE "usb_syspopup"
#endif

#include <dlog.h>
#define USB_LOG_VERBOSE    LOG_VERBOSE
#define USB_LOG_DEBUG      LOG_DEBUG
#define USB_LOG_INFO       LOG_INFO
#define USB_LOG_WARN       LOG_WARN
#define USB_LOG_ERROR      LOG_ERROR
#define USB_LOG_FATAL      LOG_FATAL

#define USB_TAG "USB_SYSPOPUP"
#define USB_LOG(log_level, format, args...)\
	LOG(log_level, USB_TAG, "[%s][Ln: %d] " format,__FILE__, __LINE__, ##args)
#define __USB_FUNC_ENTER__\
	USB_LOG(USB_LOG_DEBUG, "Entering: %s()\n", __func__)
#define __USB_FUNC_EXIT__\
	USB_LOG(USB_LOG_DEBUG, "Exit: %s()\n", __func__)

typedef enum {
	SYSPOPUP_TYPE = 0,
	MAX_NUM_SYSPOPUP_PARAM
	/* When we need to deliver other parameters to USB-syspopup
	 * add the types of parameters */
} SYSPOPUP_PARAM;

typedef enum {
	ERROR_POPUP = 0,
	SELECT_PKG_FOR_ACC_POPUP,
	REQ_ACC_PERM_POPUP,
	TEST_POPUP,
	MAX_NUM_SYSPOPUP_TYPE
	/* When we need to add other system popup,
	 * Write here the type of popup */
} POPUP_TYPE;

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
	GET_ACC_INFO
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

	UsbAccessory	*usbAcc;
	char			selPkg[PKG_NAME_LEN];

	/* add more variables here */
};

static void load_connection_failed_popup_ok_response_cb(void *data, Evas_Object * obj, void *event_info);
static void request_perm_popup_yes_response_cb(void *data, Evas_Object * obj, void *event_info);
static void request_perm_popup_no_response_cb(void *data, Evas_Object * obj, void *event_info);

#endif			  /* __SYSPOPUP_APP_H__ */

