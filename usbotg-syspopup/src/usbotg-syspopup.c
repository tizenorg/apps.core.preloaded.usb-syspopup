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
#include <pmapi.h>
#include <assert.h>
#include "usbotg-syspopup.h"

#ifdef X11
#include <Ecore_X.h>
#endif
#ifdef HAVE_WAYLAND
#include <Ecore.h>
#include <Ecore_Wayland.h>
#endif

#include <appsvc.h>
#include <syspopup.h>
#include <vconf.h>

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/tmp/usb_server_sock"
#define SOCK_STR_LEN 1542
#define UNMOUNT_USB_STORAGE 60

#define USB_MOUNT_PATH   "/opt/storage/usb"
#define MYFILE_APP_NAME  "org.tizen.myfile"
#define GALLERY_APP_NAME "org.tizen.gallery"

#define STORAGE_ADD      "storage_add"
#define STORAGE_REMOVE   "storage_remove"
#define STORAGE_UNMOUNT  "storage_unmount"
#define CAMERA_ADD       "camera_add"
#define CAMERA_REMOVE    "camera_remove"

static int ipc_socket_client_init()
{
	__USB_FUNC_ENTER__ ;
	struct sockaddr_un remote;
	int sockFd;

	if ((sockFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		USB_LOG("FAIL: socket(AF_UNIX, SOCK_STREAM, 0)");
		return -1;;
	}
	remote.sun_family = AF_UNIX;
	strncpy(remote.sun_path, SOCK_PATH, sizeof(remote.sun_family));
	if (connect(sockFd, (struct sockaddr *)&remote, sizeof(remote)) == -1) {
		USB_LOG("FAIL: connect()");
		close(sockFd);
		return -1;
	}
	__USB_FUNC_EXIT__ ;
	return sockFd;
}

static int request_to_usb_server(int request, char *device, char *answer, int answerLen)
{
	__USB_FUNC_ENTER__ ;
	assert(device);
	assert(answer);
	assert(answerLen > 0);

	int sock_remote;
	char str[SOCK_STR_LEN];
	int t;
	sock_remote = ipc_socket_client_init();
	if (sock_remote < 0) {
		USB_LOG("FAIL: ipc_socket_client_init()");
		return -1;
	}

	snprintf(str, sizeof(str), "%d|%s", request, device);
	USB_LOG("request: %s", str);
	if (send (sock_remote, str, strlen(str)+1, 0) == -1) {
		USB_LOG("FAIL: send (sock_remote, str, strlen(str), 0)");
		close(sock_remote);
		return -1;
	}

	if ((t = recv(sock_remote, answer, answerLen, 0)) > 0) {
		if (t < answerLen) {
			answer[t] = '\0';
		} else { /* t == answerLen */
			answer[answerLen - 1] = '\0';
		}
		USB_LOG("[CLIENT] Received value: %s", answer);
	} else {
		USB_LOG("FAIL: recv(sock_remote, answer, answerLen, 0)");
		close(sock_remote);
		return -1;
	}

	close(sock_remote);
	__USB_FUNC_EXIT__ ;
	return 0;
}

/* App Life cycle funtions */
static void win_del(void *data, Evas_Object *obj, void *event)
{
	elm_exit();
}

static void uosp_free_evas_object(Evas_Object **eo)
{
	__USB_FUNC_ENTER__;
	assert(eo);
	if (!(*eo)) return;
	evas_object_del(*eo);
	*eo = NULL;
	__USB_FUNC_EXIT__;
}

static void unload_window(struct appdata *ad)
{
	__USB_FUNC_ENTER__;
	assert(ad);
	if (ad->storage_added_popup || ad->storage_unmount_popup || ad->camera_added_popup) {
		USB_LOG("More than one popup is loaded");
		return;
	}

	USB_LOG("unload window");
	uosp_free_evas_object(&(ad->win_main));
	elm_exit();

	__USB_FUNC_EXIT__;
}

/* Create main window */
static Evas_Object *create_win(const char *name)
{
	__USB_FUNC_ENTER__;
	Evas_Object *eo;
	int w, h;

	eo = elm_win_add(NULL, name, ELM_WIN_DIALOG_BASIC);
	if (eo) {
		elm_win_title_set(eo, name);
		elm_win_borderless_set(eo, EINA_TRUE);
		evas_object_smart_callback_add(eo, "delete,request", win_del, NULL);
		elm_win_alpha_set(eo, EINA_TRUE);

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

	__USB_FUNC_EXIT__;
	return eo;
}

static void uosp_usbhost_chgdet_cb(keynode_t *keynode, void *data)
{
	__USB_FUNC_ENTER__ ;
	assert(keynode);
	assert(data);
	struct appdata *ad = (struct appdata *)data;
	int usb_status;

	usb_status = vconf_keynode_get_int(keynode);
	if (usb_status == VCONFKEY_SYSMAN_USB_HOST_DISCONNECTED) {
		USB_LOG("USB host is not connected");
		unload_window(ad);
	}
	__USB_FUNC_EXIT__ ;
}

/* App init */
static int app_create(void *data)
{
	__USB_FUNC_ENTER__;
	struct appdata *ad = (struct appdata *)data;
	int ret;

	ret = vconf_notify_key_changed(VCONFKEY_SYSMAN_USB_HOST_STATUS,
					uosp_usbhost_chgdet_cb, ad);
	if (0 != ret) {
		USB_LOG("FAIL: vconf_notify_key_changed()");
		return -1;
	}

	/* init internationalization */
	if (0 !=  appcore_set_i18n(PACKAGE, LOCALEDIR)) {
		USB_LOG("FAIL: appcore_set_i18n(PACKAGE, LOCALEDIR)\n");
		return -1;
	}

	__USB_FUNC_EXIT__;
	return 0;

}

/* Terminate noti handler */
static int app_terminate(void *data)
{
	__USB_FUNC_ENTER__;
	struct appdata *ad = data;
	int ret;

	ret = vconf_ignore_key_changed(VCONFKEY_SYSMAN_USB_HOST_STATUS, uosp_usbhost_chgdet_cb);
	if (0 != ret) USB_LOG("FAIL: vconf_ignore_key_changed()");

	uosp_free_evas_object(&(ad->storage_added_popup));
	uosp_free_evas_object(&(ad->storage_unmount_popup));
	uosp_free_evas_object(&(ad->camera_added_popup));
	FREE(ad->storage_added_path);
	FREE(ad->storage_unmount_path);

	__USB_FUNC_EXIT__;
	return 0;
}

/* Pause/background */
static int app_pause(void *data)
{
	return 0;
}

/* Resume */
static int app_resume(void *data)
{
	return 0;
}

static void storage_browse_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	assert(data);

	struct appdata *ad = (struct appdata *)data;
	int ret;
	bundle *b;

	uosp_free_evas_object(&(ad->storage_added_popup));

	b = bundle_create();
	appsvc_set_operation(b, APPSVC_OPERATION_VIEW);
	if (ad->storage_added_path) {
		appsvc_add_data(b, "path", ad->storage_added_path);
	} else {
		appsvc_add_data(b, "path", USB_MOUNT_PATH);
	}
	appsvc_set_pkgname(b, MYFILE_APP_NAME);
	ret = appsvc_run_service(b, 0, NULL, (void*)NULL);
	if (ret < 0)
		USB_LOG("app launching fail(%d)", ret);
	bundle_free(b);

	FREE(ad->storage_added_path);
	unload_window(ad);

	__USB_FUNC_EXIT__;
}

static void storage_unmount_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	assert(data);

	struct appdata *ad = (struct appdata *)data;
	int ret;
	char buf[SOCK_STR_LEN];

	uosp_free_evas_object(&(ad->storage_unmount_popup));

	ret = request_to_usb_server(UNMOUNT_USB_STORAGE, ad->storage_unmount_path, buf, sizeof(buf));
	if (ret < 0) {
		USB_LOG("FAIL: request_to_usb_server()");
	}
	USB_LOG("buf: %s", buf);

	FREE(ad->storage_unmount_path);
	unload_window(ad);

	__USB_FUNC_EXIT__;
}

