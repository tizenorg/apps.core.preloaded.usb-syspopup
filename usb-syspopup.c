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

#include <stdio.h>
#include <appcore-efl.h>
#include <Ecore_X.h>
#include <devman.h>
#include <heynoti.h>
#include <time.h>
#include "usb-syspopup.h"

#define SYSPOPUP_PARAM_LEN 3
#define USB_SYSPOPUP_MESSAGE_DOMAIN \
			"usb-syspopup"
#define USB_NOTICE_SYSPOPUP_FAIL \
			"USB system popup failed."

char *matchedApps[MAX_NUM_OF_MATCHED_APPS];
Elm_Genlist_Item_Class itc;
Evas_Object *genlist;
Elm_Object_Item *item;

syspopup_handler handler = {
	.def_term_fn = NULL,
	.def_timeout_fn = NULL
};

int ipc_socket_client_init(int *sock_remote)
{
	__USB_FUNC_ENTER__ ;
	if (!sock_remote) return -1;
	int t, len;
	struct sockaddr_un remote;
	char str[SOCK_STR_LEN];

	if (((*sock_remote) = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		USB_LOG(USB_LOG_VERBOSE, "FAIL: socket(AF_UNIX, SOCK_STREAM, 0)");
		return -1;;
	}
	remote.sun_family = AF_UNIX;
	strncpy(remote.sun_path, SOCK_PATH, strlen(SOCK_PATH)+1);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);

	if (connect((*sock_remote), (struct sockaddr *)&remote, len) == -1) {
		perror("connect");
		USB_LOG(USB_LOG_VERBOSE, "FAIL: connect((*sock_remote), (struct sockaddr *)&remote, len)");
		return -1;
	}
	__USB_FUNC_EXIT__ ;
	return 0;
}

int ipc_socket_client_close(int *sock_remote)
{
	__USB_FUNC_ENTER__ ;
	if (!sock_remote) return -1;
	close (*sock_remote);
	__USB_FUNC_EXIT__ ;
	return 0;
}
static void __win_del(void *data, Evas_Object * obj, void *event)
{
	__USB_FUNC_ENTER__ ;
	elm_exit();
	__USB_FUNC_EXIT__ ;
}

static Evas_Object *__create_win(const char *name)
{
	__USB_FUNC_ENTER__ ;

	if(!name) return ;
	Evas_Object *eo;
	int w;
	int h;

	eo = elm_win_add(NULL, name, ELM_WIN_DIALOG_BASIC);
	if (eo) {
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);
		elm_win_alpha_set(eo, EINA_TRUE);
		elm_win_raise(eo);
		evas_object_smart_callback_add(eo, "delete,request",
						   __win_del, NULL);
		ecore_x_window_size_get(ecore_x_window_root_first_get(),
					&w, &h);
		evas_object_resize(eo, w, h);
	}

	__USB_FUNC_EXIT__ ;

	return eo;
}

static void usb_chgdet_cb(void *data)
{
	__USB_FUNC_ENTER__ ;
	int val;
	if(device_get_property(DEVTYPE_JACK,JACK_PROP_USB_ONLINE,&val)==0)
	{
		USB_LOG(USB_LOG_VERBOSE, "device_get_property(): %d\n", val);
		if(!val)
		{
			USB_LOG(USB_LOG_VERBOSE, "Calling elm_exit()\n");
			elm_exit();
		}
	}
	__USB_FUNC_EXIT__ ;
}

static int register_heynoti(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return -1;
	struct appdata *ad = data;

	/* Registering heynoti to recognize the change of the status of USB cable connection */
	ad->noti_fd = heynoti_init();
	if (ad->noti_fd < 0)
	{
		USB_LOG(USB_LOG_VERBOSE, "FAIL: heynoti_init()\n");
		__USB_FUNC_EXIT__ ;
		return -1;
	}
	if(heynoti_attach_handler(ad->noti_fd) < 0)
	{
		USB_LOG(USB_LOG_VERBOSE, "FAIL: heynoti_attach_handler()\n");
		__USB_FUNC_EXIT__ ;
		return -1;
	}
	if (heynoti_subscribe(ad->noti_fd,"device_usb_chgdet", usb_chgdet_cb, NULL) != 0)
	{
		USB_LOG(USB_LOG_VERBOSE, "FAIL: heynoti_subscribe()\n");
		__USB_FUNC_EXIT__ ;
		return -1;
	}
	__USB_FUNC_EXIT__ ;
	return 0;
}

