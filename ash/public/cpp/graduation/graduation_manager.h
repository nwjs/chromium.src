// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_GRADUATION_GRADUATION_MANAGER_H_
#define ASH_PUBLIC_CPP_GRADUATION_GRADUATION_MANAGER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace ash::graduation {

// Creates interface to access browser-side functionalities in
// GraduationManagerImpl.
class ASH_PUBLIC_EXPORT GraduationManager {
 public:
  static GraduationManager* Get();

  GraduationManager();
  GraduationManager(const GraduationManager&) = delete;
  GraduationManager& operator=(const GraduationManager&) = delete;
  virtual ~GraduationManager();

  // Returns the language code of the device's current locale.
  virtual const std::string GetLanguageCode() const = 0;

  // Used by browser tests to set and fast-forward the system time.
  virtual void SetClocksForTesting(const base::Clock* clock,
                                   const base::TickClock* tick_clock) = 0;

  // Used by browser tests to resume the timer after it is paused (e.g. during
  // fast-forwarding).
  virtual void ResumeTimerForTesting() = 0;
};

}  // namespace ash::graduation

#endif  // ASH_PUBLIC_CPP_GRADUATION_GRADUATION_MANAGER_H_
