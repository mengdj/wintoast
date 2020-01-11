#pragma once
#include <ppl.h>
#include <agents.h>
#include <memory>
#include <cassert>
#include <chrono>
#include <atomic>

namespace xmstudio {
	typedef struct _TOAST_MSG_ {
		int id;
		bool done;
		int x;
		int y;
		int cx;
		int cy;
		long long dur;
		std::wstring msg;
	} TOAST_MSG;

	typedef struct _TOAST_CFG_ {
		HINSTANCE hinstance;
		int width;
		int height;
		struct {
			int width;
			int height;
			COLORREF color;
			wchar_t* name;
		} font;
		struct {
			BYTE alpha;
			COLORREF color;
			COLORREF translate_color;
			DWORD translate_flags;
		} background;
	} TOAST_CFG;

	class toast :public concurrency::agent {
	public:
		~toast();
		int loop();
		void run();
		long long ms_timestamp();
		LRESULT CALLBACK dispatch(UINT, WPARAM, LPARAM);
		LRESULT CALLBACK default_proc(HWND, UINT, WPARAM, LPARAM);
		bool release();
		bool notify(HWND owner_hwnd, const wchar_t * msg, int dur = 3000, int offset_x = 0, int offset_y = 0);
		static void init(const TOAST_CFG& cfg);
		static bool show(HWND owner_hwnd, const wchar_t * msg, int dur = 3000, int offset_x = 0, int offset_y = 0);
		static bool destory();
	public:
		HINSTANCE hinstance;
		HWND hwnd;
		concurrency::unbounded_buffer<std::shared_ptr<TOAST_MSG>> m_msg_queue;
		TOAST_CFG cfg;
	protected:
		std::atomic<long long> m_msg_ms;
		WNDPROC m_old_proc;
		std::wstring m_msg_body;
		const wchar_t *m_p_class_name = TEXT("xmstudio.toast");
		bool m_nc_created;
		HFONT font;
		HBRUSH brush;
		HDC mem_dc;
		HBITMAP mem_com_bitmap;
	private:
		toast();
		static concurrency::critical_section cs;
		static std::shared_ptr<toast> _this_;
		static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
	};
};