/**
 * @file xpuQueue.cpp
 * @brief Queue management - XPU Module 4
 *
 * Manages playback queue with persistence support
 */

#include "QueueManager.h"
#include "../xpuLoad/AudioFileLoader.h"
#include "../xpuLoad/DSDDecoder.h"
#include "protocol/ErrorCode.h"
#include "protocol/ErrorResponse.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

using namespace xpu;
using namespace xpu::queue;

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <command> [options]\n";
    std::cout << "\nCommands:\n";
    std::cout << "  add <files>          Add files to queue\n";
    std::cout << "  remove <index>       Remove file from queue\n";
    std::cout << "  list                 List queue contents\n";
    std::cout << "  clear                Clear queue\n";
    std::cout << "  next                 Jump to next track\n";
    std::cout << "  previous             Jump to previous track\n";
    std::cout << "  play                 Start/resume playback\n";
    std::cout << "  pause                Pause playback\n";
    std::cout << "  stop                 Stop playback\n";
    std::cout << "  shuffle              Toggle shuffle mode\n";
    std::cout << "  loop                 Toggle loop mode\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help           Show this help message\n";
    std::cout << "  -v, --version        Show version information\n";
    std::cout << "  -V, --verbose        Enable verbose output\n";
    std::cout << "\nQueue persistence:\n";
    std::cout << "  Queue is automatically saved to: ";
    std::cout << utils::PlatformUtils::getConfigDirectory() + "/queue.json\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "xpuQueue version 0.1.0\n";
    std::cout << "XPU - Cross-Platform Professional Audio Playback System\n";
}

/**
 * @brief Load metadata from file
 */
ErrorCode loadMetadata(const std::string& file_path, protocol::AudioMetadata& metadata) {
    // Check file extension for DSD
    bool is_dsd = false;
    if (file_path.size() > 4) {
        std::string ext = file_path.substr(file_path.size() - 4);
        if (ext == ".dsf" || ext == ".dff") {
            is_dsd = true;
        }
    }

    ErrorCode ret;
    if (is_dsd) {
        load::DSDDecoder decoder;
        ret = decoder.load(file_path);
        if (ret == ErrorCode::Success) {
            metadata = decoder.getMetadata();
        }
    } else {
        load::AudioFileLoader loader;
        ret = loader.load(file_path);
        if (ret == ErrorCode::Success) {
            metadata = loader.getMetadata();
        }
    }

    return ret;
}

/**
 * @brief Add files to queue
 */
ErrorCode addFiles(QueueManager& queue, const std::vector<std::string>& files) {
    int success_count = 0;
    int fail_count = 0;

    for (const auto& file : files) {
        protocol::AudioMetadata metadata;
        ErrorCode ret = loadMetadata(file, metadata);

        if (ret == ErrorCode::Success) {
            ret = queue.addTrack(file, metadata);
            if (ret == ErrorCode::Success) {
                std::cout << "Added: " << metadata.title
                         << " (" << metadata.artist << ")\n";
                success_count++;
            } else {
                std::cerr << "Failed to add: " << file << "\n";
                fail_count++;
            }
        } else {
            ErrorResponse error(ret);
            std::cerr << "Failed to load: " << file << " - "
                      << error.message << "\n";
            fail_count++;
        }
    }

    std::cout << "Added " << success_count << " file(s)";
    if (fail_count > 0) {
        std::cout << ", " << fail_count << " failed";
    }
    std::cout << "\n";

    return ErrorCode::Success;
}

/**
 * @brief List queue contents
 */
ErrorCode listQueue(QueueManager& queue) {
    QueueState state = queue.getQueueState();

    std::cout << "{\n";
    std::cout << "  \"current_index\": " << state.current_index << ",\n";
    std::cout << "  \"playback_mode\": ";

    const char* mode_names[] = {"Sequential", "Random", "LoopSingle", "LoopAll"};
    std::cout << "\"" << mode_names[static_cast<int>(state.mode)] << "\",\n";

    std::cout << "  \"count\": " << state.entries.size() << ",\n";
    std::cout << "  \"entries\": [\n";

    for (size_t i = 0; i < state.entries.size(); ++i) {
        const QueueEntry& entry = state.entries[i];

        std::cout << "    {\n";
        std::cout << "      \"index\": " << i << ",\n";
        std::cout << "      \"file_path\": \"" << entry.file_path << "\",\n";
        std::cout << "      \"title\": \"" << entry.metadata.title << "\",\n";
        std::cout << "      \"artist\": \"" << entry.metadata.artist << "\",\n";
        std::cout << "      \"album\": \"" << entry.metadata.album << "\",\n";
        std::cout << "      \"duration\": " << entry.metadata.duration << ",\n";
        std::cout << "      \"is_current\": " << (i == static_cast<size_t>(state.current_index) ? "true" : "false") << "\n";

        if (i < state.entries.size() - 1) {
            std::cout << "    },\n";
        } else {
            std::cout << "    }\n";
        }
    }

    std::cout << "  ]\n";
    std::cout << "}\n";

    return ErrorCode::Success;
}

