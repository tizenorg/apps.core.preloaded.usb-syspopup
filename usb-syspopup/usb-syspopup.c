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

#include <stdio.h>
#include <appcore-efl.h>

#ifdef X11
#include <Ecore_X.h>
#endif
#ifdef HAVE_WAYLAND
#include <Ecore.h>
#include <Ecore_Wayland.h>
#endif

#include <devman.h>

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

static void load_connection_failed_popup_ok_response_cb(void *data, Evas_Object * obj, void *event_info);
static void request_perm_popup_yes_response_cb(void *data, Evas_Object * obj, void *event_info);
static void request_perm_popup_no_response_cb(void *data, Evas_Object * obj, void *event_info);


int ipc_socket_client_init(int *sock_remote)
{
	__USB_FUNC_ENTER__ ;
	if (!sock_remote) return -1;
	int len;
	struct sockaddr_un remote;

	if (((*sock_remote) = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		USB_LOG("FAIL: socket(AF_UNIX, SOCK_STREAM, 0)");
		return -1;;
	}
	remote.sun_family = AF_UNIX;
	strncpy(remote.sun_path, SOCK_PATH, strlen(SOCK_PATH)+1);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);

	if (connect((*sock_remote), (struct sockaddr *)&remote, len) == -1) {
		perror("connect");
		USB_LOG("FAIL: connect((*sock_remote), (struct sockaddr *)&remote, len)");
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

	if(!name) return NULL;
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
                #ifdef X11
	        Ecore_X_Window *xwin;
	        xwin = elm_win_xwindow_get(eo);
	        if (xwin != NULL)
	           ecore_x_window_size_get(ecore_x_window_root_first_get(), &w, &h);
	        else {
	        #endif
	        #ifdef HAVE_WAYLAND
	        Ecore_Wl_Window *wlwin;
	        wlwin = elm_win_wl_window_get(eo);
	        if (wlwin != NULL)
		  ecore_wl_screen_size_get(&w, &h);
	        #endif
	        #ifdef X11
	        }
	        #endif

		evas_object_resize(eo, w, h);
	}

	__USB_FUNC_EXIT__ ;

	return eo;
}

static void usp_usbclient_chgdet_cb(keynode_t *in_key, void *data)
{
	__USB_FUNC_ENTER__ ;
	int ret = -1;
	int usb_status = -1;

	ret = vconf_get_int(VCONFKEY_SYSMAN_USB_STATUS, &usb_status);
	if (ret == 0 && usb_status == VCONFKEY_SYSMAN_USB_DISCONNECTED) {
		USB_LOG("USB cable is not connected");
		elm_exit();
	}
	__USB_FUNC_EXIT__ ;
}

static void usp_usbhost_chgdet_cb(keynode_t *in_key, void *data)
{
	__USB_FUNC_ENTER__ ;
	int ret = -1;
	int usb_status = -1;

	ret = vconf_get_int(VCONFKEY_SYSMAN_USBHOST_STATUS, &usb_status);
	if (ret == 0 && usb_status == VCONFKEY_SYSMAN_USB_HOST_DISCONNECTED) {
		USB_LOG("USB host is not connected");
		elm_exit();
	}
	__USB_FUNC_EXIT__ ;
}

int usp_vconf_key_notify(struct appdata *ad)
{
	__USB_FUNC_ENTER__;
	if(!ad) return -1;
	int ret = -1;

	/* Event for USB cable */
	ret = vconf_notify_key_changed(VCONFKEY_SYSMAN_USB_STATUS, usp_usbclient_chgdet_cb, NULL);
	if (0 != ret) {
		USB_LOG("FAIL: vconf_notify_key_changed(VCONFKEY_SYSMAN_USB_STATUS)");
		return -1;
	}

	/* Event for USB host */
	ret = vconf_notify_key_changed(VCONFKEY_SYSMAN_USBHOST_STATUS, usp_usbhost_chgdet_cb, NULL);
	if (0 != ret) {
		USB_LOG("FAIL: vconf_notify_key_changed(VCONFKEY_SYSMAN_USBHOST_STATUS)");
		return -1;
	}

	__USB_FUNC_EXIT__ ;
	return 0;
}

