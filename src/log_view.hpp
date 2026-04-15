#pragma once
#include <httplib.h>

class LogView {
public:
    LogView();

    void run();

private:
    [[nodiscard]] std::string random_id() const;
    [[nodiscard]] std::string html_escape(std::string_view s) const;
    [[nodiscard]] std::string color_for_priority(int p) const;
    [[nodiscard]] std::string format_realtime_timestamp(std::string_view value) const;
    [[nodiscard]] std::string render_log_file(std::string_view content) const;
    [[nodiscard]] std::string upload_page() const;
    void remove_old_logs() const;
    [[nodiscard]] std::size_t storage_size() const;

    httplib::Server m_server; // 752
    std::string m_storage_dir{}; // 32
    std::string_view m_file_cfg{"/etc/log-view/cfg.json"}; // 16
    std::size_t m_max_storage_size{}; // 8
    uint64_t m_port; // 8
};