static void deregister_heynoti(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return ;
	struct appdata *ad = data;

	/* Closing heynoti */
	if (heynoti_unsubscribe(ad->noti_fd, "device_usb_chgdet", usb_chgdet_cb) < 0)
	{
		USB_LOG(USB_LOG_VERBOSE, "ERROR: heynoti_unsubscribe() \n");
	}
	if (heynoti_detach_handler(ad->noti_fd) < 0)
	{
		USB_LOG(USB_LOG_VERBOSE, "ERROR: heynoti_detach_handler() \n");
	}
	heynoti_close(ad->noti_fd);

	__USB_FUNC_EXIT__ ;

}

static int __app_create(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return -1;
	struct appdata *ad = data;
	int r;
	int ret = -1;

	register_heynoti(ad);

	/* When USB cable is removed before registering heynoti, syspopup should be terminated */
	usb_chgdet_cb(NULL);

	/* init internationalization */
	r = appcore_set_i18n(PACKAGE, LOCALEDIR);
	if (r != 0)
	{
		USB_LOG(USB_LOG_VERBOSE, "FAIL: appcore_set_i18n(PACKAGE, LOCALEDIR)\n");
		return -1;
	}

	__USB_FUNC_EXIT__ ;

	return 0;
}

static void unload_popup(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return ;
	struct appdata *ad = data;

	if (ad->lbtn) {
		evas_object_del(ad->lbtn);
		ad->lbtn = NULL;
	}
	if (ad->rbtn) {
		evas_object_del(ad->rbtn);
		ad->rbtn = NULL;
	}
	if (ad->popup) {
		evas_object_del(ad->popup);
		ad->popup = NULL;
	}
	if (ad->win) {
		evas_object_del(ad->win);
		ad->win = NULL;
	}

	__USB_FUNC_EXIT__ ;
}

static int __app_terminate(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return -1;
	struct appdata *ad = data;
	int ret = -1;

	USB_LOG(USB_LOG_VERBOSE, "[SYSPOPUP] %s, %d\n", __func__, __LINE__);

	deregister_heynoti(ad);
	unload_popup(ad);

	if (ad->b)
	{
		ret = bundle_free(ad->b);
		if (ret != 0 )
		{
			USB_LOG(USB_LOG_VERBOSE, "FAIL: bundle_free(ad->b)\n");
		}
		ad->b = NULL;
	}

	__USB_FUNC_EXIT__ ;
	return 0;
}

static int __app_pause(void *data)
{
	__USB_FUNC_ENTER__ ;
	__USB_FUNC_EXIT__ ;
	return 0;
}

static int __app_resume(void *data)
{
	__USB_FUNC_ENTER__ ;
	__USB_FUNC_EXIT__ ;
	return 0;
}

int request_to_usb_server(int request, char *pkgName, char *answer)
{
	__USB_FUNC_ENTER__ ;
	int sock_remote;
	char str[SOCK_STR_LEN];
	int t, ret;
	ret = ipc_socket_client_init(&sock_remote);
	if (0 != ret) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: ipc_socket_client_init(&sock_remote)");
		return -1;
	}
	if(LAUNCH_APP_FOR_ACC == request) {
		snprintf(str, SOCK_STR_LEN, "%d|%s", request, pkgName);
	} else {
		snprintf(str, SOCK_STR_LEN, "%d|", request);
	}
	USB_LOG(USB_LOG_VERBOSE, "request: %s", str);
	if (send (sock_remote, str, strlen(str)+1, 0) == -1) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: send (sock_remote, str, strlen(str), 0)");
		ipc_socket_client_close(&sock_remote);
		return -1;
	}
	if ((t = recv(sock_remote, answer, SOCK_STR_LEN, 0)) > 0) {
		answer[t] = '\0';
		USB_LOG(USB_LOG_VERBOSE, "[CLIENT] Received value: %s", answer);
	} else {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: recv(sock_remote, str, SOCK_STR_LEN, 0)");
		return -1;
	}
	ipc_socket_client_close(&sock_remote);
	__USB_FUNC_EXIT__ ;
	return 0;
}

