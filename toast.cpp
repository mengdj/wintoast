#include "stdafx.h"
#include "Toast.h"
//#include <plog/Log.h>
std::shared_ptr<xmstudio::toast> xmstudio::toast::_this_ = nullptr;
concurrency::critical_section xmstudio::toast::cs;

xmstudio::toast::toast() :m_nc_create(false), m_font(nullptr), m_mem_dc(nullptr), m_brush(nullptr), m_create(false), m_reg(0) {
	m_msg = nullptr;
	m_mem_bitmap = nullptr;
	//plog::init(plog::Severity::debug, "debug.txt");
}

xmstudio::toast::~toast() {
	if (nullptr != m_mem_dc) {
		::DeleteDC(m_mem_dc);
		m_mem_dc = nullptr;
		if (nullptr != m_mem_bitmap) {
			::DeleteObject(m_mem_bitmap);
			m_mem_bitmap = nullptr;
		}
	}
	if (nullptr != m_font) {
		::DeleteObject(m_font);
		m_font = nullptr;
	}
	if (nullptr != m_brush) {
		::DeleteObject(m_brush);
		m_brush = nullptr;
	}
}

//dispatch msg
LRESULT CALLBACK xmstudio::toast::dispatch(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static PAINTSTRUCT ps;
	static int msg_height = 0;
	static HDC hdc;
	static SIZE msg_size = { 0 };
	static BITMAP compare_bitmap = { 0 };
	static TRACKMOUSEEVENT track_mouse_event = { 0 };
	static BOOL b_track_mouse = FALSE;
	switch (uMsg) {
	case WM_PAINT:
		hdc = ::BeginPaint(m_hwnd, &ps);
		::SetBkMode(hdc, TRANSPARENT);
		if (nullptr != m_msg) {
			if (compare_bitmap.bmWidth != m_msg->cx || compare_bitmap.bmHeight != m_msg->cy) {
				//切换兼容位图，当环境发生变化时width和height
				m_mem_bitmap = ::CreateCompatibleBitmap(hdc, m_msg->cx, m_msg->cy);
				if (nullptr != m_mem_bitmap) {
					::DeleteObject(::SelectObject(m_mem_dc, m_mem_bitmap));
					::GetObject(m_mem_bitmap, sizeof(BITMAP), &compare_bitmap);
				}
			}
			if (nullptr != m_brush) {
				RECT c = { 0,0,m_msg->cx, m_msg->cy };
				::FillRect(m_mem_dc, &c, m_brush);
			}
			auto msg_re_size = m_msg->multi_msg.size();
			//calc align
			int d_x = 0, d_y = 0;
			for (auto& s : m_msg->multi_msg) {
				if (msg_re_size > 1) {
					if (Align::left == m_msg->align) {
						//left
						d_x = m_cfg.padding;
						d_y = s.y + m_cfg.spacing + m_cfg.padding;
					}
					else if (Align::center == m_msg->align) {
						//middle
						d_x = (m_msg->cx >> 1) - s.x;
						d_y = (m_msg->cy >> 1) - ((msg_re_size*s.size.cy) >> 1) + s.y;
					}
					else if (Align::right == m_msg->align) {
						//right
						d_x = m_msg->cx - s.size.cx - m_cfg.padding;
						d_y = s.y + m_cfg.spacing + m_cfg.padding;
					}
				}
				else {
					if (Align::left == m_msg->align) {
						d_x = m_cfg.padding;
					}
					else if (Align::center == m_msg->align) {
						d_x = (m_msg->cx >> 1) - s.x;
					}
					else if (Align::right == m_msg->align) {
						d_x = m_msg->cx - s.size.cx - m_cfg.padding;
					}
					d_y = s.y;
				}
				::TextOut(m_mem_dc, d_x, d_y, s.str.c_str(), s.str.size());
				d_x = d_y = 0;
			}
			::BitBlt(hdc, 0, 0, m_msg->cx, m_msg->cy, m_mem_dc, 0, 0, SRCCOPY);
		}
		else {
			if (nullptr == m_mem_dc) {
				m_mem_dc = ::CreateCompatibleDC(hdc);
				::SetBkMode(m_mem_dc, TRANSPARENT);
				::SetTextColor(m_mem_dc, m_cfg.font.color);
				::DeleteObject(::SelectObject(m_mem_dc, m_font));
			}
		}
		::EndPaint(m_hwnd, &ps);
		break;
	case WM_MOUSEMOVE:
		if (!b_track_mouse) {
			track_mouse_event.cbSize = sizeof(TRACKMOUSEEVENT);
			track_mouse_event.dwFlags = TME_LEAVE | TME_HOVER;
			track_mouse_event.hwndTrack = m_hwnd;
			track_mouse_event.dwHoverTime = 10;
			b_track_mouse = ::TrackMouseEvent(&track_mouse_event);
		}
		break;
	case WM_MOUSEHOVER:
		m_msg->hover = true;
		break;
	case WM_MOUSELEAVE:
		m_msg->hover = false;
		b_track_mouse = FALSE;
		break;
	case WM_ERASEBKGND:
		return TRUE;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return default_proc(m_hwnd, uMsg, wParam, lParam);
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
bool xmstudio::toast::notify(HWND owner_m_hwnd, const wchar_t * msg, int dur, Align align, int offset_x, int offset_y) {
	static RECT tmp_ref_rect = { 0 };
	static std::atomic<int> notify_count = 0;
	auto tmp_share_ptr = std::make_shared<TOAST_MSG>();
	tmp_share_ptr->id = notify_count = (++notify_count) % 255;
	tmp_share_ptr->done = 0;
	tmp_share_ptr->offset_x = offset_x;
	tmp_share_ptr->offset_y = offset_y;
	tmp_share_ptr->dur = ms_timestamp() + dur;
	tmp_share_ptr->msg = msg;
	tmp_share_ptr->owner_hwnd = owner_m_hwnd;
	tmp_share_ptr->align = align;
	tmp_share_ptr->hover = false;
	return concurrency::asend(m_msg_queue, std::move(tmp_share_ptr));
}

//静态方法窗口回调
LRESULT CALLBACK  xmstudio::toast::proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (WM_NCCREATE == uMsg) {
		auto p_tmp_cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		if (nullptr != p_tmp_cs) {
			auto p_tmp_toast = reinterpret_cast<xmstudio::toast*>(p_tmp_cs->lpCreateParams);
			if (nullptr != p_tmp_toast) {
				p_tmp_toast->m_hwnd = hWnd;
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
		if (nullptr != m_mem_dc) {
			m_msg.swap(concurrency::receive(m_msg_queue));
			if (m_msg->done) {
				m_msg.reset();
				break;
			}
			wss.clear();
			m_msg->multi_msg.clear();
			wss << m_msg->msg;
			int msg_count = 0, msg_max_width = 0, msg_total_height = 0;
			//read \n
			while (std::getline(wss, item)) {
				if (!item.empty()) {
					TOAST_SHOW tmp_show = { 0 };
					if (TRUE == ::GetTextExtentPoint32(m_mem_dc, item.c_str(), item.size(), &tmp_show.size)) {
						tmp_show.str = item;
						tmp_show.size.cy += m_cfg.spacing;
						tmp_show.x = tmp_show.size.cx >> 1;
						msg_max_width = tmp_show.size.cx > msg_max_width ? tmp_show.size.cx : msg_max_width;
						tmp_show.y = msg_count*tmp_show.size.cy;
						tmp_show.i = msg_count;
						if (msg_count > 1) {
							tmp_show.y += m_cfg.spacing;
							msg_total_height += m_cfg.spacing;
						}
						msg_total_height += tmp_show.size.cy;
						m_msg->multi_msg.push_back(std::move(tmp_show));
						++msg_count;
					}
				}
			}
			if (msg_count) {
				m_msg->cx = msg_max_width + (m_cfg.padding << 2) > m_cfg.width ? msg_max_width + (m_cfg.padding << 1) : m_cfg.width;
				m_msg->cy = msg_total_height > m_cfg.height ? msg_total_height : m_cfg.height;
				//v-align
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
					::SetWindowPos(m_hwnd, HWND_TOPMOST, m_msg->x, m_msg->y, m_msg->cx, m_msg->cy, SWP_SHOWWINDOW);
					//radius
					if (m_cfg.radius.width || m_cfg.radius.height) {
						if (m_msg->cx != toast_radius.x2 || m_msg->cy != toast_radius.y2) {
							//build
							::SetWindowRgn(m_hwnd, nullptr, TRUE);
							//In particular, do not delete this region handle. The system deletes the region handle when it no longer needed.
							auto tmp_rgn = ::CreateRoundRectRgn(0, 0, m_msg->cx, m_msg->cy, m_cfg.radius.width, m_cfg.radius.height);
							if (nullptr != tmp_rgn) {
								if (::SetWindowRgn(m_hwnd, tmp_rgn, TRUE)) {
									toast_radius.x1 = m_msg->x;
									toast_radius.y1 = m_msg->y;
									toast_radius.x2 = m_msg->x + m_msg->cx;
									toast_radius.y2 = m_msg->y + m_msg->cy;
									toast_radius.w = m_cfg.radius.width;
									toast_radius.h = m_cfg.radius.height;
								}
							}
						}
					}
					if (!visible()) {
						//::InvalidateRect(m_hwnd, nullptr, FALSE);
						::ShowWindow(m_hwnd, SW_SHOWNORMAL);
					}
				}
			}
			msg_total_height = msg_max_width = 0;
		}
	}
}

BOOL xmstudio::toast::visible() {
	return ::GetWindowLong(m_hwnd, GWL_STYLE) & WS_VISIBLE;
}

//注册窗口
int xmstudio::toast::loop() {
	//register
	WNDCLASSEXW wcex;
	ZeroMemory(&wcex, sizeof(WNDCLASSEXW));
	wcex.cbSize = sizeof(WNDCLASSEX);
	m_reg = m_reg ? m_reg : (::GetClassInfoEx(m_cfg.hinstance, m_p_class_name, &wcex));
	if (!m_reg) {
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = xmstudio::toast::proc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = m_cfg.hinstance;
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
			cs.hInstance = m_cfg.hinstance;
			cs.hMenu = nullptr;
			cs.hwndParent = nullptr;
			cs.cx = m_cfg.width;
			cs.cy = m_cfg.height;
			cs.x = CW_USEDEFAULT;
			cs.y = CW_USEDEFAULT;
			cs.style = WS_POPUP;
			cs.lpszName = m_p_class_name;
			cs.lpszClass = m_p_class_name;
			//WS_EX_TRANSPARENT
			cs.dwExStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
			m_hwnd = ::CreateWindowEx(cs.dwExStyle, cs.lpszName, cs.lpszClass, cs.style, cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
			if (nullptr != m_hwnd) {
				m_create = true;
				//ppl
				concurrency::parallel_invoke([this]() {
					//alpha
					::SetLayeredWindowAttributes(m_hwnd, m_cfg.background.translate_color, m_cfg.background.alpha, m_cfg.background.translate_flags);
				}, [this]() {
					//m_font
					if (nullptr != m_font) {
						::DeleteObject(m_font);
						m_font = nullptr;
					}
					m_font = ::CreateFont(m_cfg.font.height, m_cfg.font.width, 0, 0, FW_THIN, 0, 0, 0,
						GB2312_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, m_cfg.font.name);
				}, [this]() {
					//m_brush
					if (nullptr != m_brush) {
						::DeleteObject(m_brush);
						m_brush = nullptr;
					}
					m_brush = ::CreateSolidBrush(m_cfg.background.color);
					if (nullptr != m_mem_bitmap) {
						::DeleteObject(m_mem_bitmap);
						m_mem_bitmap = nullptr;
					}
				});
				auto fn_tmp_proc = (WNDPROC)::GetWindowLong(m_hwnd, GWL_WNDPROC);
				if (fn_tmp_proc != xmstudio::toast::proc) {
					::SetWindowLong(m_hwnd, GWL_WNDPROC, (LONG)xmstudio::toast::proc);
					::SetWindowLong(m_hwnd, GWL_USERDATA, (LONG)this);
				}
				if (!m_nc_create) {
					::SendMessage(m_hwnd, WM_NCCREATE, 0, (LONG)&cs);
					m_nc_create = true;
				}
				::SendMessage(m_hwnd, WM_CREATE, 0, (LONG)&cs);
				//intval task m_cfg.intval
				concurrency::call<int> tmp_call([this](int v) {
					if (nullptr != m_msg) {
						if (!m_msg->hover&&m_msg->dur != 0 && ms_timestamp() >= m_msg->dur && visible()) {
							if (!::AnimateWindow(m_hwnd, 200, AW_HIDE | AW_ACTIVATE | AW_BLEND)) {
								::ShowWindow(m_hwnd, SW_HIDE);
							}
						}
					}
				});
				concurrency::timer<int> tmp_timer(m_cfg.intval, 0, &tmp_call, true);
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
	if (::IsWindow(m_hwnd)) {
		//exist message loop
		::ShowWindow(m_hwnd, SW_HIDE);
		::SendMessage(m_hwnd, WM_DESTROY, NULL, NULL);
	}
	return !m_create;
}

//init ent
void xmstudio::toast::init(const TOAST_CFG& m_cfg) {
	//lock
	if (cs.try_lock()) {
		if (nullptr == _this_) {
			//clone
			_this_ = std::shared_ptr<xmstudio::toast>(new xmstudio::toast());
		}
		else {
			if (m_cfg.width != _this_->m_cfg.width || m_cfg.height != _this_->m_cfg.height) {
				//update
				if (nullptr != _this_->m_mem_dc) {
					::DeleteDC(_this_->m_mem_dc);
					_this_->m_mem_dc = nullptr;
				}
			}
		}
		cs.unlock();
		_this_->m_cfg = m_cfg;
		if (!_this_->m_cfg.padding)
			_this_->m_cfg.padding = 15;
		if (!_this_->m_cfg.spacing)
			_this_->m_cfg.spacing = 5;
		if (!_this_->m_cfg.radius.width)
			_this_->m_cfg.radius.width = 5;
		if (!_this_->m_cfg.radius.height)
			_this_->m_cfg.radius.height = 5;
		if (!_this_->m_cfg.intval)
			_this_->m_cfg.intval = 90;
		concurrency::CurrentScheduler::ScheduleTask([](void* data)->void {
			_this_->loop();
		}, nullptr);
	}
}

/**
* show notify
*/
bool xmstudio::toast::show(HWND owner_m_hwnd, const wchar_t * msg, int dur, Align align, int offset_x, int offset_y) {
	std::weak_ptr<xmstudio::toast> tmp_weak_toast(_this_);
	if (!tmp_weak_toast.expired()) {
		return tmp_weak_toast.lock()->notify(owner_m_hwnd, msg, dur, align, offset_x, offset_y);
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