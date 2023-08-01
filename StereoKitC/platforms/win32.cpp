#include "win32.h"

#if defined(SK_OS_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "../stereokit.h"
#include "../_stereokit.h"
#include "../device.h"
#include "../sk_math.h"
#include "../asset_types/texture.h"
#include "../libraries/sokol_time.h"
#include "../libraries/stref.h"
#include "../systems/system.h"
#include "../systems/render.h"
#include "../systems/input_keyboard.h"
#include "../hands/input_hand.h"

namespace sk {

///////////////////////////////////////////

struct win32_state_t {
	HWND            window;
	HINSTANCE       hinst;
	HICON           icon;
	skg_swapchain_t swapchain;
	bool            swapchain_initialized;
	tex_format_     depth_format;
	float           scroll;
	LONG_PTR        openxr_base_winproc;
	system_t       *render_sys;

	// For managing window resizing
	bool check_resize;
	UINT resize_x;
	UINT resize_y;
};
win32_state_t local = {};

#if defined(SKG_OPENGL)
const int32_t win32_multisample = 1;
#else
const int32_t win32_multisample = 1;
#endif

// Constants for the registry key and value names
const wchar_t* REG_KEY_NAME   = L"Software\\StereoKit Simulator";
const wchar_t* REG_VALUE_NAME = L"WindowLocation";

///////////////////////////////////////////

void win32_resize(int width, int height) {
	width  = maxi(1, width);
	height = maxi(1, height);
	if (local.swapchain_initialized == false || (width == device_data.display_width && height == device_data.display_height))
		return;

	sk_info.display_width  = width;
	sk_info.display_height = height;
	device_data.display_width  = width;
	device_data.display_height = height;
	log_diagf("Resized to: %d<~BLK>x<~clr>%d", width, height);
	
	skg_swapchain_resize(&local.swapchain, device_data.display_width, device_data.display_height);
	render_update_projection();
}

///////////////////////////////////////////

void win32_physical_key_interact() {
	// On desktop, we want to hide soft keyboards on physical presses
	input_set_last_physical_keypress_time(time_totalf_unscaled());
	platform_keyboard_show(false, text_context_text);
}

///////////////////////////////////////////

bool win32_window_message_common(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_LBUTTONDOWN: if (sk_focus == app_focus_active) input_key_inject_press  (key_mouse_left);   return true;
	case WM_LBUTTONUP:   if (sk_focus == app_focus_active) input_key_inject_release(key_mouse_left);   return true;
	case WM_RBUTTONDOWN: if (sk_focus == app_focus_active) input_key_inject_press  (key_mouse_right);  return true;
	case WM_RBUTTONUP:   if (sk_focus == app_focus_active) input_key_inject_release(key_mouse_right);  return true;
	case WM_MBUTTONDOWN: if (sk_focus == app_focus_active) input_key_inject_press  (key_mouse_center); return true;
	case WM_MBUTTONUP:   if (sk_focus == app_focus_active) input_key_inject_release(key_mouse_center); return true;
	case WM_XBUTTONDOWN: input_key_inject_press  (GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? key_mouse_back : key_mouse_forward); return true;
	case WM_XBUTTONUP:   input_key_inject_release(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? key_mouse_back : key_mouse_forward); return true;
	case WM_KEYDOWN:     input_key_inject_press  ((key_)wParam); win32_physical_key_interact(); if ((key_)wParam == key_del) input_text_inject_char(0x7f); return true;
	case WM_KEYUP:       input_key_inject_release((key_)wParam); win32_physical_key_interact(); return true;
	case WM_SYSKEYDOWN:  input_key_inject_press  ((key_)wParam); win32_physical_key_interact(); return true;
	case WM_SYSKEYUP:    input_key_inject_release((key_)wParam); win32_physical_key_interact(); return true;
	case WM_CHAR:        input_text_inject_char   ((uint32_t)wParam); return true;
	case WM_MOUSEWHEEL:  if (sk_focus == app_focus_active) local.scroll += (short)HIWORD(wParam); return true;
	default: return false;
	}
}

///////////////////////////////////////////

