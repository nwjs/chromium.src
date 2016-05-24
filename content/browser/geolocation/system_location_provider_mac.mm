// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "content/browser/geolocation/system_location_provider_mac.h"
#include "base/mac/scoped_nsobject.h"

#import <Foundation/Foundation.h>
#import<CoreLocation/CoreLocation.h>

namespace content {
  
class SystemLocationProviderMac
    : public base::NonThreadSafe,
    public LocationProviderBase {
  
public:
  
  SystemLocationProviderMac();
  ~SystemLocationProviderMac() override;
  
  // LocationProvider implementation
  bool StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  void GetPosition(Geoposition *position) override;
  void RequestRefresh() override;
  void OnPermissionGranted() override;

  void NotifyCallback(const Geoposition& position) {
    DCHECK(CalledOnValidThread());
    LocationProviderBase::NotifyCallback(position);
  }
  
private:
  
  // need to make it static, as in Yosemite, it will ask for permission everytime we start updating location
  static CLLocationManager* location_manager_;
  
  DISALLOW_COPY_AND_ASSIGN(SystemLocationProviderMac);
  
};

}  //namespace content

void CLLocation2Geoposition(CLLocation *location, content::Geoposition *position) {
  position->latitude          = location.coordinate.latitude;
  position->longitude         = location.coordinate.longitude;
  position->altitude          = location.altitude;
  position->accuracy          = location.horizontalAccuracy;
  position->altitude_accuracy = location.verticalAccuracy;
  position-> error_code       = content::Geoposition::ERROR_CODE_NONE;
  position->timestamp         = base::Time::FromCFAbsoluteTime(CFDateGetAbsoluteTime(CFDateRef(location.timestamp)));
  DCHECK(position->Validate());
}

@interface SystemLocationMacDelegate : NSObject<CLLocationManagerDelegate> {
@private
  content::SystemLocationProviderMac* system_location_provider_;
}
@end

@implementation SystemLocationMacDelegate

- (id) init: (content::SystemLocationProviderMac*) system_location_provider {
  system_location_provider_ = system_location_provider;
  return self;
}

- (void) dealloc {
  system_location_provider_ = NULL;
  [super dealloc];
}

- (void)locationManager:(CLLocationManager *)manager
     didUpdateLocations:(NSArray *)locations {
  CLLocation* location = [locations lastObject];
  content::Geoposition geoposition;
  CLLocation2Geoposition(location, &geoposition);
  system_location_provider_->NotifyCallback(geoposition);
}

- (void)locationManager:(CLLocationManager *)manager
       didFailWithError:(NSError *)error {
  if (error.code >= 0) {
    content::Geoposition geoposition = content::Geoposition::Geoposition();
    geoposition.error_code = error.code == kCLErrorDenied ?
      content::Geoposition::ERROR_CODE_PERMISSION_DENIED : content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;

    system_location_provider_->NotifyCallback(geoposition);
  }
}

- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
  if (status == kCLAuthorizationStatusAuthorized) {
    system_location_provider_->OnPermissionGranted();
  } else if (status != kCLAuthorizationStatusNotDetermined) {
    content::Geoposition geoposition = content::Geoposition::Geoposition();
    geoposition.error_code = content::Geoposition::ERROR_CODE_PERMISSION_DENIED;
    system_location_provider_->NotifyCallback(geoposition);
  }
}

@end


namespace content {

CLLocationManager* SystemLocationProviderMac::location_manager_ = NULL;

SystemLocationProviderMac::SystemLocationProviderMac() {
  if (location_manager_ == NULL)
    location_manager_ = [[CLLocationManager alloc] init];
}

SystemLocationProviderMac::~SystemLocationProviderMac() {
  StopProvider();
}

bool SystemLocationProviderMac::StartProvider(bool high_accuracy) {
  DCHECK(CalledOnValidThread());
  location_manager_.desiredAccuracy = kCLLocationAccuracyBest;
  location_manager_.delegate = [[SystemLocationMacDelegate alloc] init: this];
  RequestRefresh();
  return true;
}

void SystemLocationProviderMac::StopProvider() {
  DCHECK(CalledOnValidThread());
  [location_manager_ stopUpdatingLocation];
  if (location_manager_.delegate) {
    [[location_manager_ delegate] release];
    location_manager_.delegate = NULL;
  }
}

void SystemLocationProviderMac::GetPosition(Geoposition *position) {
  DCHECK(CalledOnValidThread());
  CLLocation2Geoposition(location_manager_.location, position);
}

void SystemLocationProviderMac::RequestRefresh() {
  DCHECK(CalledOnValidThread());
  [location_manager_ startUpdatingLocation];
}

void SystemLocationProviderMac::OnPermissionGranted() {
  DCHECK(CalledOnValidThread());
  RequestRefresh();
}

// SystemLocationProvider factory function
LocationProvider* NewSystemLocationProvider() {
  return new SystemLocationProviderMac();
}
  
}  // namespace content