static void load_connection_failed_popup_ok_response_cb(void *data, 
								Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;

	if(!data) return ;
	struct appdata *ad = data;
	unload_popup(ad);

	evas_object_smart_callback_del(ad->lbtn, "clicked", load_connection_failed_popup_ok_response_cb);
	char buf[SOCK_STR_LEN];
	int ret = request_to_usb_server(ERROR_POPUP_OK_BTN, NULL, buf);
	if (ret < 0) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: request_to_usb_server(ERROR_POPUP_OK_BTN, NULL, buf)\n");
		return;
	}

	elm_exit();

	__USB_FUNC_EXIT__ ;

}

static void request_perm_popup_yes_response_cb(void *data,
								Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	if (!data) return ;
	struct appdata *ad = (struct appdata *)data;
	unload_popup(ad);
	evas_object_smart_callback_del(ad->lbtn, "clicked", request_perm_popup_yes_response_cb);
	evas_object_smart_callback_del(ad->rbtn, "clicked", request_perm_popup_no_response_cb);

	char buf[SOCK_STR_LEN];
	int ret = request_to_usb_server(REQ_ACC_PERM_NOTI_YES_BTN, NULL, buf);
	if (ret < 0) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: request_to_usb_server(NOTICE_YES_BTN, NULL, buf)\n");
		return;
	}
	elm_exit();
	__USB_FUNC_EXIT__ ;
}

static void request_perm_popup_no_response_cb(void *data,
								Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	if (!data) return ;
	struct appdata *ad = (struct appdata *)data;
	unload_popup(ad);
	evas_object_smart_callback_del(ad->lbtn, "clicked", request_perm_popup_yes_response_cb);
	evas_object_smart_callback_del(ad->rbtn, "clicked", request_perm_popup_no_response_cb);

	char buf[SOCK_STR_LEN];
	int ret = request_to_usb_server(REQ_ACC_PERM_NOTI_NO_BTN, NULL, buf);
	if (ret < 0) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: request_to_usb_server(NOTICE_NO_BTN, NULL, buf)\n");
		return;
	}
	elm_exit();
	__USB_FUNC_EXIT__ ;
}

static void load_connection_failed_popup(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return ;
	struct appdata *ad = data;
	int ret = -1;
	ad->lbtn = NULL;

	/* create window */
	ad->win = __create_win(PACKAGE);
	if (ad->win == NULL)
		return ;

	ret = syspopup_create(ad->b, &handler, ad->win, ad);
	if(ret == 0)
	{
		ad->content = dgettext(USB_SYSPOPUP_MESSAGE_DOMAIN, "IDS_USB_POP_USB_CONNECTION_FAILED");
		USB_LOG(USB_LOG_VERBOSE, "ad->content is (%s)\n", ad->content);

		evas_object_show(ad->win);
		ad->popup = elm_popup_add(ad->win);
		evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_object_text_set(ad->popup, ad->content);

		/* a button */
		ad->lbtn = elm_button_add(ad->popup);
		elm_object_text_set(ad->lbtn, dgettext("sys_string","IDS_COM_SK_OK"));
		elm_object_part_content_set(ad->popup, "button1", ad->lbtn);
		evas_object_smart_callback_add(ad->lbtn, "clicked",
						load_connection_failed_popup_ok_response_cb, ad);

		evas_object_show(ad->popup);
	}
	else
	{
		USB_LOG(USB_LOG_VERBOSE, "syspopup_create() returns an integer which is not 0\n");
	}

	__USB_FUNC_EXIT__ ;
}

