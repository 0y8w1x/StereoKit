#include "win32.h"

#if defined(SK_OS_WINDOWS)

#include "flatscreen_input.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../stereokit.h"
#include "../../_stereokit.h"
#include "../../asset_types/texture.h"
#include "../render.h"
#include "../input.h"
#include "../hand/input_hand.h"

namespace sk {

///////////////////////////////////////////

HWND            win32_window    = nullptr;
skg_swapchain_t win32_swapchain = {};
float           win32_scroll    = 0;

// For managing window resizing
bool win32_check_resize = true;
UINT win32_resize_x     = 0;
UINT win32_resize_y     = 0;

///////////////////////////////////////////

void win32_resize(int width, int height) {
	if (width == sk_info.display_width && height == sk_info.display_height)
		return;
	sk_info.display_width  = width;
	sk_info.display_height = height;
	log_diagf("Resized to: %d<~BLK>x<~clr>%d", width, height);
	
	skg_swapchain_resize(&win32_swapchain, sk_info.display_width, sk_info.display_height);
	render_update_projection();
}

///////////////////////////////////////////

bool win32_init() {
	return true;
}

///////////////////////////////////////////

void win32_shutdown() {
}

///////////////////////////////////////////

bool win32_start() {
	sk_info.display_width  = sk_settings.flatscreen_width;
	sk_info.display_height = sk_settings.flatscreen_height;
	sk_info.display_type   = display_opaque;

	WNDCLASS wc = {0}; 
	wc.lpfnWndProc   = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch(message) {
		case WM_CLOSE:     sk_run     = false; PostQuitMessage(0); break;
		case WM_SETFOCUS:  sk_focused = true;  break;
		case WM_KILLFOCUS: sk_focused = false; break;
		case WM_MOUSEWHEEL:win32_scroll += (short)HIWORD(wParam); break;
		case WM_SYSCOMMAND: {
			// Has the user pressed the restore/'un-maximize' button?
			// WM_SIZE happens -after- this event, and contains the new size.
			if (GET_SC_WPARAM(wParam) == SC_RESTORE)
				win32_check_resize = true;
			
			// Disable alt menu
			if (GET_SC_WPARAM(wParam) == SC_KEYMENU) 
				return (LRESULT)0; 
		} return DefWindowProc(hWnd, message, wParam, lParam); 
		case WM_SIZE: {
			win32_resize_x = (UINT)LOWORD(lParam);
			win32_resize_y = (UINT)HIWORD(lParam);

			// Don't check every time the size changes, this can lead to ugly memory alloc.
			// If a restore event, a maximize, or something else says we should resize, check it!
			if (win32_check_resize || wParam == SIZE_MAXIMIZED) { 
				win32_check_resize = false; 
				win32_resize(win32_resize_x, win32_resize_y); 
			}
		} return DefWindowProc(hWnd, message, wParam, lParam);
		case WM_EXITSIZEMOVE: {
			// If the user was dragging the window around, WM_SIZE is called -before- this 
			// event, so we can go ahead and resize now!
			win32_resize(win32_resize_x, win32_resize_y);
		} return DefWindowProc(hWnd, message, wParam, lParam);
		default: return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return (LRESULT)0;
	};
	wc.hInstance     = GetModuleHandle(NULL);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = sk_app_name;
	if( !RegisterClass(&wc) ) return false;

	RECT r;
	r.left   = sk_settings.flatscreen_pos_x;
	r.right  = sk_settings.flatscreen_pos_x + sk_info.display_width;
	r.top    = sk_settings.flatscreen_pos_y;
	r.bottom = sk_settings.flatscreen_pos_y + sk_info.display_height;
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, false);
	win32_window = CreateWindow(
		wc.lpszClassName, 
		sk_app_name, 
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
		max(0,r.left), 
		max(0,r.top), 
		r.right  - r.left, 
		r.bottom - r.top, 
		0, 0, 
		wc.hInstance, 
		nullptr);
	if( !win32_window ) return false;

	RECT bounds;
	GetClientRect(win32_window, &bounds);
	int32_t width  = bounds.right  - bounds.left;
	int32_t height = bounds.bottom - bounds.top;

	skg_tex_fmt_ color_fmt = skg_tex_fmt_rgba32_linear;
	skg_tex_fmt_ depth_fmt = render_preferred_depth_fmt();
	win32_swapchain = skg_swapchain_create(win32_window, color_fmt, depth_fmt, width, height);
	sk_info.display_width  = win32_swapchain.width;
	sk_info.display_height = win32_swapchain.height;
	log_diagf("Created swapchain: %dx%d color:%s depth:%s", win32_swapchain.width, win32_swapchain.height, render_fmt_name((tex_format_)color_fmt), render_fmt_name((tex_format_)depth_fmt));

	flatscreen_input_init();

	return true;
}

///////////////////////////////////////////

void win32_stop() {
	flatscreen_input_shutdown();
	skg_swapchain_destroy(&win32_swapchain);
}

///////////////////////////////////////////

void win32_step_begin() {
	MSG msg = {0};
	if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage (&msg);
	}
	flatscreen_input_update();
}

///////////////////////////////////////////

void win32_step_end() {
	skg_draw_begin();

	color128 col = render_get_clear_color();
	skg_swapchain_bind(&win32_swapchain, true, &col.r);

	input_update_predicted();

	matrix view = render_get_cam_root  ();
	matrix proj = render_get_projection();
	matrix_inverse(view, view);
	render_draw_matrix(&view, &proj, 1);
	render_clear();
}

///////////////////////////////////////////

void win32_vsync() {
	skg_swapchain_present(&win32_swapchain);
}

///////////////////////////////////////////

void *win32_hwnd() {
	return win32_window;
}

} // namespace sk

#endif // defined(SK_OS_WINDOWS)