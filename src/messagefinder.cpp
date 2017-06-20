#include "messagefinder.h"

#include <regex>

MessageFinder::MessageFinder(Media::TextRecording* recording) :
m_pattern(), m_current_idx(0), m_found_msg(), m_recording(recording) { }

int
MessageFinder::get_next_message() const
{
    if (this->m_current_idx >= this->m_found_msg.size()) return -1;
    this->m_current_idx ++;
    return this->m_found_msg[this->m_current_idx - 1];
}

int
MessageFinder::get_previous_message() const
{
    if (this->m_found_msg.size() == 0 || this->m_current_idx == 0) return -1;
    this->m_current_idx --;
    return this->m_found_msg[this->m_current_idx];
}

std::vector<int>&
MessageFinder::search_messages(const std::string& pattern)
{
    this->m_current_idx = 0;
    this->m_pattern = pattern;
    this->m_found_msg.clear();
    auto model = this->m_recording->instantTextMessagingModel();
    std::regex re(pattern);

    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex idx = model->index(row, 0);
        auto message = idx.data().value<QString>().toStdString();
        std::smatch match;
        if (std::regex_search(message, match, re) && match.size() > 0) {
            this->m_found_msg.push_back(idx.row());
        }
    }

    return this->m_found_msg;
}
