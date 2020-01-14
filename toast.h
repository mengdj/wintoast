#pragma once
#include <ppl.h>
#include <agents.h>
#include <memory>
#include <cassert>
#include <chrono>
#include <atomic>
#include <sstream>

namespace xmstudio {
	enum class Align {
		left,
		center,
		right,
	};
	typedef struct _TOAST_SHOW_ {
		int i;
		int x;
		int y;
		SIZE size;
		std::wstring str;
	} TOAST_SHOW;

	typedef struct _TOAST_MSG_ {
		int id;
		bool done;
		int x;
		int y;
		int offset_x;							//�ɵ���λ��
		int offset_y;							//
		int cx;
		int cy;
		Align align;							//����ˮƽ���뷽ʽ
		long long dur;							//����ʱ�䣬�Ժ������
		std::wstring msg;						//������δ��ֵ���Ϣ
		std::vector<TOAST_SHOW> multi_msg;
		HWND owner_hwnd;						//��������
	} TOAST_MSG;

	typedef struct _TOAST_CFG_ {
		HINSTANCE hinstance;					//ʵ��
		int width;								//��ȣ���Ϊ0��������ݿ������Ӧ+padding	
		int height;								//�߶ȣ���Ϊ0��������ݿ������Ӧ+padding	
		int padding;							//���ݱ߾࣬��width��height�Զ�����ʱ����Ч
		int spacing;							//��������ʱ��������֮��ļ��
		struct {
			int width;
			int height;
			COLORREF color;
			wchar_t* name;						//�������ƣ������ϵͳ�Ѱ�װ�������壩
		} font;
		struct {
			BYTE alpha;							//͸����
			COLORREF color;						//���ڱ���ɫ
			COLORREF translate_color;			//ָ��͸��ɫ
			DWORD translate_flags;				//͸��ģʽ LWA_COLORKEY LWA_ALPHA
		} background;
	} TOAST_CFG;

	class toast {
	public:
		~toast();
		int loop();
		void run();
		long long ms_timestamp();
		LRESULT CALLBACK dispatch(UINT, WPARAM, LPARAM);
		LRESULT CALLBACK default_proc(HWND, UINT, WPARAM, LPARAM);
		bool release();
		bool notify(HWND owner_hwnd, const wchar_t * msg, int dur = 3000, Align align = Align::center, int offset_x = 0, int offset_y = 0);
		BOOL visible();
		static void init(const TOAST_CFG& cfg);
		static bool show(HWND owner_hwnd, const wchar_t * msg, int dur = 3000, Align align = Align::center, int offset_x = 0, int offset_y = 0);
		static bool destory();
	public:
		HWND hwnd;
		concurrency::unbounded_buffer<std::shared_ptr<TOAST_MSG>> m_msg_queue;
		TOAST_CFG cfg;
	protected:
		WNDPROC m_old_proc;
		std::shared_ptr<TOAST_MSG> m_msg;
		const wchar_t *m_p_class_name = TEXT("xmstudio.toast");
		bool m_nc_create;
		bool m_create;
		HFONT font;
		HBRUSH brush;
		HBITMAP m_mem_bitmap;
		HDC mem_dc;
		int m_reg;
	private:
		toast();
		static concurrency::critical_section cs;
		static std::shared_ptr<toast> _this_;
		static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
	};
};