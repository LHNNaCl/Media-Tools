#pragma once

#include "mediatool/types.hpp"

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace MediaTool {

struct ProgressInfo {
    int index = 1;
    int count = 1;
    std::string name;
    int percent = -1;
    bool merge = false;
};

struct JobRequest {
    PlanRequest options;
    std::vector<std::string> files;
    std::string folderPath;
    std::optional<MediaInfo> known;
    EncoderAvailability encoders;
    bool chooseEncoder = false;
};

struct CancelState {
    std::atomic<bool> stop{false};
    std::atomic<bool> skip{false};
};

class TaskRunner {
public:
    std::function<std::optional<MediaInfo>(const std::string& path, std::string& error)> probe;
    std::function<int(const std::vector<std::string>& arguments, double durationSeconds, const std::function<void(int)>& percent, const std::function<bool()>& canceled)> run;
    std::function<void(const ProgressInfo&)> onProgress;
    CancelState* cancel = nullptr;

    std::vector<ItemResult> execute(const JobRequest& job);
};

std::vector<std::string> listVideos(const std::string& directory);

}  // namespace MediaTool