static void camera_browse_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	assert(data);

	struct appdata *ad = (struct appdata *)data;
	bundle *b;
	int ret;

	uosp_free_evas_object(&(ad->camera_added_popup));

	b = bundle_create();
	appsvc_set_operation(b, APPSVC_OPERATION_VIEW);
	appsvc_add_data(b, "album-id", "GALLERY_ALBUM_PTP_ID");
	appsvc_set_pkgname(b, GALLERY_APP_NAME);
	ret = appsvc_run_service(b, 0, NULL, (void*)NULL);
	if (ret < 0)
		USB_LOG("app launching fail(%d)", ret);
	bundle_free(b);

	unload_window(ad);

	__USB_FUNC_EXIT__;
}

static void storage_browse_cancel_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	assert(data);
	struct appdata *ad = (struct appdata *)data;

	uosp_free_evas_object(&(ad->storage_added_popup));
	FREE(ad->storage_added_path);
	unload_window(ad);

	__USB_FUNC_EXIT__;
}

static void storage_unmount_cancel_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	assert(data);
	struct appdata *ad = (struct appdata *)data;

	uosp_free_evas_object(&(ad->storage_unmount_popup));
	FREE(ad->storage_unmount_path);
	unload_window(ad);

	__USB_FUNC_EXIT__;
}

