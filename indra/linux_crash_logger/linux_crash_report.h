/**
 * @file linux_crash_report.h
 * @brief Durable Linux crash reports, independent of the viewer and GUI.
 *
 * Copyright (c) 2026, The Phoenix Firestorm Project, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

// <FS:Beq> Keep report capture/upload usable even when no display is available.
#include <filesystem>
#include <string>
#include <vector>

namespace FSCrashReport
{
struct Attachment
{
    std::string file;
    std::string label;
    bool included = false;
    bool required = false;
};

struct Report
{
    std::filesystem::path directory;
    std::string database;
    std::string product;
    std::string version;
    std::string timestamp;
    std::string name;
    std::string description;
    std::string status = "pending";
    std::vector<Attachment> attachments;
};

Report capture(const std::filesystem::path& dump, const std::string& version,
               const std::string& database);
Report load(const std::filesystem::path& manifest);
void save(const Report& report);
void log(const Report& report, const std::string& message);
// An override is used only by the separately built local HTTP test driver.
bool upload(Report& report, std::string& error, const std::string& test_url = {});
std::vector<std::filesystem::path> pending(const std::filesystem::path& logs);
}
// </FS:Beq>