static int get_accessory_matched(struct appdata* ad)
{
	__USB_FUNC_ENTER__ ;
	int numOfApps = 0;
	matchedApps[0]=strdup("acc_test");
	numOfApps++;


	__USB_FUNC_EXIT__ ;
	return numOfApps;
}

static char *_gl_text_get(void *data, Evas_Object *obj, const char *part)
{
	__USB_FUNC_ENTER__ ;
	int index = (int)data;
	USB_LOG(USB_LOG_VERBOSE, "App name: %s\n", matchedApps[index]);
	__USB_FUNC_EXIT__ ;
	return strdup(matchedApps[index]);
}

static void select_app_popup_cancel_response_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__ ;
	if (!data) return ;
	struct appdata *ad = (struct appdata *)data;
	unload_popup(ad);
	elm_exit();
	__USB_FUNC_EXIT__ ; 
}

static int send_sel_pkg_to_usb_server(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;
	int ret = -1;
	char answer[SOCK_STR_LEN];
	ret = request_to_usb_server(LAUNCH_APP_FOR_ACC, ad->selPkg, answer);
	if (0 != ret) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: request_to_usb_server(LAUNCH_APP, ad->selPkg, answer)");
		return -1;
	}
	USB_LOG(USB_LOG_VERBOSE, "Launching app result is %s\n", answer);
	__USB_FUNC_EXIT__ ;
	return 0;
}

static void select_app_popup_gl_select_cb(void *data, Evas_Object *obj, void *event_info)
{
	__USB_FUNC_ENTER__ ;
	if (!event_info) return;
	if (!data) return;
	struct appdata *ad = (struct appdata *)data;
	int index = 0;
	Elm_Object_Item *item = (Elm_Object_Item *)event_info;
	if (item) {
		index = (int)elm_object_item_data_get(item);
		USB_LOG(USB_LOG_VERBOSE, "Selected Item: %d: %s\n", index, matchedApps[index]);
		snprintf(ad->selPkg, PKG_NAME_LEN, "%s", matchedApps[index]);
	}
	unload_popup(ad);
	int ret = send_sel_pkg_to_usb_server(ad);
	if ( 0!= ret ) USB_LOG(USB_LOG_VERBOSE,"FAIL: send_sel_pkg_to_usb_server(ad)");
	elm_exit();
	
	__USB_FUNC_EXIT__ ;
}

static void load_popup_to_select_app(struct appdata *ad, int numOfApps)
{
	__USB_FUNC_ENTER__ ;
	if(!ad) return ;
	int ret = -1;
	ad->lbtn = NULL;
	int index;
	Evas_Object *win;

	/* create window */
	win = __create_win(PACKAGE);
	if (win == NULL)
		return ;
	ad->win = win;

	ret = syspopup_create(ad->b, &handler, ad->win, ad);
	USB_LOG(USB_LOG_VERBOSE, "ret: %d\n", ret);
	if(ret == 0) {
		evas_object_show(ad->win);
		ad->popup = elm_popup_add(ad->win);
		elm_object_style_set(ad->popup,"menustyle");
		elm_object_part_text_set(ad->popup, "title,text", "Select app to launch");
		evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		ad->lbtn = elm_button_add(ad->popup);
		elm_object_text_set(ad->lbtn, "Cancel");
		elm_object_part_content_set(ad->popup, "button1", ad->lbtn);
		evas_object_smart_callback_add(ad->lbtn, "clicked", select_app_popup_cancel_response_cb, ad);
	
		itc.item_style = "1text";
		itc.func.text_get = _gl_text_get;
		itc.func.content_get = NULL;
		itc.func.state_get = NULL;
		itc.func.del = NULL;
		genlist = elm_genlist_add(ad->popup);
		evas_object_size_hint_weight_set(genlist, EVAS_HINT_EXPAND,	EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);
		for (index = 0; index < numOfApps; index++) {
			USB_LOG(USB_LOG_VERBOSE, "%d\n", numOfApps);
			item = elm_genlist_item_append(genlist, &itc, (void *) index, NULL,
				ELM_GENLIST_ITEM_NONE, select_app_popup_gl_select_cb, ad);
			if (NULL == item) USB_LOG(USB_LOG_VERBOSE, "NULL ==item\n");
		}
		elm_object_content_set(ad->popup, genlist);
		evas_object_show(ad->popup);
	}
	__USB_FUNC_EXIT__ ; 
}

