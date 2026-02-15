/*
 * Copyright (c) 2026, Cygwin Contributors.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Jon Turney <jon.turney@dronecode.org.uk>
 *
 */

#ifndef SETUP_SITE_SPEED_ESTIMATOR_H
#define SETUP_SITE_SPEED_ESTIMATOR_H

#include "SiteSetting.h"
#include <mutex>
#include <queue>

class SiteSpeedEstimator
{
public:
  SiteSpeedEstimator(SiteList &site_list);
  ~SiteSpeedEstimator();

  /* Measure speed for each site in the list using a thread pool. */
  void annotate_sitelist();

private:
  /* Measure speed for a single site */
  double estimate_speed(const site_list_type &site);

  /* Worker thread function for the thread pool */
  void worker_thread();

  std::mutex queue_mutex_;
  std::queue<size_t> work_queue_;
  SiteList *current_site_list_;
};

#endif /* SETUP_SITE_SPEED_ESTIMATOR_H */
