#include "stdafx.h"
#include "Toast.h"

std::shared_ptr<xmstudio::toast> xmstudio::toast::_this_ = nullptr;
concurrency::critical_section xmstudio::toast::cs;

xmstudio::toast::toast() :m_nc_created(false), m_msg_ms(0), font(nullptr), mem_dc(nullptr), mem_com_bitmap(nullptr), brush(nullptr) {
}

xmstudio::toast::~toast() {
}

//dispatch msg
LRESULT CALLBACK xmstudio::toast::dispatch(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	PAINTSTRUCT ps;
	if (WM_PAINT == uMsg) {
		HDC hdc = ::BeginPaint(hwnd, &ps);
		::SetBkMode(hdc, TRANSPARENT);
		if (nullptr == mem_dc) {
			mem_dc = ::CreateCompatibleDC(hdc);
			mem_com_bitmap = ::CreateCompatibleBitmap(hdc, cfg.width, cfg.height);
			::SelectObject(mem_dc, mem_com_bitmap);
			::SetBkMode(mem_dc, TRANSPARENT);
			::SetTextColor(mem_dc, cfg.font.color);
			::SelectObject(mem_dc, font);
		}
		if (nullptr != brush) {
			RECT c = { 0,0,cfg.width,cfg.height };
			::FillRect(mem_dc, &c, brush);
		}
		//textout
		if (!m_msg_body.empty()) {
			SIZE tmp_msg_body_size = { 0 };
			if (TRUE == GetTextExtentPoint32(mem_dc, m_msg_body.c_str(), m_msg_body.size(), &tmp_msg_body_size)) {
				::TextOut(mem_dc, (cfg.width >> 1) - (tmp_msg_body_size.cx >> 1), (cfg.height >> 1) - (tmp_msg_body_size.cy >> 1), m_msg_body.c_str(), m_msg_body.size());
			}
		}
		::BitBlt(hdc, 0, 0, cfg.width, cfg.height, mem_dc, 0, 0, SRCCOPY);
		::EndPaint(hwnd, &ps);
	}
	return default_proc(hwnd, uMsg, wParam, lParam);
}

//默认的消息处理
LRESULT CALLBACK  xmstudio::toast::default_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (m_old_proc != xmstudio::toast::proc)
		return ::CallWindowProc(m_old_proc, hWnd, uMsg, wParam, lParam);
	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//提示窗口信息
bool xmstudio::toast::notify(HWND owner_hwnd, const wchar_t * msg, int dur, int offset_x, int offset_y) {
	static RECT tmp_ref_rect = { 0 };
	static std::atomic<int> notify_count = 0;
	auto is_window = ::IsWindow(owner_hwnd);
	if (is_window ? (::GetWindowRect(owner_hwnd, &tmp_ref_rect)) : (::GetWindowRect(::GetDesktopWindow(), &tmp_ref_rect))) {
		auto tmp_share_ptr = std::make_shared<TOAST_MSG>();
		tmp_share_ptr->id = notify_count = (++notify_count) % 255;
		tmp_share_ptr->done = 0;
		tmp_share_ptr->x = (tmp_ref_rect.right >> 1) - (cfg.width >> 1) + offset_x;
		tmp_share_ptr->y = (tmp_ref_rect.bottom >> 1) - (cfg.height >> 1) + offset_y;
		tmp_share_ptr->dur = ms_timestamp() + dur;
		tmp_share_ptr->msg = msg;
		if (is_window) {
			POINT tmp_point = { 0 };
			if (::ClientToScreen(owner_hwnd, &tmp_point)) {
				tmp_share_ptr->x += tmp_point.x >> 1;
				tmp_share_ptr->y += tmp_point.y >> 1;
			}
		}
		return concurrency::asend(m_msg_queue, std::move(tmp_share_ptr));
	}
	return false;
}

//静态方法窗口回调
LRESULT CALLBACK  xmstudio::toast::proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (WM_NCCREATE == uMsg) {
		auto p_tmp_cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		if (nullptr != p_tmp_cs) {
			auto p_tmp_toast = reinterpret_cast<xmstudio::toast*>(p_tmp_cs->lpCreateParams);
			if (nullptr != p_tmp_toast) {
				p_tmp_toast->hwnd = hWnd;
				p_tmp_toast->m_old_proc = (WNDPROC)::GetWindowLong(hWnd, GWL_WNDPROC);
				SetWindowLong(hWnd, GWL_USERDATA, (LONG)p_tmp_toast);
				return p_tmp_toast->dispatch(uMsg, wParam, lParam);
			}
		}
	}
	else {
		auto p_tmp_toast = reinterpret_cast<xmstudio::toast*>(GetWindowLong(hWnd, GWL_USERDATA));
		if (nullptr != p_tmp_toast) {
			return p_tmp_toast->dispatch(uMsg, wParam, lParam);
		}
	}
	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void xmstudio::toast::run() {
	while (1) {
		auto tmp_ptr = concurrency::receive(m_msg_queue);
		if (tmp_ptr->done) {
			break;
		}
		m_msg_ms = tmp_ptr->dur;
		m_msg_body = std::move(tmp_ptr->msg);
		auto is_visible = ::GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE;
		::SetWindowPos(hwnd, HWND_TOP, tmp_ptr->x, tmp_ptr->y, cfg.width, cfg.height, SWP_SHOWWINDOW);
		if (is_visible) {
			::InvalidateRect(hwnd, NULL, FALSE);
		}
	}
	done();
}

