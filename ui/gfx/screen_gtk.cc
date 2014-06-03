// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"

namespace {

bool GetScreenWorkArea(gfx::Rect* out_rect) {
  gboolean ok;
  guchar* raw_data = NULL;
  gint data_len = 0;
  ok = gdk_property_get(gdk_get_default_root_window(),  // a gdk window
                        gdk_atom_intern("_NET_WORKAREA", FALSE),  // property
                        gdk_atom_intern("CARDINAL", FALSE),  // property type
                        0,  // byte offset into property
                        0xff,  // property length to retrieve
                        false,  // delete property after retrieval?
                        NULL,  // returned property type
                        NULL,  // returned data format
                        &data_len,  // returned data len
                        &raw_data);  // returned data
  if (!ok)
    return false;

  // We expect to get four longs back: x, y, width, height.
  if (data_len < static_cast<gint>(4 * sizeof(glong))) {
    NOTREACHED();
    g_free(raw_data);
    return false;
  }

  glong* data = reinterpret_cast<glong*>(raw_data);
  gint x = data[0];
  gint y = data[1];
  gint width = data[2];
  gint height = data[3];
  g_free(raw_data);

  out_rect->SetRect(x, y, width, height);
  return true;
}

gfx::Display GetDisplayForMonitorNum(GdkScreen* screen, gint monitor_num) {
  GdkRectangle bounds;
  gdk_screen_get_monitor_geometry(screen, monitor_num, &bounds);
  // Use |monitor_num| as display id.
  gfx::Display display(monitor_num, gfx::Rect(bounds));
  if (gdk_screen_get_primary_monitor(screen) == monitor_num) {
    gfx::Rect rect;
    if (GetScreenWorkArea(&rect))
      display.set_work_area(gfx::IntersectRects(rect, display.bounds()));
  }
  return display;
}

gfx::Display GetMonitorAreaNearestWindow(gfx::NativeView view) {
  GdkScreen* screen = gdk_screen_get_default();
  gint monitor_num = 0;
  if (view && GTK_IS_WINDOW(view)) {
    GtkWidget* top_level = gtk_widget_get_toplevel(view);
    DCHECK(GTK_IS_WINDOW(top_level));
    GtkWindow* window = GTK_WINDOW(top_level);
    screen = gtk_window_get_screen(window);
    monitor_num = gdk_screen_get_monitor_at_window(
        screen,
        gtk_widget_get_window(top_level));
  }
  return GetDisplayForMonitorNum(screen, monitor_num);
}

class ScreenGtk : public gfx::Screen {
  ObserverList<gfx::DisplayObserver> observer_list_;
  std::vector<gfx::Display> last_known_display_;

  static const gfx::Display* FindDisplay(const std::vector<gfx::Display>& list, int64 id) {
    for (std::vector<gfx::Display>::const_iterator i = list.begin(); i != list.end(); i++) {
      if (i->id() == id)
        return &(*i);
    }
    return NULL;
  }

  static void OnMonitorsChanged (GdkScreen *screen, gpointer user_data) {
    ScreenGtk* pThis = static_cast<ScreenGtk*>(user_data);

    // get current configuration
    const std::vector<gfx::Display>& current_display = pThis->GetAllDisplays();

    // find removed display
    for (std::vector<gfx::Display>::const_iterator i = pThis->last_known_display_.begin(); i != pThis->last_known_display_.end(); i++) {
      if (FindDisplay(current_display, i->id()) == NULL) {
        FOR_EACH_OBSERVER(gfx::DisplayObserver, pThis->observer_list_,
          OnDisplayRemoved(*i));
      }
    }

    // find added display
    for (std::vector<gfx::Display>::const_iterator i = current_display.begin(); i != current_display.end(); i++) {
      if (FindDisplay(pThis->last_known_display_, i->id()) == NULL) {
        FOR_EACH_OBSERVER(gfx::DisplayObserver, pThis->observer_list_,
          OnDisplayAdded(*i));
      }
    }

    // find changed display
    for (std::vector<gfx::Display>::const_iterator i = current_display.begin(); i != current_display.end(); i++) {
      const gfx::Display* matched_display = FindDisplay(pThis->last_known_display_, i->id());
      if (matched_display && memcmp(&(*i), matched_display, sizeof(gfx::Display)) != 0 ) {
        FOR_EACH_OBSERVER(gfx::DisplayObserver, pThis->observer_list_,
          OnDisplayBoundsChanged(*i));
      }
    }

    // update the last known display configuration with current;
    pThis->last_known_display_ = current_display;

  }

