/**
 * @file linux_crash_logger.cpp
 * @brief Durable Linux crash report snapshots and BugSplat uploads
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// <FS:Beq>
#include "linux_crash_report.h"
#include <boost/json/src.hpp>
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace json = boost::json;
namespace FSCrashReport
{
namespace
{
std::string now()
{
    const auto time = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&time, &utc);
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%d %H:%M:%S UTC");
    return out.str();
}

json::object read(const fs::path& path)
{
    if (fs::file_size(path) > 1024 * 1024)
        throw std::runtime_error("Crash report metadata is too large");
    std::ifstream input(path);
    std::ostringstream text;
    text << input.rdbuf();
    if (!input.good() && !input.eof())
        throw std::runtime_error("Cannot read crash report metadata");
    return json::parse(text.str()).as_object();
}

std::string value(const json::object& object, const char* key, const std::string& fallback = {})
{
    const auto* item = object.if_contains(key);
    return item && item->is_string() ? std::string(item->as_string()) : fallback;
}

bool flag(const json::object& object, const char* key)
{
    const auto* item = object.if_contains(key);
    return item && item->is_bool() && item->as_bool();
}

void privateFile(const fs::path& path)
{
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write);
}

bool validDatabase(const std::string& database)
{
    return !database.empty() && database.size() <= 63 &&
           std::all_of(database.begin(), database.end(), [](unsigned char c)
           { return std::isalnum(c) || c == '-' || c == '_'; });
}

fs::path attachmentPath(const Report& report, const Attachment& attachment)
{
    const fs::path name(attachment.file);
    if (name.empty() || name.has_parent_path() || name == "." || name == "..")
        throw std::runtime_error("Invalid attachment filename");
    const auto path = report.directory / name;
    if (fs::is_symlink(path) || !fs::is_regular_file(path))
        throw std::runtime_error("Missing or unsafe attachment: " + attachment.file);
    return path;
}

size_t discardResponse(char*, size_t size, size_t count, void*)
{
    return size * count;
}
}

void save(const Report& report)
{
    json::array attachments;
    for (const auto& attachment : report.attachments)
        attachments.push_back(json::object{{"file", attachment.file}, {"label", attachment.label},
                              {"included", attachment.included}, {"required", attachment.required}});
    json::object object{{"schema", 1}, {"database", report.database}, {"product", report.product},
                        {"version", report.version}, {"timestamp", report.timestamp},
                        {"name", report.name}, {"description", report.description},
                        {"status", report.status}, {"attachments", attachments}};
    const auto temporary = report.directory / "report.json.tmp";
    {
        std::ofstream output(temporary, std::ios::trunc);
        privateFile(temporary);
        output << json::serialize(object) << '\n';
        output.close();
        if (!output) throw std::runtime_error("Cannot save crash report");
    }
    fs::rename(temporary, report.directory / "report.json");
}

Report load(const fs::path& manifest)
{
    const auto object = read(manifest);
    Report report;
    report.directory = fs::absolute(manifest).parent_path();
    report.database = value(object, "database");
    report.product = value(object, "product");
    report.version = value(object, "version");
    report.timestamp = value(object, "timestamp");
    report.name = value(object, "name");
    report.description = value(object, "description");
    report.status = value(object, "status", "pending");
    for (const auto& item : object.at("attachments").as_array())
    {
        const auto& attachment = item.as_object();
        report.attachments.push_back({value(attachment, "file"), value(attachment, "label"),
                                      flag(attachment, "included"), flag(attachment, "required")});
    }
    if (!validDatabase(report.database) || report.product.empty() || report.version.empty())
        throw std::runtime_error("Crash report has invalid application metadata");
    return report;
}

void log(const Report& report, const std::string& message)
{
    const auto path = report.directory / "bugsplat.log";
    std::ofstream output(path, std::ios::app);
    privateFile(path);
    output << now() << " " << message << '\n';
}

Report capture(const fs::path& dump, const std::string& version, const std::string& database)
{
    if (!validDatabase(database) || !fs::is_regular_file(dump) || fs::is_symlink(dump))
        throw std::runtime_error("Invalid crash database or minidump");
    Report report;
    report.directory = fs::absolute(dump).parent_path() / ("report-" + dump.stem().string());
    if (fs::exists(report.directory / "report.json"))
        return load(report.directory / "report.json");
    fs::create_directory(report.directory);
    fs::permissions(report.directory, fs::perms::owner_all);
    report.database = database;
    report.product = "Firestorm-Releasex64";
    report.version = version;
    report.timestamp = now();
    json::object context;
    const auto contextFile = dump.parent_path() / "report-context.json";
    try { if (fs::exists(contextFile)) context = read(contextFile); }
    catch (const std::exception&) { log(report, "Could not read application context; retaining the minidump."); }
    report.product = value(context, "product", report.product);
    report.name = value(context, "name");
    report.description = value(context, "description");

    auto copy = [&](const fs::path& source, const char* target, const char* label, bool included, bool required)
    {
        try
        {
            if (source.empty() || !fs::is_regular_file(source) || fs::is_symlink(source))
            {
                if (required) throw std::runtime_error("Missing minidump");
                return;
            }
            fs::copy_file(source, report.directory / target, fs::copy_options::overwrite_existing);
            privateFile(report.directory / target);
            report.attachments.push_back({target, label, included, required});
        }
        catch (const std::exception&)
        {
            if (required) throw;
            log(report, std::string("Could not snapshot optional file: ") + target);
        }
    };
    copy(dump, "minidump.dmp", "Crash dump (may contain information from memory)", true, true);
    // Keep local source paths and identity out of the system-context attachment.
    json::object systemContext;
    for (const auto* key : {"product", "version", "channel", "flavor", "system", "description"})
        if (const auto* item = context.if_contains(key)) systemContext[key] = *item;
    {
        const auto path = report.directory / "CrashContext.json";
        std::ofstream output(path);
        privateFile(path);
        output << json::serialize(systemContext) << '\n';
        output.close();
        if (!output) throw std::runtime_error("Cannot snapshot crash context");
        report.attachments.push_back({"CrashContext.json", "Application and system information", true, false});
    }
    copy(value(context, "debug_file"), "debug_info.log", "Viewer diagnostic information", true, false);
    if (flag(context, "include_settings"))
    {
        copy(value(context, "settings_file"), "settings.xml", "Viewer settings (may contain your username)", true, false);
        copy(value(context, "account_settings_file"), "settings_per_account.xml", "Account settings (may contain your username)", true, false);
    }
    copy(value(context, "ca_file"), "ca-bundle.crt", "TLS certificate authorities", false, false);
    // The CA bundle is used locally, never offered as an attachment.
    report.attachments.erase(std::remove_if(report.attachments.begin(), report.attachments.end(),
        [](const Attachment& a) { return a.file == "ca-bundle.crt"; }), report.attachments.end());
    save(report);
    log(report, "Captured crash report; no files have been submitted.");
    return report;
}

bool upload(Report& report, std::string& error, const std::string& test_url)
{
    if (report.status == "sent" || report.status == "discarded")
    {
        error = "This report has already been sent or discarded.";
        return false;
    }
    if (!validDatabase(report.database)) throw std::runtime_error("Invalid BugSplat database");
    // Prevent concurrent retries from sending the same report twice.
    const int lock = open((report.directory / "upload.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) != 0)
    {
        if (lock >= 0) close(lock);
        error = "Another crash reporter is submitting this report.";
        return false;
    }
    struct Unlock { int fd; ~Unlock() { flock(fd, LOCK_UN); close(fd); } } unlock{lock};
    const auto disk = load(report.directory / "report.json");
    if (disk.status == "sent" || disk.status == "discarded")
    {
        error = "This report has already been sent or discarded.";
        return false;
    }
    const std::string url = test_url.empty()
        ? "https://" + report.database + ".bugsplat.com/post/bp/crash/crashpad.php" : test_url;
    auto* curl = curl_easy_init();
    if (!curl) { error = "Could not initialize the uploader."; return false; }
    struct curl_httppost* form = nullptr;
    struct curl_httppost* tail = nullptr;
    struct Cleanup
    {
        CURL* curl; curl_httppost*& form;
        ~Cleanup() { curl_formfree(form); curl_easy_cleanup(curl); }
    } cleanup{curl, form};
    auto field = [&](const char* name, const std::string& value)
    {
        if (curl_formadd(&form, &tail, CURLFORM_COPYNAME, name,
                         CURLFORM_COPYCONTENTS, value.c_str(), CURLFORM_END) != CURL_FORMADD_OK)
            throw std::runtime_error("Cannot prepare crash report field");
    };
    field("format", "minidump");
    field("database", report.database);
    field("product", report.product);
    field("version", report.version);
    field("user", report.name);
    field("comments", report.description);
    field("list_annotations", report.description);
    bool hasDump = false;
    for (const auto& attachment : report.attachments)
    {
        if (!attachment.included && !attachment.required) continue;
        const auto path = attachmentPath(report, attachment);
        const bool dump = attachment.file == "minidump.dmp";
        const std::string name = dump ? "upload_file_minidump" : attachment.file;
        hasDump |= dump;
        if (curl_formadd(&form, &tail, CURLFORM_COPYNAME, name.c_str(),
                         CURLFORM_FILE, path.c_str(), CURLFORM_END) != CURL_FORMADD_OK)
            throw std::runtime_error("Cannot prepare crash report attachment");
    }
    if (!hasDump) throw std::runtime_error("Crash report is missing its minidump");
    save(report);
    log(report, "Submitting report to BugSplat database " + report.database + ".");
    char detail[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, detail);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardResponse);
    if (fs::exists(report.directory / "ca-bundle.crt"))
        curl_easy_setopt(curl, CURLOPT_CAINFO, (report.directory / "ca-bundle.crt").c_str());
    const auto result = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    const bool sent = result == CURLE_OK && http >= 200 && http < 300;
    if (sent)
    {
        report.status = "sent";
        error.clear();
        log(report, "Upload accepted (HTTP " + std::to_string(http) + ").");
    }
    else
    {
        error = result == CURLE_OK ? "BugSplat returned HTTP " + std::to_string(http)
                                  : std::string("Upload failed: ") + (detail[0] ? detail : curl_easy_strerror(result));
        report.status = "pending";
        log(report, error + "; report retained for retry.");
    }
    save(report);
    return sent;
}

std::vector<fs::path> pending(const fs::path& logs)
{
    std::vector<fs::path> result;
    if (!fs::is_directory(logs)) return result;
    for (const auto& run : fs::directory_iterator(logs))
    {
        if (run.is_symlink() || !run.is_directory() || run.path().filename().string().find("dump-") != 0) continue;
        for (const auto& entry : fs::directory_iterator(run.path()))
        {
            const auto manifest = entry.path() / "report.json";
            if (entry.is_symlink() || !entry.is_directory() || !fs::is_regular_file(manifest)) continue;
            try { if (load(manifest).status == "pending") result.push_back(manifest); }
            catch (const std::exception&) { /* One damaged report must not hide the others. */ }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
}
// </FS:Beq>