LRESULT win32_openxr_winproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (!win32_window_message_common(message, wParam, lParam)) {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

///////////////////////////////////////////

bool win32_start_pre_xr() {
	return true;
}

///////////////////////////////////////////

bool win32_start_post_xr() {
	wchar_t *app_name_w = platform_to_wchar(sk_app_name);

	// Create a window just to grab input
	WNDCLASSW wc = {0}; 
	wc.lpfnWndProc   = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		if (!win32_window_message_common(message, wParam, lParam)) {
			switch (message) {
			case WM_CLOSE: sk_quit(); PostQuitMessage(0); break;
			default: return DefWindowProcW(hWnd, message, wParam, lParam);
			}
		}
		return (LRESULT)0;
	};
	wc.hInstance     = GetModuleHandle(NULL);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = app_name_w;
	wc.hIcon         = local.icon;
	if( !RegisterClassW(&wc) ) return false;

	local.window = CreateWindowW(
		app_name_w,
		app_name_w,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0, 0, 0,
		0, 0, 
		wc.hInstance,
		nullptr);

	sk_free(app_name_w);
	if (!local.window) {
		return false;
	}
	return true;
}

///////////////////////////////////////////

bool win32_init() {
	local = {};
	local.check_resize = true;
	local.render_sys   = systems_find("FrameRender");

	// Find the icon from the exe itself
	wchar_t path[MAX_PATH];
	if (GetModuleFileNameW(GetModuleHandle(nullptr), path, MAX_PATH) != 0)
		ExtractIconExW(path, 0, &local.icon, nullptr, 1);

	return true;
}

///////////////////////////////////////////

void win32_shutdown() {
	local = {};
}

///////////////////////////////////////////

bool win32_start_flat() {
	sk_info.display_type      = display_opaque;
	device_data.display_blend = display_blend_opaque;

	wchar_t *app_name_w = platform_to_wchar(sk_app_name);

	WNDCLASSW wc = {0};
	wc.lpfnWndProc   = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		if (!win32_window_message_common(message, wParam, lParam)) {
			switch(message) {
			case WM_CLOSE:      sk_quit(); PostQuitMessage(0);   break;
			case WM_SETFOCUS:   sk_focus = app_focus_active;     break;
			case WM_KILLFOCUS:  sk_focus = app_focus_background; break;
			case WM_MOUSEWHEEL: if (sk_focus == app_focus_active) local.scroll += (short)HIWORD(wParam); break;
			case WM_SYSCOMMAND: {
				// Has the user pressed the restore/'un-maximize' button?
				// WM_SIZE happens -after- this event, and contains the new size.
				if (GET_SC_WPARAM(wParam) == SC_RESTORE)
					local.check_resize = true;
			
				// Disable alt menu
				if (GET_SC_WPARAM(wParam) == SC_KEYMENU)
					return (LRESULT)0; 
			} return DefWindowProcW(hWnd, message, wParam, lParam);
			case WM_SIZE: {
				local.resize_x = (UINT)LOWORD(lParam);
				local.resize_y = (UINT)HIWORD(lParam);

				// Don't check every time the size changes, this can lead to ugly memory alloc.
				// If a restore event, a maximize, or something else says we should resize, check it!
				if (local.check_resize || wParam == SIZE_MAXIMIZED) {
					local.check_resize = false; 
					win32_resize(local.resize_x, local.resize_y);
				}
			} return DefWindowProcW(hWnd, message, wParam, lParam);
			case WM_EXITSIZEMOVE: {
				// If the user was dragging the window around, WM_SIZE is called -before- this 
				// event, so we can go ahead and resize now!
				win32_resize(local.resize_x, local.resize_y);
			} return DefWindowProcW(hWnd, message, wParam, lParam);
			default: return DefWindowProcW(hWnd, message, wParam, lParam);
			}
		}
		return (LRESULT)0;
	};
	wc.hInstance     = GetModuleHandleW(NULL);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = app_name_w;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = local.icon;
	if (!RegisterClassW(&wc)) { sk_free(app_name_w); return false; }
	local.hinst = wc.hInstance;

	RECT rect = {};
	HKEY reg_key;
	if (backend_xr_get_type() == backend_xr_type_simulator && RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY_NAME, 0, KEY_READ, &reg_key) == ERROR_SUCCESS) {
		DWORD data_size = sizeof(RECT);
		RegQueryValueExW(reg_key, REG_VALUE_NAME, 0, nullptr, (BYTE*)&rect, &data_size);
		RegCloseKey     (reg_key);
	} else {
		rect.left   = sk_settings.flatscreen_pos_x;
		rect.right  = sk_settings.flatscreen_pos_x + sk_settings.flatscreen_width;
		rect.top    = sk_settings.flatscreen_pos_y;
		rect.bottom = sk_settings.flatscreen_pos_y + sk_settings.flatscreen_height;
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, false);
	}
	if (rect.right == rect.left) rect.right  = rect.left + sk_settings.flatscreen_width;
	if (rect.top == rect.bottom) rect.bottom = rect.top  + sk_settings.flatscreen_height;
	
	local.window = CreateWindowW(
		app_name_w,
		app_name_w,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		maxi(0,rect.left),
		maxi(0,rect.top),
		rect.right  - rect.left,
		rect.bottom - rect.top,
		0, 0, 
		wc.hInstance,
		nullptr);

	sk_free(app_name_w);

	if( !local.window ) return false;

	RECT bounds;
	GetClientRect(local.window, &bounds);
	int32_t width  = maxi(1, bounds.right  - bounds.left);
	int32_t height = maxi(1, bounds.bottom - bounds.top);

	skg_tex_fmt_ color_fmt = skg_tex_fmt_rgba32_linear;
	local.depth_format = render_preferred_depth_fmt();