/**
 * @brief Remove track from queue
 */
ErrorCode removeTrack(QueueManager& queue, int index) {
    QueueState state = queue.getQueueState();

    if (index < 0 || index >= static_cast<int>(state.entries.size())) {
        std::cerr << "Error: Invalid index " << index
                  << " (queue size: " << state.entries.size() << ")\n";
        return ErrorCode::InvalidOperation;
    }

    std::string title = state.entries[index].metadata.title;
    ErrorCode ret = queue.removeTrack(index);

    if (ret == ErrorCode::Success) {
        std::cout << "Removed: " << title << " (index " << index << ")\n";
    }

    return ret;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Initialize logger
    utils::Logger::initialize(utils::PlatformUtils::getLogFilePath(), true);

    LOG_INFO("xpuQueue starting");

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "-h" || command == "--help") {
        printUsage(argv[0]);
        return 0;
    } else if (command == "-v" || command == "--version") {
        printVersion();
        return 0;
    }

    // Initialize queue manager
    QueueManager queue;
    ErrorCode ret = queue.initialize();
    if (ret != ErrorCode::Success) {
        std::cerr << "Failed to initialize queue manager\n";
        return 1;
    }

    // Process commands
    if (command == "add") {
        if (argc < 3) {
            std::cerr << "Usage: xpuQueue add <files...>\n";
            return 1;
        }

        std::vector<std::string> files;
        for (int i = 2; i < argc; ++i) {
            files.push_back(argv[i]);
        }

        ret = addFiles(queue, files);

    } else if (command == "list") {
        ret = listQueue(queue);

    } else if (command == "remove") {
        if (argc < 3) {
            std::cerr << "Usage: xpuQueue remove <index>\n";
            return 1;
        }

        int index = std::atoi(argv[2]);
        ret = removeTrack(queue, index);

    } else if (command == "clear") {
        ret = queue.clearQueue();
        if (ret == ErrorCode::Success) {
            std::cout << "Queue cleared\n";
        }

    } else if (command == "next") {
        QueueEntry entry;
        ret = queue.getNextTrack(entry);
        if (ret == ErrorCode::Success) {
            std::cout << "Next: " << entry.metadata.title
                     << " (" << entry.metadata.artist << ")\n";
        } else if (ret == ErrorCode::EndOfQueue) {
            std::cout << "End of queue reached\n";
        } else {
            std::cerr << "Failed to get next track\n";
        }

    } else if (command == "previous") {
        QueueEntry entry;
        ret = queue.getPreviousTrack(entry);
        if (ret == ErrorCode::Success) {
            std::cout << "Previous: " << entry.metadata.title
                     << " (" << entry.metadata.artist << ")\n";
        } else if (ret == ErrorCode::EndOfQueue) {
            std::cout << "Already at beginning of queue\n";
        } else {
            std::cerr << "Failed to get previous track\n";
        }

    } else if (command == "play") {
        QueueEntry entry;
        ret = queue.getCurrentTrack(entry);
        if (ret == ErrorCode::Success) {
            std::cout << "Now playing: " << entry.metadata.title
                     << " (" << entry.metadata.artist << ")\n";
            std::cout << "File: " << entry.file_path << "\n";
        } else {
            std::cerr << "No track in queue\n";
        }

    } else if (command == "pause") {
        std::cout << "Pause command (to be implemented with player integration)\n";

    } else if (command == "stop") {
        std::cout << "Stop command (to be implemented with player integration)\n";

    } else if (command == "shuffle") {
        ret = queue.setPlaybackMode(PlaybackMode::Random);
        if (ret == ErrorCode::Success) {
            std::cout << "Shuffle mode enabled\n";
        }

    } else if (command == "loop") {
        QueueState state = queue.getQueueState();
        if (state.mode == PlaybackMode::LoopAll) {
            ret = queue.setPlaybackMode(PlaybackMode::Sequential);
            if (ret == ErrorCode::Success) {
                std::cout << "Loop mode disabled\n";
            }
        } else {
            ret = queue.setPlaybackMode(PlaybackMode::LoopAll);
            if (ret == ErrorCode::Success) {
                std::cout << "Loop mode enabled\n";
            }
        }

    } else {
        std::cerr << "Unknown command: " << command << "\n";
        printUsage(argv[0]);
        return 1;
    }

    if (ret != ErrorCode::Success) {
        ErrorResponse error(ret);
        std::cerr << "Error: " << error.message << "\n";
        return static_cast<int>(getHTTPStatusCode(ret));
    }

    LOG_INFO("xpuQueue completed successfully");
    return 0;
}