static void camera_browse_cancel_clicked_cb(void *data, Evas_Object * obj, void *event_info)
{
	__USB_FUNC_ENTER__;
	assert(data);
	struct appdata *ad = (struct appdata *)data;

	uosp_free_evas_object(&(ad->camera_added_popup));
	unload_window(ad);

	__USB_FUNC_EXIT__;
}

static int load_usbotg_popup(struct appdata *ad,
				bundle *b,
				Evas_Object **popup,
				char *mainText,
				char *lbtnText,
				Evas_Smart_Cb lbtn_cb,
				char *rbtnText,
				Evas_Smart_Cb rbtn_cb)
{
	__USB_FUNC_ENTER__;
	assert(ad && b && popup && mainText && lbtnText && lbtn_cb && rbtnText && rbtn_cb);

	Evas_Object *btn1;
	Evas_Object *btn2;
	Evas_Object *win;
	syspopup_handler handler = {
		.def_term_fn = NULL,
		.def_timeout_fn = NULL
	};

	/* create window */
	if (!(ad->win_main)) {
		win = create_win(PACKAGE);
		if (win == NULL) {
			USB_LOG("FAIL: create_win()");
			return -1;
		}
		ad->win_main = win;
		if (syspopup_create(b, &handler, ad->win_main, ad) != 0) {
			USB_LOG("FAIL: syspopup_create()");
			return -1;
		}
	}

	evas_object_show(ad->win_main);

	*popup = elm_popup_add(ad->win_main);
	evas_object_size_hint_weight_set(*popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_text_set(*popup, mainText);

	btn1 = elm_button_add(*popup);
	elm_object_text_set(btn1, lbtnText);
	elm_object_part_content_set(*popup, "button1", btn1);
	evas_object_smart_callback_add(btn1, "clicked", lbtn_cb, ad);

	btn2 = elm_button_add(*popup);
	elm_object_text_set(btn2, rbtnText);
	elm_object_part_content_set(*popup, "button2", btn2);
	evas_object_smart_callback_add(btn2, "clicked", rbtn_cb, ad);

	evas_object_show(*popup);

	__USB_FUNC_EXIT__;
	return 0;
}

static void load_storage_added_popup(struct appdata *ad, bundle *b)
{
	__USB_FUNC_ENTER__;
	assert(ad);
	assert(b);
	int ret;
	char *path;

	uosp_free_evas_object(&(ad->storage_added_popup));
	FREE(ad->storage_added_path);

	path = (char *)(bundle_get_val(b, "DEVICE_PATH"));
	if (!path) return;
	USB_LOG("storage_added_path: %s", path);

	ret = load_usbotg_popup(ad, b, &(ad->storage_added_popup),
			"Browse storage connected via USB?",
			"Browse", storage_browse_clicked_cb,
			"Cancel", storage_browse_cancel_clicked_cb);
	if (ret < 0) {
		USB_LOG("FAIL: load_usbotg_popup(storage_added)");
		unload_window(ad);
		return;
	}

	ad->storage_added_path = strdup(path);

	__USB_FUNC_EXIT__;
}

static void load_storage_unmount_popup(struct appdata *ad, bundle *b)
{
	__USB_FUNC_ENTER__;
	assert(ad);
	assert(b);
	int ret;
	char *path;

	uosp_free_evas_object(&(ad->storage_unmount_popup));
	FREE(ad->storage_unmount_path);

	path = (char *)(bundle_get_val(b, "DEVICE_PATH"));
	if (!path) return;
	USB_LOG("unmount_path: %s", path);

	/* If unmount path is same as storage_added_path, unload storage_added_popup */
	if (ad->storage_added_path
		&& !strncmp(path, ad->storage_added_path, strlen(ad->storage_added_path))) {
		uosp_free_evas_object(&(ad->storage_added_popup));
		FREE(ad->storage_added_path);
	}

	ret = load_usbotg_popup(ad, b, &(ad->storage_unmount_popup),
			"Unmount USB mass storage before removing to avoid data loss",
			"Unmount", storage_unmount_clicked_cb,
			"Cancel", storage_unmount_cancel_clicked_cb);
	if (ret < 0) {
		USB_LOG("FAIL: load_usbotg_popup(storage_unmount)");
		unload_window(ad);
	}

	ad->storage_unmount_path = strdup(path);

	__USB_FUNC_EXIT__;
}

static void load_camera_added_popup(struct appdata *ad, bundle *b)
{
	__USB_FUNC_ENTER__;
	assert(ad);
	assert(b);
	int ret;

	uosp_free_evas_object(&(ad->camera_added_popup));

	ret = load_usbotg_popup(ad, b, &(ad->camera_added_popup),
			"Browse camera connected via USB?",
			"Browse", camera_browse_clicked_cb,
			"Cancel", camera_browse_cancel_clicked_cb);
	if (ret < 0) {
		USB_LOG("FAIL: load_usbotg_popup(storage_unmount)");
		unload_window(ad);
	}

	__USB_FUNC_EXIT__;
}

static void unload_storage_added_unmount_popup(struct appdata *ad, bundle *b)
{
	__USB_FUNC_ENTER__;
	assert(ad);
	assert(b);
	char *path;

	path = (char *)(bundle_get_val(b, "DEVICE_PATH"));
	USB_LOG("removed_path: %s", path);
	if (!path) return;

	if (NULL == ad->storage_added_path
		|| !strncmp(path, ad->storage_added_path, strlen(ad->storage_added_path))) {
		uosp_free_evas_object(&(ad->storage_added_popup));
		FREE(ad->storage_added_path);
	}

	if (NULL == ad->storage_unmount_path
		|| !strncmp(path, ad->storage_unmount_path, strlen(ad->storage_unmount_path))) {
		uosp_free_evas_object(&(ad->storage_unmount_popup));
		FREE(ad->storage_unmount_path);
	}

	unload_window(ad);

	__USB_FUNC_EXIT__;
}

/* Reset */
static int app_reset(bundle *b, void *data)
{
	__USB_FUNC_ENTER__;
	assert(data);
	assert(b);

	struct appdata *ad = (struct appdata *)data;
	const char *opt = bundle_get_val(b, "POPUP_TYPE");
	if (!opt) {
		USB_LOG("FAIL: bundle_get_val()");
		unload_window(ad);
		return 0;
	}

	if (syspopup_has_popup(b)) {
		USB_LOG("usbotg-syspopup is already loaded");
		syspopup_reset(b);
	}

	if (pm_change_state(LCD_NORMAL) != 0) return -1;

	if (!strcmp(opt, STORAGE_ADD)) {
		load_storage_added_popup(ad, b);
		__USB_FUNC_EXIT__;
		return 0;
	}

	if (!strcmp(opt, STORAGE_REMOVE)) {
		unload_storage_added_unmount_popup(ad, b);
		__USB_FUNC_EXIT__;
		return 0;
	}

	if (!strcmp(opt, STORAGE_UNMOUNT)) {
		load_storage_unmount_popup(ad, b);
		__USB_FUNC_EXIT__;
		return 0;
	}

	if (!strcmp(opt, CAMERA_ADD)) {
		load_camera_added_popup(ad, b);
		__USB_FUNC_EXIT__;
		return 0;
	}

	if (!strcmp(opt, CAMERA_REMOVE)) {
		uosp_free_evas_object(&(ad->camera_added_popup));
		unload_window(ad);
		__USB_FUNC_EXIT__;
		return 0;
	}

	USB_LOG("FAIL: popup type is %s", opt);
	__USB_FUNC_EXIT__;
	return -1;
}

int main(int argc, char *argv[])
{
	__USB_FUNC_ENTER__;
	struct appdata ad;

	/* App life cycle management */
	struct appcore_ops ops = {
		.create = app_create,
		.terminate = app_terminate,
		.pause = app_pause,
		.resume = app_resume,
		.reset = app_reset,
	};

	memset(&ad, 0x0, sizeof(struct appdata));
	ops.data = &ad;

	__USB_FUNC_EXIT__;
	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}