 public:
  ScreenGtk() {
	last_known_display_ = GetAllDisplays();
    GdkScreen *screen = gdk_screen_get_default ();
    g_signal_connect(screen, "monitors-changed", G_CALLBACK(OnMonitorsChanged), this);
  }

  virtual ~ScreenGtk() {
    GdkScreen *screen = gdk_screen_get_default ();
    g_signal_handlers_disconnect_by_func(screen, reinterpret_cast<gpointer>(&OnMonitorsChanged), this);
  }

  virtual bool IsDIPEnabled() OVERRIDE {
    return false;
  }

  virtual gfx::Point GetCursorScreenPoint() OVERRIDE {
    gint x, y;
    gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);
    return gfx::Point(x, y);
  }

  // Returns the window under the cursor.
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE {
    GdkWindow* window = gdk_window_at_pointer(NULL, NULL);
    if (!window)
      return NULL;

    gpointer data = NULL;
    gdk_window_get_user_data(window, &data);
    GtkWidget* widget = reinterpret_cast<GtkWidget*>(data);
    if (!widget)
      return NULL;
    widget = gtk_widget_get_toplevel(widget);
    return GTK_IS_WINDOW(widget) ? GTK_WINDOW(widget) : NULL;
  }

  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  // Returns the number of displays.
  // Mirrored displays are excluded; this method is intended to return the
  // number of distinct, usable displays.
  virtual int GetNumDisplays() const OVERRIDE {
    // This query is kinda bogus for Linux -- do we want number of X screens?
    // The number of monitors Xinerama has?  We'll just use whatever GDK uses.
    GdkScreen* screen = gdk_screen_get_default();
    return gdk_screen_get_n_monitors(screen);
  }

  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE {
    GdkScreen* screen = gdk_screen_get_default();
    gint num_of_displays = gdk_screen_get_n_monitors(screen);
    std::vector<gfx::Display> all_displays;
    for (gint i = 0; i < num_of_displays; ++i)
      all_displays.push_back(GetDisplayForMonitorNum(screen, i));
    return all_displays;
  }

  // Returns the display nearest the specified window.
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE {
    // Do not use the _NET_WORKAREA here, this is supposed to be an area on a
    // specific monitor, and _NET_WORKAREA is a hint from the WM that
    // generally spans across all monitors.  This would make the work area
    // larger than the monitor.
    // TODO(danakj) This is a work-around as there is no standard way to get
    // this area, but it is a rect that we should be computing.  The standard
    // means to compute this rect would be to watch all windows with
    // _NET_WM_STRUT(_PARTIAL) hints, and subtract their space from the
    // physical area of the display to construct a work area.
    // TODO(oshima): Implement Observer.
    return GetMonitorAreaNearestWindow(view);
  }

  // Returns the the display nearest the specified point.
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE {
    GdkScreen* screen = gdk_screen_get_default();
    gint monitor = gdk_screen_get_monitor_at_point(
        screen, point.x(), point.y());
    // TODO(oshima): Implement Observer.
    return GetDisplayForMonitorNum(screen, monitor);
  }

  // Returns the display that most closely intersects the provided bounds.
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE {
    std::vector<gfx::Display> displays = GetAllDisplays();
    gfx::Display maxIntersectDisplay;
    gfx::Rect maxIntersection;
    for (std::vector<gfx::Display>::iterator it = displays.begin();
         it != displays.end(); ++it) {
      gfx::Rect displayIntersection = it->bounds();
      displayIntersection.Intersect(match_rect);
      if (displayIntersection.size().GetArea() >
          maxIntersection.size().GetArea()) {
        maxIntersectDisplay = *it;
        maxIntersection = displayIntersection;
      }
    }
    return maxIntersectDisplay.is_valid() ?
        maxIntersectDisplay : GetPrimaryDisplay();
  }

  // Returns the primary display.
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE {
    GdkScreen* screen = gdk_screen_get_default();
    gint primary_monitor_index = gdk_screen_get_primary_monitor(screen);
    // TODO(oshima): Implement Observer.
    return GetDisplayForMonitorNum(screen, primary_monitor_index);
  }

  virtual void AddObserver(gfx::DisplayObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }

  virtual void RemoveObserver(gfx::DisplayObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenGtk);
};

}  // namespace

namespace gfx {

Screen* CreateNativeScreen() {
  return new ScreenGtk;
}

}  // namespace gfx