int usp_vconf_key_ignore()
{
	__USB_FUNC_ENTER__;
	int ret = -1;

	/* Event for USB cable */
	ret = vconf_ignore_key_changed(VCONFKEY_SYSMAN_USB_STATUS, usp_usbclient_chgdet_cb);
	if (0 != ret) {
		USB_LOG("FAIL: vconf_ignore_key_changed(VCONFKEY_SYSMAN_USB_STATUS)");
		return -1;
	}

	/* Event for USB host */
	ret = vconf_ignore_key_changed(VCONFKEY_SYSMAN_USBHOST_STATUS, usp_usbhost_chgdet_cb);
	if (0 != ret) {
		USB_LOG("FAIL: vconf_ignore_key_changed(VCONFKEY_SYSMAN_USBHOST_STATUS)");
		return -1;
	}

	__USB_FUNC_EXIT__ ;
	return 0;
}

static int __app_create(void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return -1;
	struct appdata *ad = data;
	int ret = -1;

	ret = usp_vconf_key_notify(ad);
	if (ret != 0) USB_LOG("FAIL: usp_vconf_key_notify(ad)");

	/* init internationalization */
	ret = appcore_set_i18n(PACKAGE, LOCALEDIR);
	if (ret != 0)
	{
		USB_LOG("FAIL: appcore_set_i18n(PACKAGE, LOCALEDIR)\n");
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

	ret = usp_vconf_key_ignore();
	if (ret != 0) USB_LOG("FAIL: usp_vconf_key_ignore()");

	unload_popup(ad);

	if (ad->b)
	{
		ret = bundle_free(ad->b);
		if (ret != 0 )
		{
			USB_LOG("FAIL: bundle_free(ad->b)\n");
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
		USB_LOG("FAIL: ipc_socket_client_init(&sock_remote)");
		return -1;
	}
	if(LAUNCH_APP_FOR_ACC == request || LAUNCH_APP_FOR_HOST == request) {
		snprintf(str, SOCK_STR_LEN, "%d|%s", request, pkgName);
	} else {
		snprintf(str, SOCK_STR_LEN, "%d|", request);
	}
	USB_LOG("request: %s", str);
	if (send (sock_remote, str, strlen(str)+1, 0) == -1) {
		USB_LOG("FAIL: send (sock_remote, str, strlen(str), 0)");
		ipc_socket_client_close(&sock_remote);
		return -1;
	}
	if ((t = recv(sock_remote, answer, SOCK_STR_LEN, 0)) > 0) {
		if (t < SOCK_STR_LEN) {
			answer[t] = '\0';
		} else { /* t == SOCK_STR_LEN */
			answer[SOCK_STR_LEN-1] = '\0';
		}
		USB_LOG("[CLIENT] Received value: %s", answer);
	} else {
		USB_LOG("FAIL: recv(sock_remote, str, SOCK_STR_LEN, 0)");
		return -1;
	}
	ipc_socket_client_close(&sock_remote);
	__USB_FUNC_EXIT__ ;
	return 0;
}

static void load_connection_failed_popup_ok_response_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;

	if(!data) return ;
	struct appdata *ad = data;
	unload_popup(ad);

	evas_object_smart_callback_del(ad->lbtn, "clicked", load_connection_failed_popup_ok_response_cb);
	char buf[SOCK_STR_LEN];
	int ret = request_to_usb_server(ERROR_POPUP_OK_BTN, NULL, buf);
	if (ret < 0) {
		USB_LOG("FAIL: request_to_usb_server(ERROR_POPUP_OK_BTN, NULL, buf)\n");
		return;
	}

	elm_exit();

	__USB_FUNC_EXIT__ ;

}

static void request_perm_popup_yes_response_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	if (!data) return ;
	struct appdata *ad = (struct appdata *)data;
	int ret = -1;
	unload_popup(ad);
	evas_object_smart_callback_del(ad->lbtn, "clicked", request_perm_popup_yes_response_cb);
	evas_object_smart_callback_del(ad->rbtn, "clicked", request_perm_popup_no_response_cb);

	char buf[SOCK_STR_LEN];
	if (REQ_ACC_PERM_POPUP == ad->type)
		ret = request_to_usb_server(REQ_ACC_PERM_NOTI_YES_BTN, NULL, buf);
	else if (REQ_HOST_PERM_POPUP == ad->type)
		ret = request_to_usb_server(REQ_HOST_PERM_NOTI_YES_BTN, NULL, buf);
	else
		ret = request_to_usb_server(-1, NULL, buf);

	if (ret < 0) {
		USB_LOG("FAIL: request_to_usb_server(NOTICE_YES_BTN, NULL, buf)\n");
		return;
	}
	elm_exit();
	__USB_FUNC_EXIT__ ;
}

