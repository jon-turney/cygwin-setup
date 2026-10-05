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

#include "SiteSpeedEstimator.h"
#include "netio.h"
#include "ini.h"
#include "LogFile.h"
#include "Feedback.h"

#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>

SiteSpeedEstimator::SiteSpeedEstimator(SiteList &site_list)
  : current_site_list_(&site_list)
{
}

SiteSpeedEstimator::~SiteSpeedEstimator()
{
}

void
SiteSpeedEstimator::annotate_sitelist(Feedback  &feedback)
{
  std::vector<std::thread> worker_threads;

  int max_threads = std::thread::hardware_concurrency() * 2;

  progress_counts_.assign(max_threads, 0);

  /* Populate the work queue with indices of all sites not marked 'noshow' */
  for (size_t i = 0; i < current_site_list_->size(); ++i)
    if (!(*current_site_list_)[i].noshow)
      work_queue_.push(i);

  work_count = work_queue_.size();

  auto start_time = std::chrono::high_resolution_clock::now();

  Log (LOG_PLAIN) << "Starting speed measurement for "
                  << work_count << " site(s) with "
                  << max_threads << " concurrent thread(s)" << endLog;

  /* Create worker threads */
  for (int i = 0; i < max_threads; ++i)
    worker_threads.emplace_back([this, i]() { worker_thread(i); });

  /* Create progress reporting thread */
  std::thread progress = std::thread([this, &feedback]() { progress_thread(feedback); });

  /* Wait for all worker threads to complete */
  for (auto &thread : worker_threads)
    {
      if (thread.joinable())
        thread.join();
    }

  /* Wait for progress reporting thread */
  progress.join();

  /* Calculate elapsed time */
  auto end_time = std::chrono::high_resolution_clock::now();
  double elapsed = (std::chrono::duration<double>(end_time - start_time)).count();

  Log (LOG_PLAIN) << "Speed measurement complete in " << elapsed << " seconds" << endLog;
}

void
SiteSpeedEstimator::worker_thread(int worker_index)
{
  while (true)
    {
      size_t site_index;

      /* Get next work item from queue (in a threadsafe manner) */
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if (work_queue_.empty())
          break;

        site_index = work_queue_.front();
        work_queue_.pop();
      }

      /* Perform the measurement */
      double speed = estimate_speed((*current_site_list_)[site_index]);
      (*current_site_list_)[site_index].speed = speed;

      /* Increment this worker's progress count */
      progress_counts_[worker_index]++;
    }
}

void
SiteSpeedEstimator::progress_thread(Feedback  &feedback)
{
  while (true)
    {
      /* Count total work consumed by all workers */
      unsigned int total_progress = 0;
      for (unsigned int count : progress_counts_)
        total_progress += count;

      /* Report progress */
      feedback.phase_progress(total_progress, work_count);

      /* Exit if no work remains... */
      if (total_progress >= work_count)
        break;

      /* ... otherwise wait */
      Sleep(1000);
    }
}

/* Maximum time allowed for a single speed test in seconds */
#define TIMEOUT 5

/* Maximum amount of test file to download (128 KB) */
#define TEST_FILE_SIZE (1024*128)

double
SiteSpeedEstimator::estimate_speed(const site_list_type &site)
{
  double speed = 0;

  std::string test_url = site.url;
  if (test_url.empty())
    return speed;

  test_url += SetupArch();
  test_url += '/';
  test_url += SetupBaseName();
  test_url += ".ini";

  /* Attempt to open connection to URL */
  NetIO *connection = NetIO::open(test_url.c_str(), false);
  if (!connection || !connection->ok())
    return speed;

  auto start_time = std::chrono::high_resolution_clock::now();
  auto timeout_time = start_time + std::chrono::seconds(TIMEOUT);

  char buffer[4096];
  size_t total_bytes = 0;

  /* Download data until timeout or test_file_size reached */
  while (true)
    {
      /* Check if we've exceeded the timeout */
      auto current_time = std::chrono::high_resolution_clock::now();
      if (current_time >= timeout_time)
        break;

      int bytes_read = connection->read(buffer, sizeof(buffer));
      if (bytes_read <= 0)
        break;

      total_bytes += bytes_read;

      /* Stop if we've downloaded enough data */
      if (total_bytes >= TEST_FILE_SIZE)
        break;
    }

  delete connection;

  /* Calculate elapsed time */
  auto end_time = std::chrono::high_resolution_clock::now();
  double elapsed = (std::chrono::duration<double>(end_time - start_time)).count();

  if (elapsed > 0 && total_bytes > 0)
    {
      /* Calculate speed, converting bytes -> bits */
      speed = (total_bytes * 8.0) / elapsed;
    }

  return speed;
}