//注册窗口
int xmstudio::toast::loop() {
	//register
	WNDCLASSEXW wcex;
	ZeroMemory(&wcex, sizeof(WNDCLASSEXW));
	wcex.cbSize = sizeof(WNDCLASSEX);
	BOOL reg_class = GetClassInfoEx(hinstance, m_p_class_name, &wcex);
	if (!reg_class) {
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = xmstudio::toast::proc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hinstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = m_p_class_name;
		wcex.hIconSm = nullptr;
		reg_class = RegisterClassExW(&wcex);
	}
	if (reg_class) {
		CREATESTRUCT cs;
		ZeroMemory(&cs, sizeof(CREATESTRUCT));
		cs.lpCreateParams = (LPVOID)this;
		cs.hInstance = hinstance;
		cs.hMenu = nullptr;
		cs.hwndParent = nullptr;
		cs.cx = cfg.width;
		cs.cy = cfg.height;
		cs.x = CW_USEDEFAULT;
		cs.y = CW_USEDEFAULT;
		cs.style = WS_POPUP;
		cs.lpszName = m_p_class_name;
		cs.lpszClass = m_p_class_name;
		cs.dwExStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
		hwnd = CreateWindowEx(cs.dwExStyle, cs.lpszName, cs.lpszClass, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
		if (nullptr != hwnd) {
			concurrency::parallel_invoke([this]() {
				//alpha
				::SetLayeredWindowAttributes(hwnd, cfg.background.translate_color, cfg.background.alpha, cfg.background.translate_flags);
			}, [this]() {
				//font
				font = CreateFont(cfg.font.height, cfg.font.width, 0, 0, FW_THIN, 0, 0, 0,
					GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, cfg.font.name);
			}, [this]() {
				//brush
				brush = CreateSolidBrush(cfg.background.color);
			});
			auto fn_tmp_proc = (WNDPROC)::GetWindowLong(hwnd, GWL_WNDPROC);
			if (fn_tmp_proc != xmstudio::toast::proc) {
				::SetWindowLong(hwnd, GWL_WNDPROC, (LONG)xmstudio::toast::proc);
				::SetWindowLong(hwnd, GWL_USERDATA, (LONG)this);
				if (!m_nc_created)
					::SendMessage(hwnd, WM_NCCREATE, 0, (LONG)&cs);
				::SendMessage(hwnd, WM_CREATE, 0, (LONG)&cs);
			}
			//timer
			concurrency::call<int> tmp_call([this](int v) {
				if (m_msg_ms != 0 && ms_timestamp() >= m_msg_ms && (::GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE)) {
					::ShowWindow(hwnd, SW_HIDE);
				}
			});
			concurrency::timer<int> tmp_timer(100, 0, &tmp_call, true);
			tmp_timer.start();
			start();
			MSG msg;
			while (GetMessage(&msg, nullptr, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			return (int)msg.wParam;
		}
	}
	return 0;
}

bool xmstudio::toast::release() {
	::DestroyWindow(hwnd);
	if (nullptr != font) {
		::DeleteObject(font);
		font = nullptr;
	}
	if (nullptr != brush) {
		::DeleteObject(brush);
		brush = nullptr;
	}
	return true;
}

//init ent
void xmstudio::toast::init(const TOAST_CFG& cfg) {
	//lock
	if (cs.try_lock()) {
		if (nullptr == _this_) {
			//clone
			_this_ = std::shared_ptr<xmstudio::toast>(new xmstudio::toast());
			_this_->cfg = cfg;
			concurrency::CurrentScheduler::ScheduleTask([](void* data)->void {
				auto tmp_this = *(static_cast<std::shared_ptr<xmstudio::toast>*>(data));
				if (nullptr != tmp_this) {
					tmp_this->loop();
				}
			}, &_this_);
		}
		cs.unlock();
	}
}

/**
* show notify
*/
bool xmstudio::toast::show(HWND owner_hwnd, const wchar_t * msg, int dur, int offset_x, int offset_y) {
	std::weak_ptr<xmstudio::toast> tmp_weak_toast(_this_);
	if (!tmp_weak_toast.expired()) {
		return tmp_weak_toast.lock()->notify(owner_hwnd, msg, dur, offset_x, offset_y);
	}
	return false;
}

//destory resource
bool xmstudio::toast::destory() {
	std::weak_ptr<xmstudio::toast> tmp_weak_toast(_this_);
	if (!tmp_weak_toast.expired()) {
		auto tmp_share_toast = tmp_weak_toast.lock();
		if (nullptr != tmp_share_toast) {
			auto tmp_done_msg = std::make_shared<TOAST_MSG>();
			tmp_done_msg->done = true;
			concurrency::send(tmp_share_toast->m_msg_queue, std::move(tmp_done_msg));
			return tmp_share_toast->release();
		}
	}
	return false;
}

//get ms
long long xmstudio::toast::ms_timestamp() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
