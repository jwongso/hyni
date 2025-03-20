#include "whisper.h"
#include "common.h"
#include "common-sdl.h"
#include "chatapi.h"
#include <iostream>
#include <termios.h>
#include <thread>
#include <atomic>
#include <string>
#include <unistd.h>
#include <vector>
#include <algorithm> // For std::search

// Global flag for pause/resume
std::atomic<bool> is_paused(false);

// Global vector to store transcriptions
std::vector<std::string> transcriptions;

// Function to listen for key presses
void key_listener(ChatAPI& chatapi, audio_async& audio) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);  // Disable buffering and echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (true) {
        char ch = getchar();
        if (ch == 's') {
            // Send transcriptions to ChatAPI
            if (!transcriptions.empty()) {
                std::string combined_transcription;
                for (const auto& text : transcriptions) {
                    combined_transcription += text + " ";
                }

                std::cout << " -------------------------- " << std::endl;
                std::cout << "Prompt: " << combined_transcription << std::endl;
                std::cout << " -------------------------- " << std::endl;

                std::string response = chatapi.sendMessage(combined_transcription);
                std::string reply = chatapi.getAssistantReply(response);
                std::cout << "Assistant: " << reply << std::endl;
            }

            // Clear the transcriptions vector and audio buffer
            transcriptions.clear();
            audio.clear();
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Restore terminal settings
}

// Function to extract new content from the transcription
std::string extract_new_content(const std::string& previous, const std::string& current) {
    if (previous.empty()) {
        return current; // No previous transcription, return the entire current transcription
    }

    // Find the position where the previous transcription ends in the current transcription
    auto pos = std::search(current.begin(), current.end(), previous.begin(), previous.end());
    if (pos != current.end()) {
        // Extract the new content after the previous transcription
        return std::string(pos + previous.length(), current.end());
    }

    // No match found, return the entire current transcription
    return current;
}

// Function to remove partial bracketed text (e.g., [inaudible], [ Background Conversations ])
std::string remove_bracketed_text(const std::string& text) {
    std::string cleaned_text = text;
    size_t start_pos = cleaned_text.find('[');
    while (start_pos != std::string::npos) {
        size_t end_pos = cleaned_text.find(']', start_pos);
        if (end_pos != std::string::npos) {
            // Remove the bracketed text
            cleaned_text.erase(start_pos, end_pos - start_pos + 1);
        } else {
            break; // No closing bracket found
        }
        start_pos = cleaned_text.find('[', start_pos);
    }
    return cleaned_text;
}

// Function to trim leading and trailing whitespace
std::string lrtrim(const std::string& text) {
    const char* whitespace = " \t\n\r";
    size_t start = text.find_first_not_of(whitespace);
    size_t end = text.find_last_not_of(whitespace);

    if (start == std::string::npos) {
        return ""; // The string is all whitespace
    }

    return text.substr(start, end - start + 1);
}

int main() {
    // Initialize whisper context
    struct whisper_context_params cparams = whisper_context_default_params();
    struct whisper_context* ctx = whisper_init_from_file_with_params("models/ggml-small.en.bin", cparams); // Use the base model
    if (!ctx) {
        std::cerr << "Failed to initialize Whisper context.\n";
        return 1;
    }

    // Initialize audio capture
    audio_async audio(60000); // 60-second buffer (to accommodate 30-second chunks)
    if (!audio.init(-1, WHISPER_SAMPLE_RATE)) {
        std::cerr << "Failed to initialize audio capture.\n";
        return 1;
    }
    audio.resume();

    // Initialize ChatAPI
    ChatAPI chatapi("API_KEY_",
                    "https://api.deepseek.com/v1/chat/completions",
                    "deepseek-coder");


    // std::string response = chatapi.sendMessage("Hello, world!", 50, 0.7f);
    // std::string reply = chatapi.getAssistantReply(response);
    // std::cout << "Assistant Reply: " << reply << std::endl;

    // Start the key listener thread
    std::thread key_thread(key_listener, std::ref(chatapi), std::ref(audio));
    key_thread.detach();

    // Main loop
    std::vector<float> pcmf32(WHISPER_SAMPLE_RATE * 10, 0.0f); // 10 seconds of audio
    std::string previous_transcription;

    // VAD parameters
    float vad_thold = 0.8f; // Increase to reduce sensitivity
    float freq_thold = 100.0f; // Frequency threshold for VAD

    while (true) {
        // Capture audio
        audio.get(10000, pcmf32); // Get 30 seconds of audio

        // Run VAD to check for speech activity
        if (::vad_simple(pcmf32, WHISPER_SAMPLE_RATE, 1000, vad_thold, freq_thold, false)) {
            // Run inference only if speech is detected
            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            wparams.print_progress = false;
            wparams.print_realtime = false;
            wparams.no_context = true; // Disable context carryover
            wparams.language = "en";
            wparams.n_threads = 8; // Set the number of threads here

            if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
                std::cerr << "Failed to process audio.\n";
                break;
            }

            // Get the latest transcription
            const int n_segments = whisper_full_n_segments(ctx);
            if (n_segments > 0) {
                std::string current_transcription;
                for (int i = 0; i < n_segments; ++i) {
                    const char* text = whisper_full_get_segment_text(ctx, i);
                    if (text) {
                        current_transcription += text;
                    }
                }

                // Remove partial bracketed text (e.g., [inaudible], [ Background Conversations ])
                current_transcription = remove_bracketed_text(current_transcription);

                // Trim leading and trailing whitespace
                current_transcription = lrtrim(current_transcription);

                // Skip if the transcription is empty after cleaning
                if (current_transcription.empty()) {
                    continue;
                }

                // Extract new content by comparing with the previous transcription
                std::string new_content = extract_new_content(previous_transcription, current_transcription);

                // Add the new content to the transcriptions vector if it's not empty
                if (!new_content.empty()) {
                    transcriptions.push_back(new_content);
                    std::cout << new_content << std::endl; // Print the new content
                    previous_transcription = current_transcription; // Update the previous transcription
                }
            }
        }
    }

    // Clean up
    audio.pause();
    whisper_free(ctx);
    return 0;
}
