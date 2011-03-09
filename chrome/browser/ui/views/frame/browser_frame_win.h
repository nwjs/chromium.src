// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "views/window/window_win.h"

class AeroGlassNonClientView;
class BrowserNonClientFrameView;
class BrowserRootView;
class BrowserView;
class NonClientFrameView;
class Profile;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin
//
//  BrowserFrame is a WindowWin subclass that provides the window frame for the
//  Chrome browser window.
//
class BrowserFrameWin : public BrowserFrame, public views::WindowWin {
 public:
  // Normally you will create this class by calling BrowserFrame::Create.
  // Init must be called before using this class, which Create will do for you.
  BrowserFrameWin(BrowserView* browser_view, Profile* profile);
  virtual ~BrowserFrameWin();

  // This initialization function must be called after construction, it is
  // separate to avoid recursive calling of the frame from its constructor.
  void InitBrowserFrame();

  BrowserView* browser_view() const { return browser_view_; }

  // BrowserFrame implementation.
  virtual views::Window* GetWindow();
  virtual int GetMinimizeButtonOffset() const;
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const;
  virtual void UpdateThrobber(bool running);
  virtual ui::ThemeProvider* GetThemeProviderForFrame() const;
  virtual bool AlwaysUseNativeFrame() const;
  virtual views::View* GetFrameView() const;
  virtual void TabStripDisplayModeChanged();

 protected:
  // Overridden from views::WindowWin:
  virtual gfx::Insets GetClientAreaInsets() const OVERRIDE;
  virtual bool GetAccelerator(int cmd_id,
                              ui::Accelerator* accelerator) OVERRIDE;
  virtual void OnEndSession(BOOL ending, UINT logoff) OVERRIDE;
  virtual void OnEnterSizeMove() OVERRIDE;
  virtual void OnExitSizeMove() OVERRIDE;
  virtual void OnInitMenuPopup(HMENU menu,
                               UINT position,
                               BOOL is_system_menu) OVERRIDE;
  virtual LRESULT OnMouseActivate(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) OVERRIDE;
  virtual void OnMove(const CPoint& point) OVERRIDE;
  virtual void OnMoving(UINT param, LPRECT new_bounds) OVERRIDE;
  virtual LRESULT OnNCActivate(BOOL active) OVERRIDE;
  virtual LRESULT OnNCHitTest(const CPoint& pt) OVERRIDE;
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) OVERRIDE;
  virtual ui::ThemeProvider* GetThemeProvider() const OVERRIDE;
  virtual void OnScreenReaderDetected() OVERRIDE;

  // Overridden from views::Window:
  virtual int GetShowState() const;
  virtual void Activate();
  virtual bool IsAppWindow() const { return true; }
  virtual views::NonClientFrameView* CreateFrameViewForWindow();
  virtual void UpdateFrameAfterFrameChange();
  virtual views::RootView* CreateRootView();

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  BrowserNonClientFrameView* browser_frame_view_;

  // An unowning reference to the root view associated with the window. We save
  // a copy as a BrowserRootView to avoid evil casting later, when we need to
  // call functions that only exist on BrowserRootView (versus RootView).
  BrowserRootView* root_view_;

  bool frame_initialized_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
