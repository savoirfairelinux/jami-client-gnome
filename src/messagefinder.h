#pragma once

#include <string>
#include <vector>
#include <media/textrecording.h>

class MessageFinder
{
public:
    explicit MessageFinder(Media::TextRecording* recording);

    ~MessageFinder() = default;
    MessageFinder(const MessageFinder& other) = default;
    MessageFinder(MessageFinder&& other) = default;
    MessageFinder& operator=(const MessageFinder& other) = default;
    MessageFinder& operator=(MessageFinder&& other) = default;

    int get_next_message() const;
    int get_previous_message() const;

    std::string get_pattern() const { return m_pattern; }
    Media::TextRecording* get_text_recording() const { return m_recording; }

    std::vector<int>& search_messages(const std::string& pattern);

private:
    std::string m_pattern;
    mutable unsigned int m_current_idx;
    std::vector<int> m_found_msg;
    Media::TextRecording* m_recording;
};
