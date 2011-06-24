// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "views/window/window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

const int kFileManagerWidth = 720;  // pixels
const int kFileManagerHeight = 580;  // pixels

// Returns the browser represented by |window| or NULL if not found.
// TODO(jamescook):  Move this to browser_list.h.
Browser* FindBrowserWithWindow(gfx::NativeWindow window) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it) {
    Browser* browser = *it;
    if (browser->window() && browser->window()->GetNativeHandle() == window)
      return browser;
  }
  return NULL;
}

}

// Shows a dialog box for selecting a file or a folder.
class FileManagerDialog
    : public SelectFileDialog,
      public ExtensionDialog::Observer {

 public:
  explicit FileManagerDialog(Listener* listener);

  // BaseShellDialog implementation.
  virtual bool IsRunning(gfx::NativeWindow owner_window) const OVERRIDE;
  virtual void ListenerDestroyed() OVERRIDE;

  // ExtensionDialog::Observer implementation.
  virtual void ExtensionDialogIsClosing(ExtensionDialog* dialog) OVERRIDE;

 protected:
  // SelectFileDialog implementation.
  virtual void SelectFileImpl(Type type,
                              const string16& title,
                              const FilePath& default_path,
                              const FileTypeInfo* file_types,
                              int file_type_index,
                              const FilePath::StringType& default_extension,
                              gfx::NativeWindow owning_window,
                              void* params) OVERRIDE;

 private:
  virtual ~FileManagerDialog();

  // Host for the extension that implements this dialog.
  scoped_refptr<ExtensionDialog> extension_dialog_;

  // ID of the tab that spawned this dialog, used to route callbacks.
  int32 tab_id_;

  gfx::NativeWindow owner_window_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerDialog);
};

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new FileManagerDialog(listener);
}

FileManagerDialog::FileManagerDialog(Listener* listener)
    : SelectFileDialog(listener),
      tab_id_(0),
      owner_window_(0) {
}

FileManagerDialog::~FileManagerDialog() {
  if (extension_dialog_)
    extension_dialog_->ObserverDestroyed();
  FileDialogFunction::Callback::Remove(tab_id_);
}

bool FileManagerDialog::IsRunning(gfx::NativeWindow owner_window) const {
  return owner_window_ == owner_window;
}

void FileManagerDialog::ListenerDestroyed() {
  listener_ = NULL;
  FileDialogFunction::Callback::Remove(tab_id_);
}

void FileManagerDialog::ExtensionDialogIsClosing(ExtensionDialog* dialog) {
  LOG(INFO) << "FileBrowser: ExtensionDialogIsClosing";
  owner_window_ = NULL;
  // Release our reference to the dialog to allow it to close.
  extension_dialog_ = NULL;
  FileDialogFunction::Callback::Remove(tab_id_);
}

void FileManagerDialog::SelectFileImpl(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    void* params) {
  LOG(INFO) << "FileBrowser: SelectFileImpl default_path "
    << default_path.value();
  if (owner_window_) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }
  Browser* owner_browser = (owner_window ?
      FindBrowserWithWindow(owner_window) :
      BrowserList::GetLastActive());
  if (!owner_browser) {
    NOTREACHED() << "Can't find owning browser";
    return;
  }

  GURL file_browser_url = FileManagerUtil::GetFileBrowserUrlWithParams(
      type, title, default_path, file_types, file_type_index,
      default_extension);
  extension_dialog_ = ExtensionDialog::Show(file_browser_url,
      owner_browser, kFileManagerWidth, kFileManagerHeight,
      this /* ExtensionDialog::Observer */);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  TabContents* contents = owner_browser->GetSelectedTabContents();
  int32 tab_id = (contents ? contents->controller().session_id().id() : 0);
  FileDialogFunction::Callback::Add(tab_id, listener_, params);

  tab_id_ = tab_id;
  owner_window_ = owner_window;
}