static void load_popup_to_confirm_uri(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;
	__USB_FUNC_EXIT__ ;
}

static int get_accessory_info(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;
	if (!ad) return -1;
	int ret, i;
	char device[ACC_INFO_NUM][ACC_ELEMENT_LEN];
	char key[SYSPOPUP_PARAM_LEN];
	for (i = 0; i < ACC_INFO_NUM; i++) {
		snprintf(key, SYSPOPUP_PARAM_LEN, "%d", 1 + i);
		const char* type = bundle_get_val(ad->b, (const char *)key);
		if (!type) {
			USB_LOG(USB_LOG_VERBOSE, "ERROR: bundle_get_val(b)\n");
			return -1;
		} else {
			USB_LOG(USB_LOG_VERBOSE, "%d: %s\n", i, type);
			snprintf(device[i], ACC_ELEMENT_LEN, "%s", type);
		}
	}

	USB_LOG(USB_LOG_VERBOSE, "Get USB Acc info\n");
	snprintf(ad->usbAcc->manufacturer, ACC_ELEMENT_LEN, "%s", device[ACC_MANUFACTURER]);
	snprintf(ad->usbAcc->model, ACC_ELEMENT_LEN, "%s", device[ACC_MODEL]);
	snprintf(ad->usbAcc->description, ACC_ELEMENT_LEN, "%s", device[ACC_DESCRIPTION]);
	snprintf(ad->usbAcc->version, ACC_ELEMENT_LEN, "%s", device[ACC_VERSION]);
	snprintf(ad->usbAcc->uri, ACC_ELEMENT_LEN, "%s", device[ACC_URI]);
	snprintf(ad->usbAcc->serial, ACC_ELEMENT_LEN, "%s", device[ACC_SERIAL]);

	USB_LOG(USB_LOG_VERBOSE, "Print USB acc info\n");
	USB_LOG(USB_LOG_VERBOSE, "** USB ACCESSORY INFO **");
	USB_LOG(USB_LOG_VERBOSE, "* Manufacturer: %s *", ad->usbAcc->manufacturer);
	USB_LOG(USB_LOG_VERBOSE, "* Model       : %s *", ad->usbAcc->model);
	USB_LOG(USB_LOG_VERBOSE, "* Description : %s *", ad->usbAcc->description);
	USB_LOG(USB_LOG_VERBOSE, "* Version     : %s *", ad->usbAcc->version);
	USB_LOG(USB_LOG_VERBOSE, "* URI         : %s *", ad->usbAcc->uri);
	USB_LOG(USB_LOG_VERBOSE, "* SERIAL      : %s *", ad->usbAcc->serial);
	USB_LOG(USB_LOG_VERBOSE, "************************");
	__USB_FUNC_EXIT__ ;
	return 0;
}

static void load_select_pkg_for_acc_popup(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;
	if (!ad) return;
	int ret = -1;
	UsbAccessory usbAcc;
	memset(&usbAcc, 0x0, sizeof(UsbAccessory));
	ad->usbAcc = &usbAcc;
	ret = get_accessory_info(ad);
	if (0 != ret) {
		USB_LOG(USB_LOG_VERBOSE, "FAIL: get_accessory_info(ad)");
		elm_exit();
	}

	int numOfApps = get_accessory_matched(ad);
	if (numOfApps > 0) {
		USB_LOG(USB_LOG_VERBOSE, "number of apps matched: %d\n", numOfApps);
		load_popup_to_select_app(ad, numOfApps);
	} else {
		USB_LOG(USB_LOG_VERBOSE, "number of apps matched is 0\n");
		load_popup_to_confirm_uri(ad);
	}
	__USB_FUNC_EXIT__ ;
}

