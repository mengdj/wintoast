#include "stdafx.h"
#include "Toast.h"
std::shared_ptr<xmstudio::toast> xmstudio::toast::_this_ = nullptr;
concurrency::critical_section xmstudio::toast::cs;

xmstudio::toast::toast() :m_nc_create(false), font(nullptr), mem_dc(nullptr), brush(nullptr), m_create(false), m_reg(0) {
	m_msg = nullptr;
	m_mem_bitmap = nullptr;
}

xmstudio::toast::~toast() {
	if (nullptr != mem_dc) {
		::DeleteDC(mem_dc);
		mem_dc = nullptr;
		if (nullptr != m_mem_bitmap) {
			::DeleteObject(m_mem_bitmap);
			m_mem_bitmap = nullptr;
		}
	}
	if (nullptr != font) {
		::DeleteObject(font);
		font = nullptr;
	}
	if (nullptr != brush) {
		::DeleteObject(brush);
		brush = nullptr;
	}
}

//dispatch msg
LRESULT CALLBACK xmstudio::toast::dispatch(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static PAINTSTRUCT ps;
	static int msg_height = 0;
	static HDC hdc;
	static SIZE msg_size = { 0 };
	static BITMAP compare_bitmap = { 0 };
	switch (uMsg) {
	case WM_PAINT:
		hdc = ::BeginPaint(hwnd, &ps);
		::SetBkMode(hdc, TRANSPARENT);
		if (nullptr != m_msg) {
			if (compare_bitmap.bmWidth != m_msg->cx || compare_bitmap.bmHeight != m_msg->cy) {
				//切换兼容位图，当环境发生变化时width和height
				m_mem_bitmap = ::CreateCompatibleBitmap(hdc, m_msg->cx, m_msg->cy);
				if (nullptr != m_mem_bitmap) {
					::DeleteObject(::SelectObject(mem_dc, m_mem_bitmap));
					::GetObject(m_mem_bitmap, sizeof(BITMAP), &compare_bitmap);
				}
			}
			if (nullptr != brush) {
				RECT c = { 0,0,m_msg->cx, m_msg->cy };
				::FillRect(mem_dc, &c, brush);
			}
			auto msg_re_size = m_msg->multi_msg.size();
			for (auto& s : m_msg->multi_msg) {
				if (msg_re_size > 1) {
					::TextOut(mem_dc, (m_msg->cx >> 1) - s.x, (m_msg->cy >> 1) - ((msg_re_size*s.size.cy) >> 1) + s.y, s.str.c_str(), s.str.size());
				}
				else {
					::TextOut(mem_dc, (m_msg->cx >> 1) - s.x, s.y, s.str.c_str(), s.str.size());
				}
			}
			::BitBlt(hdc, 0, 0, m_msg->cx, m_msg->cy, mem_dc, 0, 0, SRCCOPY);
		}
		else {
			if (nullptr == mem_dc) {
				mem_dc = ::CreateCompatibleDC(hdc);
				::SetBkMode(mem_dc, TRANSPARENT);
				::SetTextColor(mem_dc, cfg.font.color);
				::SelectObject(mem_dc, font);
			}
		}
		::EndPaint(hwnd, &ps);
		break;
	case WM_ERASEBKGND:
		return TRUE;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return default_proc(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

//默认的消息处理
LRESULT CALLBACK  xmstudio::toast::default_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (m_old_proc != xmstudio::toast::proc)
		return ::CallWindowProc(m_old_proc, hWnd, uMsg, wParam, lParam);
	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//提示窗口信息
bool xmstudio::toast::notify(HWND owner_hwnd, const wchar_t * msg, int dur, Align align, int offset_x, int offset_y) {
	static RECT tmp_ref_rect = { 0 };
	static std::atomic<int> notify_count = 0;
	auto tmp_share_ptr = std::make_shared<TOAST_MSG>();
	tmp_share_ptr->id = notify_count = (++notify_count) % 255;
	tmp_share_ptr->done = 0;
	tmp_share_ptr->offset_x = offset_x;
	tmp_share_ptr->offset_y = offset_y;
	tmp_share_ptr->dur = ms_timestamp() + dur;
	tmp_share_ptr->msg = msg;
	tmp_share_ptr->owner_hwnd = owner_hwnd;
	tmp_share_ptr->align = align;
	return concurrency::asend(m_msg_queue, std::move(tmp_share_ptr));
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
				::SetWindowLong(hWnd, GWL_USERDATA, (LONG)p_tmp_toast);
				return p_tmp_toast->dispatch(uMsg, wParam, lParam);
			}
		}
	}
	else {
		auto p_tmp_toast = reinterpret_cast<xmstudio::toast*>(::GetWindowLong(hWnd, GWL_USERDATA));
		if (nullptr != p_tmp_toast) {
			return p_tmp_toast->dispatch(uMsg, wParam, lParam);
		}
	}
	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/**
	运行消息计算(计算并负责刷新窗口)
*/
void xmstudio::toast::run() {
	std::wstringstream wss;
	std::wstring item;
	RECT ref_rect = { 0 };
	TOAST_RADIUS toast_radius = { 0 };
	while (1) {
		if (nullptr != mem_dc) {
			m_msg.swap(concurrency::receive(m_msg_queue));
			if (m_msg->done) {
				m_msg.reset();
				break;
			}
			wss.clear();
			m_msg->multi_msg.clear();
			wss << m_msg->msg;
			int msg_count = 0, msg_max_width = 0, msg_total_height = 0;
			while (std::getline(wss, item)) {
				if (!item.empty()) {
					TOAST_SHOW tmp_show = { 0 };
					if (TRUE == ::GetTextExtentPoint32(mem_dc, item.c_str(), item.size(), &tmp_show.size)) {
						tmp_show.str = item;
						tmp_show.size.cy += cfg.spacing;
						tmp_show.x = tmp_show.size.cx >> 1;
						msg_max_width = tmp_show.size.cx > msg_max_width ? tmp_show.size.cx : msg_max_width;
						tmp_show.y = msg_count*tmp_show.size.cy;
						tmp_show.i = msg_count;
						if (msg_count >= 1) {
							tmp_show.y += cfg.spacing;
							msg_total_height += cfg.spacing;
						}
						msg_total_height += tmp_show.size.cy;
						m_msg->multi_msg.push_back(std::move(tmp_show));
						++msg_count;
					}
				}
			}
			if (msg_count) {
				m_msg->cx = msg_max_width > cfg.width ? msg_max_width : cfg.width;
				m_msg->cy = msg_total_height > cfg.height ? msg_total_height : cfg.height;
				if (m_msg->cx != cfg.width)
					m_msg->cx += cfg.padding;
				if (m_msg->cy != cfg.height)
					m_msg->cy += cfg.padding;
				if (msg_count == 1)
					m_msg->multi_msg[0].y = (m_msg->cy >> 1) - (m_msg->multi_msg[0].size.cy >> 1);
				auto is_window = ::IsWindow(m_msg->owner_hwnd);
				if (is_window ? (::GetWindowRect(m_msg->owner_hwnd, &ref_rect)) : (::GetWindowRect(::GetDesktopWindow(), &ref_rect))) {
					m_msg->x += (ref_rect.right >> 1) - (m_msg->cx >> 1) + m_msg->offset_x;
					m_msg->y += (ref_rect.bottom >> 1) - (m_msg->cy >> 1) + m_msg->offset_y;
					if (is_window) {
						POINT tmp_point = { 0 };
						if (::ClientToScreen(m_msg->owner_hwnd, &tmp_point)) {
							m_msg->x += tmp_point.x >> 1;
							m_msg->y += tmp_point.y >> 1;
						}
					}
					//HWND_TOPMOST
					::SetWindowPos(hwnd, HWND_TOPMOST, m_msg->x, m_msg->y, m_msg->cx, m_msg->cy, SWP_SHOWWINDOW);
					//radius
					if (cfg.radius.width || cfg.radius.height) {
						if (m_msg->cx != toast_radius.x2 || m_msg->cy != toast_radius.y2) {
							//build
							::SetWindowRgn(hwnd, nullptr, FALSE);
							//In particular, do not delete this region handle. The system deletes the region handle when it no longer needed.
							auto tmp_rgn = ::CreateRoundRectRgn(0, 0, m_msg->cx, m_msg->cy, cfg.radius.width, cfg.radius.height);
							if (nullptr != tmp_rgn) {
								if (::SetWindowRgn(hwnd, tmp_rgn, TRUE)) {
									toast_radius.x1 = m_msg->x;
									toast_radius.y1 = m_msg->y;
									toast_radius.x2 = m_msg->x + m_msg->cx;
									toast_radius.y2 = m_msg->y + m_msg->cy;
									toast_radius.w = cfg.radius.width;
									toast_radius.h = cfg.radius.height;
								}
							}
						}
					}
					if (!visible()) {
						::InvalidateRect(hwnd, nullptr, FALSE);
						::ShowWindow(hwnd, SW_SHOWNORMAL);
					}
				}
			}
			msg_total_height = msg_max_width = 0;
		}
	}
}

BOOL xmstudio::toast::visible() {
	return ::GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE;
}

//注册窗口
int xmstudio::toast::loop() {
	//register
	WNDCLASSEXW wcex;
	ZeroMemory(&wcex, sizeof(WNDCLASSEXW));
	wcex.cbSize = sizeof(WNDCLASSEX);
	m_reg = m_reg ? m_reg : (::GetClassInfoEx(cfg.hinstance, m_p_class_name, &wcex));
	if (!m_reg) {
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = xmstudio::toast::proc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = cfg.hinstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = m_p_class_name;
		wcex.hIconSm = nullptr;
		m_reg = RegisterClassExW(&wcex);
	}
	if (m_reg) {
		if (!m_create) {
			CREATESTRUCT cs;
			::ZeroMemory(&cs, sizeof(CREATESTRUCT));
			cs.lpCreateParams = (LPVOID)this;
			cs.hInstance = cfg.hinstance;
			cs.hMenu = nullptr;
			cs.hwndParent = nullptr;
			cs.cx = cfg.width;
			cs.cy = cfg.height;
			cs.x = CW_USEDEFAULT;
			cs.y = CW_USEDEFAULT;
			cs.style = WS_POPUP;
			cs.lpszName = m_p_class_name;
			cs.lpszClass = m_p_class_name;
			cs.dwExStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;
			hwnd = ::CreateWindowEx(cs.dwExStyle, cs.lpszName, cs.lpszClass, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
			if (nullptr != hwnd) {
				m_create = true;
				//ppl
				concurrency::parallel_invoke([this]() {
					//alpha
					::SetLayeredWindowAttributes(hwnd, cfg.background.translate_color, cfg.background.alpha, cfg.background.translate_flags);
				}, [this]() {
					//font
					if (nullptr != font) {
						::DeleteObject(font);
						font = nullptr;
					}
					font = ::CreateFont(cfg.font.height, cfg.font.width, 0, 0, FW_THIN, 0, 0, 0,
						GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, cfg.font.name);
				}, [this]() {
					//brush
					if (nullptr != brush) {
						::DeleteObject(brush);
						brush = nullptr;
					}
					brush = ::CreateSolidBrush(cfg.background.color);
					if (nullptr != m_mem_bitmap) {
						::DeleteObject(m_mem_bitmap);
						m_mem_bitmap = nullptr;
					}
				});
				auto fn_tmp_proc = (WNDPROC)::GetWindowLong(hwnd, GWL_WNDPROC);
				if (fn_tmp_proc != xmstudio::toast::proc) {
					::SetWindowLong(hwnd, GWL_WNDPROC, (LONG)xmstudio::toast::proc);
					::SetWindowLong(hwnd, GWL_USERDATA, (LONG)this);
				}
				if (!m_nc_create) {
					::SendMessage(hwnd, WM_NCCREATE, 0, (LONG)&cs);
					m_nc_create = true;
				}
				::SendMessage(hwnd, WM_CREATE, 0, (LONG)&cs);
				//timer
				concurrency::call<int> tmp_call([this](int v) {
					if (nullptr != m_msg) {
						if (m_msg->dur != 0 && ms_timestamp() >= m_msg->dur && (::GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE)) {
							if (!::AnimateWindow(hwnd, 200, AW_HIDE | AW_ACTIVATE | AW_BLEND)) {
								::ShowWindow(hwnd, SW_HIDE);
							}
						}
					}
				});
				concurrency::timer<int> tmp_timer(100, 0, &tmp_call, true);
				tmp_timer.start();
				concurrency::CurrentScheduler::ScheduleTask([](void* data)->void {
					_this_->run();
				}, nullptr);
				MSG msg;
				while (::GetMessage(&msg, nullptr, 0, 0)) {
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
				return (int)msg.wParam;
			}
			else {
#ifdef LOGD
				LOGD << GetLastError();
#endif // LOGD
			}
		}
	}
	return 0;
}

bool xmstudio::toast::release() {
	m_create = m_nc_create = false;
	if (::IsWindow(hwnd)) {
		//exist message loop
		::ShowWindow(hwnd, SW_HIDE);
		::SendMessage(hwnd, WM_DESTROY, NULL, NULL);
	}
	return !m_create;
}

//init ent
void xmstudio::toast::init(const TOAST_CFG& cfg) {
	//lock
	if (cs.try_lock()) {
		if (nullptr == _this_) {
			//clone
			_this_ = std::shared_ptr<xmstudio::toast>(new xmstudio::toast());
		}
		else {
			if (cfg.width != _this_->cfg.width || cfg.height != _this_->cfg.height) {
				//update
				if (nullptr != _this_->mem_dc) {
					::DeleteDC(_this_->mem_dc);
					_this_->mem_dc = nullptr;
				}
			}
		}
		cs.unlock();
		_this_->cfg = cfg;
		if (!_this_->cfg.padding)
			_this_->cfg.padding = 20;
		if (!_this_->cfg.spacing)
			_this_->cfg.spacing = 5;
		if (!_this_->cfg.radius.width)
			_this_->cfg.radius.width = 5;
		if (!_this_->cfg.radius.height)
			_this_->cfg.radius.height = 5;
		concurrency::CurrentScheduler::ScheduleTask([](void* data)->void {
			_this_->loop();
		}, nullptr);
	}
}

/**
* show notify
*/
bool xmstudio::toast::show(HWND owner_hwnd, const wchar_t * msg, int dur, Align align, int offset_x, int offset_y) {
	std::weak_ptr<xmstudio::toast> tmp_weak_toast(_this_);
	if (!tmp_weak_toast.expired()) {
		return tmp_weak_toast.lock()->notify(owner_hwnd, msg, dur, align, offset_x, offset_y);
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