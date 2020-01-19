#pragma once
#include <ppl.h>
#include <agents.h>
#include <memory>
#include <cassert>
#include <chrono>
#include <atomic>
#include <sstream>
/**
* wintoast
* @author mengdj@outlook.com
*/
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
		bool done;								//是否完成
		int x;									//x
		int y;									//
		int offset_x;							//可调整位置
		int offset_y;							//
		int cx;
		int cy;
		bool hover;								//当前鼠标是否hover
		Align align;							//内容水平对齐方式(左、右、居中)
		long long dur;							//持续时间，以毫秒计算
		std::wstring msg;						//完整的未拆分的消息
		std::vector<TOAST_SHOW> multi_msg;		//多行消息时将存入此队列
		HWND owner_hwnd;						//归属窗口
	} TOAST_MSG;

	typedef struct _TOAST_RADIUS_ {
		int x1;
		int y1;
		int x2;
		int y2;
		int w;
		int h;
	} TOAST_RADIUS;

	typedef struct _TOAST_CFG_ {
		HINSTANCE hinstance;					//实例
		int width;								//宽度，若为0则根据内容宽度自适应+padding	
		int height;								//高度，若为0则根据内容宽度自适应+padding	
		int padding;							//内容边距，仅width或height自动计算时才生效
		int spacing;							//多行内容时的行与行之间的间距
		int intval;								//定时器检查间隔ms
		struct {
			int width;							//圆角宽度，默认5
			int height;							//圆角高度，默认5
		} radius;								//圆角
		struct {
			int width;							//字体宽度
			int height;							//字体高度
			COLORREF color;						//字体颜色
			wchar_t* name;						//字体名称（需操作系统已安装的字体）
		} font;
		struct {
			BYTE alpha;							//透明度
			COLORREF color;						//窗口背景色
			COLORREF translate_color;			//指定透明色
			DWORD translate_flags;				//透明模式 LWA_COLORKEY LWA_ALPHA
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
		static bool hide();
		static bool destory();
	public:
		HWND m_hwnd;
		concurrency::unbounded_buffer<std::shared_ptr<TOAST_MSG>> m_msg_queue;
		TOAST_CFG m_cfg;
	protected:
		WNDPROC m_old_proc;
		std::shared_ptr<TOAST_MSG> m_msg;
		const wchar_t *m_p_class_name = TEXT("xmstudio.toast");
		bool m_nc_create;
		bool m_create;
		HFONT m_font;
		HBRUSH m_brush;
		HBITMAP m_mem_bitmap;
		HDC m_mem_dc;
		int m_reg;
	private:
		toast();
		static concurrency::critical_section cs;
		static std::shared_ptr<toast> _this_;
		static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
	};
};