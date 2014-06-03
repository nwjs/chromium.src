// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen_win.h"

#include <windows.h>

#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "base/observer_list.h"
#include "ui/gfx/display.h"
#include "ui/gfx/win/dpi.h"
#include "ui/gfx/display_observer.h"

namespace {

MONITORINFOEX GetMonitorInfoForMonitor(HMONITOR monitor) {
  MONITORINFOEX monitor_info;
  ZeroMemory(&monitor_info, sizeof(MONITORINFOEX));
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

gfx::Display GetDisplay(MONITORINFOEX& monitor_info) {
  // TODO(oshima): Implement Observer.
  int64 id = static_cast<int64>(
      base::Hash(base::WideToUTF8(monitor_info.szDevice)));
  gfx::Rect bounds = gfx::Rect(monitor_info.rcMonitor);
  gfx::Display display(id, bounds);
  display.set_work_area(gfx::Rect(monitor_info.rcWork));
  display.SetScaleAndBounds(gfx::win::GetDeviceScaleFactor(), bounds);
  return display;
}

BOOL CALLBACK EnumMonitorCallback(HMONITOR monitor,
                                  HDC hdc,
                                  LPRECT rect,
                                  LPARAM data) {
  std::vector<gfx::Display>* all_displays =
      reinterpret_cast<std::vector<gfx::Display>*>(data);
  DCHECK(all_displays);

  MONITORINFOEX monitor_info = GetMonitorInfoForMonitor(monitor);
  gfx::Display display = GetDisplay(monitor_info);
  all_displays->push_back(display);
  return TRUE;
}

}  // namespace

namespace gfx {

ScreenWin::ScreenWin() {
  last_known_display_ = GetAllDisplays();
}

ScreenWin::~ScreenWin() {
}

bool ScreenWin::IsDIPEnabled() {
  return IsInHighDPIMode();
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  GetCursorPos(&pt);
  return gfx::Point(pt);
}

gfx::NativeWindow ScreenWin::GetWindowUnderCursor() {
  POINT cursor_loc;
  HWND hwnd = GetCursorPos(&cursor_loc) ? WindowFromPoint(cursor_loc) : NULL;
  return GetNativeWindowFromHWND(hwnd);
}

gfx::NativeWindow ScreenWin::GetWindowAtScreenPoint(const gfx::Point& point) {
  return GetNativeWindowFromHWND(WindowFromPoint(point.ToPOINT()));
}

int ScreenWin::GetNumDisplays() const {
  return GetSystemMetrics(SM_CMONITORS);
}

std::vector<gfx::Display> ScreenWin::GetAllDisplays() const {
  std::vector<gfx::Display> all_displays;
  EnumDisplayMonitors(NULL, NULL, EnumMonitorCallback,
                      reinterpret_cast<LPARAM>(&all_displays));
  return all_displays;
}

gfx::Display ScreenWin::GetDisplayNearestWindow(gfx::NativeView window) const {
  HWND window_hwnd = GetHWNDFromNativeView(window);
  if (!window_hwnd) {
    // When |window| isn't rooted to a display, we should just return the
    // default display so we get some correct display information like the
    // scaling factor.
    return GetPrimaryDisplay();
  }

  MONITORINFOEX monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(window_hwnd, MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return GetDisplay(monitor_info);
}

gfx::Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEX mi;
  ZeroMemory(&mi, sizeof(MONITORINFOEX));
  mi.cbSize = sizeof(mi);
  if (monitor && GetMonitorInfo(monitor, &mi)) {
    return GetDisplay(mi);
  }
  return gfx::Display();
}

gfx::Display ScreenWin::GetDisplayMatching(const gfx::Rect& match_rect) const {
  RECT other_bounds_rect = match_rect.ToRECT();
  MONITORINFOEX monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
      &other_bounds_rect, MONITOR_DEFAULTTONEAREST));
  return GetDisplay(monitor_info);
}

gfx::Display ScreenWin::GetPrimaryDisplay() const {
  MONITORINFOEX mi = GetMonitorInfoForMonitor(
      MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
  gfx::Display display = GetDisplay(mi);
  // TODO(kevers|girard): Test if these checks can be reintroduced for high-DIP
  // once more of the app is DIP-aware.
  if (!(IsInHighDPIMode() || IsHighDPIEnabled())) {
    DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), display.size().width());
    DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), display.size().height());
  }
  return display;
}

const gfx::Display* FindDisplay(const std::vector<gfx::Display>& list, int64 id) {
  for (std::vector<gfx::Display>::const_iterator i = list.begin(); i != list.end(); i++) {
    if (i->id() == id)
      return &(*i);
  }
  return NULL;
}

void ScreenWin::OnDisplayChanged() {
  // get current configuration
  const std::vector<gfx::Display>& current_display = GetAllDisplays();

  // find removed display
  for (std::vector<gfx::Display>::const_iterator i = last_known_display_.begin(); i != last_known_display_.end(); i++) {
    if (FindDisplay(current_display, i->id()) == NULL) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
        OnDisplayRemoved(*i));
    }
  }

  // find added display
  for (std::vector<gfx::Display>::const_iterator i = current_display.begin(); i != current_display.end(); i++) {
    if (FindDisplay(last_known_display_, i->id()) == NULL) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
        OnDisplayAdded(*i));
    }
  }

  // find changed display
  for (std::vector<gfx::Display>::const_iterator i = current_display.begin(); i != current_display.end(); i++) {
    const gfx::Display* matched_display = FindDisplay(last_known_display_, i->id());
    if (matched_display && memcmp(&(*i), matched_display, sizeof(gfx::Display)) != 0 ) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
        OnDisplayBoundsChanged(*i));
    }
  }

  // update the last known display configuration with current;
  last_known_display_ = current_display;
}

void ScreenWin::AddObserver(DisplayObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(DisplayObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

HWND ScreenWin::GetHWNDFromNativeView(NativeView window) const {
#if defined(USE_AURA)
  NOTREACHED();
  return NULL;
#else
  return window;
#endif  // USE_AURA
}

NativeWindow ScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
#if defined(USE_AURA)
  NOTREACHED();
  return NULL;
#else
  return hwnd;
#endif  // USE_AURA
}

#if !defined(USE_AURA)
Screen* CreateNativeScreen() {
  return new ScreenWin;
}
#endif  // !USE_AURA

}  // namespace gfx