void load_request_perm_popup(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;

	if(!ad) return ;
	Evas_Object *win;
	int ret = -1;
	ad->lbtn = NULL;
	ad->rbtn = NULL;

	/* create window */
	win = __create_win(PACKAGE);
	if (win == NULL)
		return ;
	ad->win = win;

	ret = syspopup_create(ad->b, &handler, ad->win, ad);
	if(ret == 0)
	{
		ad->content = dgettext(USB_SYSPOPUP_MESSAGE_DOMAIN,
						"IDS_COM_POP_ALLOW_APPLICATION_P1SS_TO_ACCESS_USB_ACCESSORY_Q_ABB");
		USB_LOG(USB_LOG_VERBOSE, "ad->content is (%s)\n", ad->content);

		evas_object_show(ad->win);
		ad->popup = elm_popup_add(ad->win);
		evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_object_text_set(ad->popup, ad->content);

		/* Left button */
		ad->lbtn = elm_button_add(ad->popup);
		elm_object_text_set(ad->lbtn, dgettext("sys_string","IDS_COM_SK_YES"));
		elm_object_part_content_set(ad->popup, "button1", ad->lbtn);
		evas_object_smart_callback_add(ad->lbtn, "clicked", request_perm_popup_yes_response_cb, ad);

		/* Right button */
		ad->rbtn = elm_button_add(ad->popup);
		elm_object_text_set(ad->rbtn, dgettext("sys_string","IDS_COM_SK_NO"));
		elm_object_part_content_set(ad->popup, "button2", ad->rbtn);
		evas_object_smart_callback_add(ad->rbtn, "clicked", request_perm_popup_no_response_cb, ad);

		evas_object_show(ad->popup);
	}
	else
	{
		USB_LOG(USB_LOG_VERBOSE, "syspopup_create() returns an integer which is not 0\n");
	}

	__USB_FUNC_EXIT__ ;
}

static int __app_reset(bundle *b, void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return -1;
	struct appdata *ad = data;
	char key[SYSPOPUP_PARAM_LEN];
	int ret = 0;

	ad->b = bundle_dup(b);

	/* When syspopup is already loaded, remove the popup and load new popup */
	if (syspopup_has_popup(b)) {
		USB_LOG(USB_LOG_VERBOSE, "syspopup_has_popup(b) returns 1\n");
		unload_popup(ad);
		/* Resetting all proporties of syspopup */
		syspopup_reset(b);
	}

	snprintf(key, SYSPOPUP_PARAM_LEN, "%d", SYSPOPUP_TYPE);
	const char* type = bundle_get_val(b, (const char *)key);
	if (!type) {
		USB_LOG(USB_LOG_VERBOSE, "ERROR: Non existing type of popup\n");
		elm_exit();
	} else {
		ad->type = atoi(type);
		USB_LOG(USB_LOG_VERBOSE, "ad->type is (%d)\n", ad->type);
	}

	switch(ad->type) {
	case ERROR_POPUP:
		USB_LOG(USB_LOG_VERBOSE, "Connection failed popup is loaded\n");
		load_connection_failed_popup(ad);
		break;
	case SELECT_PKG_FOR_ACC_POPUP:
		USB_LOG(USB_LOG_VERBOSE, "Select pkg for acc popup is loaded\n");
		load_select_pkg_for_acc_popup(ad);
		break;
	case REQ_ACC_PERM_POPUP:
		USB_LOG(USB_LOG_VERBOSE, "Request Permission popup is loaded\n");
		load_request_perm_popup(ad);
	default:
		USB_LOG(USB_LOG_VERBOSE, "ERROR: The popup type(%d) does not exist\n", ad->type);
		break;
	}

	__USB_FUNC_EXIT__ ;

	return 0;
}

int main(int argc, char *argv[])
{
	__USB_FUNC_ENTER__ ;

	struct appdata ad;
	struct appcore_ops ops = {
		.create = __app_create,
		.terminate = __app_terminate,
		.pause = __app_pause,
		.resume = __app_resume,
		.reset = __app_reset,
	};

	memset(&ad, 0x0, sizeof(struct appdata));
	ops.data = &ad;

	__USB_FUNC_EXIT__ ;
	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}