static void request_perm_popup_no_response_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	if (!data) return ;
	struct appdata *ad = (struct appdata *)data;
	int ret = -1;
	unload_popup(ad);
	evas_object_smart_callback_del(ad->lbtn, "clicked", request_perm_popup_yes_response_cb);
	evas_object_smart_callback_del(ad->rbtn, "clicked", request_perm_popup_no_response_cb);

	char buf[SOCK_STR_LEN];
	if (REQ_ACC_PERM_POPUP == ad->type)
		ret = request_to_usb_server(REQ_ACC_PERM_NOTI_NO_BTN, NULL, buf);
	else if (REQ_HOST_PERM_POPUP == ad->type)
		ret = request_to_usb_server(REQ_HOST_PERM_NOTI_NO_BTN, NULL, buf);
	else
		ret = request_to_usb_server(-1, NULL, buf);

	if (ret < 0) {
		USB_LOG("FAIL: request_to_usb_server(NOTICE_NO_BTN, NULL, buf)\n");
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
		USB_LOG("ad->content is (%s)\n", ad->content);

		evas_object_show(ad->win);
		ad->popup = elm_popup_add(ad->win);
		evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_object_text_set(ad->popup, ad->content);

		/* a button */
		ad->lbtn = elm_button_add(ad->popup);
		elm_object_text_set(ad->lbtn, dgettext("sys_string","IDS_COM_SK_OK"));
		elm_object_part_content_set(ad->popup, "button1", ad->lbtn);
		evas_object_smart_callback_add(ad->lbtn, "clicked", load_connection_failed_popup_ok_response_cb, ad);

		evas_object_show(ad->popup);
	}
	else
	{
		USB_LOG("syspopup_create(b, &handler, ad->win, ad) returns an integer which is not 0\n");
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
	USB_LOG("App name: %s\n", matchedApps[index]);
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
	if (ad->type == SELECT_PKG_FOR_ACC_POPUP)
		ret = request_to_usb_server(LAUNCH_APP_FOR_ACC, ad->selPkg, answer);
	else if (ad->type == SELECT_PKG_FOR_HOST_POPUP)
		ret = request_to_usb_server(LAUNCH_APP_FOR_HOST, ad->selPkg, answer);
	if (0 != ret) {
		USB_LOG("FAIL: request_to_usb_server(LAUNCH_APP, ad->selPkg, answer)");
		return -1;
	}
	USB_LOG("Launching app result is %s\n", answer);
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
		USB_LOG("Selected Item: %d: %s\n", index, matchedApps[index]);
		snprintf(ad->selPkg, PKG_NAME_LEN, "%s", matchedApps[index]);
	}
	unload_popup(ad);
	int ret = send_sel_pkg_to_usb_server(ad);
	if ( 0!= ret ) USB_LOG("FAIL: send_sel_pkg_to_usb_server(ad)");
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
	USB_LOG("ret: %d\n", ret);
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
			USB_LOG("%d\n", numOfApps);
			item = elm_genlist_item_append(genlist, &itc, (void *) index, NULL,
				ELM_GENLIST_ITEM_NONE, select_app_popup_gl_select_cb, ad);
			if (NULL == item) USB_LOG("NULL ==item\n");
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
	int i;
	char device[ACC_INFO_NUM][ACC_ELEMENT_LEN];
	char key[SYSPOPUP_PARAM_LEN];
	for (i = 0; i < ACC_INFO_NUM; i++) {
		snprintf(key, SYSPOPUP_PARAM_LEN, "%d", 1 + i);
		const char* type = bundle_get_val(ad->b, (const char *)key);
		if (!type) {
			USB_LOG("ERROR: bundle_get_val(b)\n");
			return -1;
		} else {
			USB_LOG("%d: %s\n", i, type);
			snprintf(device[i], ACC_ELEMENT_LEN, "%s", type);
		}
	}

	USB_LOG("Get USB Acc info\n");
	snprintf(ad->usbAcc->manufacturer, ACC_ELEMENT_LEN, "%s", device[ACC_MANUFACTURER]);
	snprintf(ad->usbAcc->model, ACC_ELEMENT_LEN, "%s", device[ACC_MODEL]);
	snprintf(ad->usbAcc->description, ACC_ELEMENT_LEN, "%s", device[ACC_DESCRIPTION]);
	snprintf(ad->usbAcc->version, ACC_ELEMENT_LEN, "%s", device[ACC_VERSION]);
	snprintf(ad->usbAcc->uri, ACC_ELEMENT_LEN, "%s", device[ACC_URI]);
	snprintf(ad->usbAcc->serial, ACC_ELEMENT_LEN, "%s", device[ACC_SERIAL]);

	USB_LOG("Print USB acc info\n");
	USB_LOG("** USB ACCESSORY INFO **");
	USB_LOG("* Manufacturer: %s *", ad->usbAcc->manufacturer);
	USB_LOG("* Model       : %s *", ad->usbAcc->model);
	USB_LOG("* Description : %s *", ad->usbAcc->description);
	USB_LOG("* Version     : %s *", ad->usbAcc->version);
	USB_LOG("* URI         : %s *", ad->usbAcc->uri);
	USB_LOG("* SERIAL      : %s *", ad->usbAcc->serial);
	USB_LOG("************************");
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
		USB_LOG("FAIL: get_accessory_info(ad)");
		elm_exit();
	}

	int numOfApps = get_accessory_matched(ad);
	if (numOfApps > 0) {
		USB_LOG("number of apps matched: %d\n", numOfApps);
		load_popup_to_select_app(ad, numOfApps);
	} else {
		USB_LOG("number of apps matched is 0\n");
		load_popup_to_confirm_uri(ad);
	}
	__USB_FUNC_EXIT__ ;
}

