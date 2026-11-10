#include "mediatool/bundle.hpp"

namespace MediaTool {

Bundle locateBundle(const std::filesystem::path& executableDirectory) {
    Bundle bundle;
    bundle.root = executableDirectory;
    bundle.ffmpeg = executableDirectory / "ffmpeg" / "ffmpeg.exe";
    bundle.ffprobe = executableDirectory / "ffmpeg" / "ffprobe.exe";
    bundle.licenseApp = executableDirectory / "licenses" / "LICENSE.txt";
    bundle.licenseFfmpeg = executableDirectory / "licenses" / "FFMPEG-LICENSE.txt";
    bundle.sourceNote = executableDirectory / "licenses" / "SOURCE.txt";
    std::error_code error;
    bundle.valid = std::filesystem::is_regular_file(bundle.ffmpeg, error) && std::filesystem::is_regular_file(bundle.ffprobe, error);
    return bundle;
}

}  // namespace MediaTool
