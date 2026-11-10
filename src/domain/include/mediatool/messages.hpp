#pragma once

#include <string>

namespace MediaTool {

inline constexpr const char* kNoAudio = "没有音轨";
inline constexpr const char* kNoSubtitle = "没有字幕";
inline constexpr const char* kImageSubtitle = "这一轨是图像字幕，请选原样";
inline constexpr const char* kNoSidecarSub = "找不到同名字幕";
inline constexpr const char* kNoSidecarAudio = "找不到同名音频";
inline constexpr const char* kBadSubtitle = "字幕文件无法读取";
inline constexpr const char* kBadTime = "时间无效";
inline constexpr const char* kTimePastEnd = "结束时间超过视频时长";
inline constexpr const char* kContainerFit = "目标容器无法直接容纳现有编码";
inline constexpr const char* kCannotWrite = "无法写入";
inline constexpr const char* kCannotRead = "无法读取这个文件";
inline constexpr const char* kCanceled = "已取消";
inline constexpr const char* kNotProcessed = "未处理";
inline constexpr const char* kCorrupt = "处理失败，请检查文件是否损坏";
inline constexpr const char* kNoVideoInFolder = "这个文件夹里没有可处理的视频";
inline constexpr const char* kNeedTwo = "至少需要两个视频";
inline constexpr const char* kNoFfmpeg = "找不到随包的 FFmpeg";
inline constexpr const char* kWebmVp9Only = "WebM 只能使用 VP9";
inline constexpr const char* kDone = "已完成";
inline constexpr const char* kUsedMp4 = "已改用 MP4";
inline constexpr const char* kVolumeInvalid = "音量必须是 0 到 200 的整数";
inline constexpr const char* kBitrateInvalid = "码率必须是 1 到 200000 的整数";
inline constexpr const char* kPickVideo = "请选择视频";
inline constexpr const char* kPickSubtitle = "请选择字幕文件";
inline constexpr const char* kPickAudio = "请选择音频文件";

inline std::string cannotReadNamed(const std::string& fileName) {
    return std::string(kCannotRead) + "：" + fileName;
}

inline std::string successWith(const std::string& extra) {
    if (extra.empty()) return kDone;
    return std::string(kDone) + "；" + extra;
}

inline std::string usedFile(const std::string& fileName) {
    return std::string("已使用 ") + fileName;
}

inline const char* statusText(ItemStatus status) {
    switch (status) {
    case ItemStatus::Succeeded: return "成功";
    case ItemStatus::Failed: return "失败";
    case ItemStatus::Canceled: return "已取消";
    case ItemStatus::NotProcessed: return "未处理";
    }
    return "失败";
}

}  // namespace MediaTool