static int get_host_matched(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;
	if (!ad) return 0;
	int numOfApps = 0;
	matchedApps[0]=strdup("host_test");
	numOfApps++;
	__USB_FUNC_EXIT__ ;
	return numOfApps;
}

static void load_select_pkg_for_host_popup(struct appdata *ad)
{
	__USB_FUNC_ENTER__ ;
	if (!ad) return;
	int numOfApps = get_host_matched(ad);
	if (numOfApps > 0) {
		USB_LOG("number of apps matched: %d\n", numOfApps);
		load_popup_to_select_app(ad, numOfApps);
	} else {
		USB_LOG("number of apps matched is 0\n");
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
		if (ad->type == REQ_ACC_PERM_POPUP)
			ad->content = dgettext(USB_SYSPOPUP_MESSAGE_DOMAIN, "IDS_COM_POP_ALLOW_APPLICATION_P1SS_TO_ACCESS_USB_ACCESSORY_Q_ABB");
		else if (ad->type == REQ_HOST_PERM_POPUP)
			ad->content = dgettext(USB_SYSPOPUP_MESSAGE_DOMAIN, "Allow application to access USB host?");
		else {
			ad->content = dgettext(USB_SYSPOPUP_MESSAGE_DOMAIN, "This app cannot access to the usb device");
		}
		USB_LOG("ad->content is (%s)\n", ad->content);

		evas_object_show(ad->win);
		ad->popup = elm_popup_add(ad->win);
		evas_object_size_hint_weight_set(ad->popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		elm_object_text_set(ad->popup, ad->content);

		if (ad->type == REQ_ACC_PERM_POPUP || ad->type == REQ_HOST_PERM_POPUP) {
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
		} else {
			/* OK button */
			ad->lbtn = elm_button_add(ad->popup);
			elm_object_text_set(ad->lbtn, dgettext("sys_string","IDS_COM_SK_OK"));
			elm_object_part_content_set(ad->popup, "button1", ad->lbtn);
			evas_object_smart_callback_add(ad->lbtn, "clicked", request_perm_popup_no_response_cb, ad);
		}

		evas_object_show(ad->popup);
	}
	else
	{
		USB_LOG("syspopup_create(b, &handler, ad->win, ad) returns an integer which is not 0\n");
	}

	__USB_FUNC_EXIT__ ;
}

static int __app_reset(bundle *b, void *data)
{
	__USB_FUNC_ENTER__ ;

	if(!data) return -1;
	struct appdata *ad = data;
	char key[SYSPOPUP_PARAM_LEN];

	ad->b = bundle_dup(b);

	/* When syspopup is already loaded, remove the popup and load new popup */
	if (syspopup_has_popup(b)) {
		USB_LOG("syspopup_has_popup(b) returns 1\n");
		unload_popup(ad);
		/* Resetting all proporties of syspopup */
		syspopup_reset(b);
	}

	snprintf(key, SYSPOPUP_PARAM_LEN, "%d", SYSPOPUP_TYPE);
	const char* type = bundle_get_val(b, (const char *)key);
	if (!type) {
		USB_LOG("ERROR: Non existing type of popup\n");
		elm_exit();
	} else {
		ad->type = atoi(type);
		USB_LOG("ad->type is (%d)\n", ad->type);
	}

	/* In case that USB cable/device is disconnected before launching popup,
	 * the connection status are checked according to the popup type */
	switch(ad->type) {
	case ERROR_POPUP:
	case SELECT_PKG_FOR_ACC_POPUP:
	case REQ_ACC_PERM_POPUP:
		ad->isClientOrHost = USB_DEVICE_CLIENT;
		usp_usbclient_chgdet_cb(NULL, NULL);
		break;
	case SELECT_PKG_FOR_HOST_POPUP:
	case REQ_HOST_PERM_POPUP:
		ad->isClientOrHost = USB_DEVICE_HOST;
		usp_usbhost_chgdet_cb(NULL, NULL);
		break;
	default:
		USB_LOG("ERROR: The popup type(%d) does not exist\n", ad->type);
		ad->isClientOrHost = USB_DEVICE_UNKNOWN;
		elm_exit();
		break;
	}

	switch(ad->type) {
	case ERROR_POPUP:
		USB_LOG("Connection failed popup is loaded\n");
		load_connection_failed_popup(ad);
		break;
	case SELECT_PKG_FOR_ACC_POPUP:
		USB_LOG("Select pkg for acc popup is loaded\n");
		load_select_pkg_for_acc_popup(ad);
		break;
	case SELECT_PKG_FOR_HOST_POPUP:
		USB_LOG("Select pkg for host popup is loaded\n");
		load_select_pkg_for_host_popup(ad);
		break;
	case REQ_ACC_PERM_POPUP:
	case REQ_HOST_PERM_POPUP:
		USB_LOG("Request Permission popup is loaded\n");
		load_request_perm_popup(ad);
		break;
	default:
		USB_LOG("ERROR: The popup type(%d) does not exist\n", ad->type);
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

