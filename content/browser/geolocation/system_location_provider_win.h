// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_WIN_H_
#define CONTENT_BROWSER_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_WIN_H_

#include "content/browser/geolocation/system_location_provider.h"
#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <LocationApi.h>

namespace content {

class SystemLocationDataProviderWin :
  public CComObjectRoot,
  public ILocationEvents { // We must include this interface so the Location API knows how to talk to our object

public:
  SystemLocationDataProviderWin();

  DECLARE_NOT_AGGREGATABLE(SystemLocationDataProviderWin)

  BEGIN_COM_MAP(SystemLocationDataProviderWin)
    COM_INTERFACE_ENTRY(ILocationEvents)
  END_COM_MAP()

  // ILocationEvents

  // This is called when there is a new location report
  STDMETHOD(OnLocationChanged)(REFIID reportType, ILocationReport* pLocationReport);

  // This is called when the status of a report type changes.
  // The LOCATION_REPORT_STATUS enumeration is defined in LocApi.h in the SDK
  STDMETHOD(OnStatusChanged)(REFIID reportType, LOCATION_REPORT_STATUS status);

  void DoRunCallbacks(Geoposition position);

  typedef base::Callback<void(Geoposition)> SystemLocationUpdateCallback;
  void Register(SystemLocationUpdateCallback* system_location_callback);
  void UnRegister(SystemLocationUpdateCallback* system_location_callback);


protected:
  void PostTask(Geoposition& position);
  virtual ~SystemLocationDataProviderWin();

  // callback to SystemLocationProviderWin, currently we only store 1 callback, change it to set if more is needed
  SystemLocationUpdateCallback* system_location_callback_;

  // Reference to the client's message loop. All callbacks should happen in this
  // context.
  base::MessageLoop* client_loop_;
};

class SystemLocationProviderWin :
  public base::NonThreadSafe,
  public LocationProviderBase {
public:
  SystemLocationProviderWin();
  ~SystemLocationProviderWin() override;

  // LocationProvider implementation
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  void GetPosition(Geoposition *position) override;
  void RequestRefresh() override;
  void OnPermissionGranted() override;

  void NotifyCallback(Geoposition position);

private:
  CComPtr<ILocation> location_; // This is the main Location interface
  CComObject<SystemLocationDataProviderWin>* location_data_provider_; // This is our callback object for location reports
  IID report_type_;
  
  SystemLocationDataProviderWin::SystemLocationUpdateCallback system_location_callback_;

  DISALLOW_COPY_AND_ASSIGN(SystemLocationProviderWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_WIN_H_