#if defined(SKG_OPENGL)
	depth_fmt = skg_tex_fmt_depthstencil;
#endif
	local.swapchain = skg_swapchain_create(local.window, color_fmt, skg_tex_fmt_none, width, height);
	local.swapchain_initialized = true;
	sk_info.display_width  = local.swapchain.width;
	sk_info.display_height = local.swapchain.height;
	device_data.display_width  = local.swapchain.width;
	device_data.display_height = local.swapchain.height;

	log_diagf("Created swapchain: %dx%d color:%s depth:%s", local.swapchain.width, local.swapchain.height, render_fmt_name((tex_format_)color_fmt), render_fmt_name(local.depth_format));
	render_update_projection();

	return true;
}

///////////////////////////////////////////

void win32_stop_flat() { 
	if (backend_xr_get_type() == backend_xr_type_simulator) {
		RECT rect;
		HKEY reg_key;
		GetWindowRect(local.window, &rect);
		if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY_NAME, 0, nullptr, 0, KEY_WRITE, nullptr, &reg_key, nullptr) == ERROR_SUCCESS) {
			RegSetValueExW(reg_key, REG_VALUE_NAME, 0, REG_BINARY, (const BYTE*)&rect, sizeof(RECT));
			RegCloseKey   (reg_key);
		}
	}

	skg_swapchain_destroy(&local.swapchain);
	local.swapchain_initialized = false;


	if (local.icon)   DestroyIcon  (local.icon);
	if (local.window) DestroyWindow(local.window);
	wchar_t* app_name_w = platform_to_wchar(sk_app_name);
	UnregisterClassW(app_name_w, local.hinst);
	sk_free(app_name_w);

	local.icon   = nullptr;
	local.window = nullptr;
	local.hinst  = nullptr;
}

///////////////////////////////////////////

void win32_step_begin_xr() {
	MSG msg = {0};
	while (PeekMessage(&msg, local.window, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage (&msg);
	}
}

///////////////////////////////////////////

void win32_step_begin_flat() {
	MSG msg = {0};
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage (&msg);
	}
}

///////////////////////////////////////////

void win32_step_end_flat() {
	matrix view = matrix_invert(render_get_cam_final());
	matrix proj = render_get_projection_matrix();

	render_surface_set_view("primary",
		local.swapchain.width, local.swapchain.height, render_get_multisample(),
		tex_format_rgba32, local.depth_format,
		&view, &proj, 1);
	render_surfaces();
	tex_t image = render_surface_get_tex("primary");
	if (image) {
		skg_tex_copy_to_swapchain(&image->tex, &local.swapchain);
		tex_release(image);
	}

	local.render_sys->profile_frame_duration = stm_since(local.render_sys->profile_frame_start);
	skg_swapchain_present(&local.swapchain);
}

///////////////////////////////////////////

void *win32_hwnd() {
	return local.window;
}

///////////////////////////////////////////

float win32_get_scroll() {
	return local.scroll;
}

} // namespace sk

#endif // defined(SK_OS_WINDOWS)