// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_

#include <map>
#include <string>

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/base_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace base::allocator {

#if PA_CONFIG(ALLOW_PCSCAN)
BASE_EXPORT void RegisterPCScanStatsReporter();
#endif

// Starts a periodic timer on the current thread to purge all thread caches.
BASE_EXPORT void StartThreadCachePeriodicPurge();

BASE_EXPORT void StartMemoryReclaimer(
    scoped_refptr<SequencedTaskRunner> task_runner);

BASE_EXPORT std::map<std::string, std::string> ProposeSyntheticFinchTrials();

// Install handlers for when dangling raw_ptr(s) have been detected. This prints
// two StackTraces. One where the memory is freed, one where the last dangling
// raw_ptr stopped referencing it.
//
// This is currently effective, only when compiled with
// `enable_dangling_raw_ptr_checks` build flag.
BASE_EXPORT void InstallDanglingRawPtrChecks();
BASE_EXPORT void InstallUnretainedDanglingRawPtrChecks();

// Allows to re-configure PartitionAlloc at run-time.
class BASE_EXPORT PartitionAllocSupport {
 public:
  // Reconfigure* functions re-configure PartitionAlloc. It is impossible to
  // configure PartitionAlloc before/at its initialization using information not
  // known at compile-time (e.g. process type, Finch), because by the time this
  // information is available memory allocations would have surely happened,
  // that requiring a functioning allocator.
  //
  // *Earlyish() is called as early as it is reasonably possible.
  // *AfterZygoteFork() is its complement to finish configuring process-specific
  // stuff that had to be postponed due to *Earlyish() being called with
  // |process_type==kZygoteProcess|.
  // *AfterFeatureListInit() is called in addition to the above, once
  // FeatureList has been initialized and ready to use. It is guaranteed to be
  // called on non-zygote processes or after the zygote has been forked.
  // *AfterTaskRunnerInit() is called once it is possible to post tasks, and
  // after the previous steps.
  //
  // *Earlyish() must be called exactly once. *AfterZygoteFork() must be called
  // once iff *Earlyish() was called before with |process_type==kZygoteProcess|.
  //
  // *AfterFeatureListInit() may be called more than once, but will perform its
  // re-configuration steps exactly once.
  //
  // *AfterTaskRunnerInit() may be called more than once.
  void ReconfigureEarlyish(const std::string& process_type);
  void ReconfigureAfterZygoteFork(const std::string& process_type);
  void ReconfigureAfterFeatureListInit(const std::string& process_type);
  void ReconfigureAfterTaskRunnerInit(const std::string& process_type);

  // |has_main_frame| tells us if the renderer contains a main frame.
  void OnForegrounded(bool has_main_frame);
  void OnBackgrounded();

  static PartitionAllocSupport* Get() {
    static auto* singleton = new PartitionAllocSupport();
    return singleton;
  }

 private:
  PartitionAllocSupport();

  base::Lock lock_;
  bool called_earlyish_ GUARDED_BY(lock_) = false;
  bool called_after_zygote_fork_ GUARDED_BY(lock_) = false;
  bool called_after_feature_list_init_ GUARDED_BY(lock_) = false;
  bool called_after_thread_pool_init_ GUARDED_BY(lock_) = false;
  std::string established_process_type_ GUARDED_BY(lock_) = "INVALID";

#if PA_CONFIG(THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  size_t largest_cached_size_ =
      ::partition_alloc::ThreadCacheLimits::kDefaultSizeThreshold;
#endif
};

}  // namespace base::allocator

#endif  // BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_
