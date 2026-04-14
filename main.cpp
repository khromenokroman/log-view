#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace {
std::string html_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::string color_for_priority(int p) {
    switch (p) {
        case 0:
        case 1:
        case 2: return "#ff5555"; // emergency/alert/crit
        case 3: return "#ff4444"; // error
        case 4: return "#ffaa00"; // warning
        case 5: return "#ffffff"; // notice
        case 6: return "#dddddd"; // info
        case 7: return "#888888"; // debug
        default: return "#ffffff";
    }
}

std::string format_realtime_timestamp(std::string_view value) {
    if (value.empty()) {
        return {};
    }

    auto const us = std::stoll(std::string(value));
    auto const sec = static_cast<std::time_t>(us / 1000000);
    auto const micros = static_cast<int>(us % 1000000);

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &sec);
#else
    localtime_r(&sec, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(6) << std::setfill('0') << micros;
    return oss.str();
}

std::string render_log_file(std::string_view content) {
    std::ostringstream out;

    out << "<!doctype html><html><head><meta charset='utf-8'>"
        << "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        << "<title>pretty_log</title>"
        << "<style>"
        << "body{background:#111;color:#ddd;font-family:monospace;padding:20px;}"
        << ".line{white-space:pre-wrap;line-height:1.45;margin:0;}"
        << ".ts{color:#777;}"
        << ".host{color:#777;}"
        << ".ident{color:#66d9ef;}"
        << ".msg{white-space:pre-wrap;}"
        << ".err{color:#ff5555;}"
        << ".box{background:#1a1a1a;border:1px solid #333;padding:16px;border-radius:8px;}"
        << "a{color:#8be9fd;}"
        << "input,button{font:inherit;}"
        << "</style></head><body>";

    out << "<h1>pretty_log</h1>";
    out << "<p><a href='/'>← загрузить другой файл</a></p>";
    out << "<div class='box'>";

    std::istringstream input{std::string(content)};
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        try {
            auto json = nlohmann::json::parse(line);

            auto ts = json.value("__REALTIME_TIMESTAMP", "");
            auto host = json.value("_HOSTNAME", "");
            auto ident = json.value("_COMM", "unknown");
            auto pid = json.value("_PID", "unknown");
            auto msg = json.value("MESSAGE", "");
            auto prio_str = json.value("PRIORITY", "6");

            int prio = 6;
            try {
                prio = std::stoi(prio_str);
            } catch (...) {
                prio = 6;
            }

            auto color = color_for_priority(prio);

            out << "<p class='line'>";

            auto ts_fmt = format_realtime_timestamp(ts);
            if (!ts_fmt.empty()) {
                out << "<span class='ts'>" << html_escape(ts_fmt) << "</span> ";
            }

            if (!host.empty()) {
                out << "<span class='host'>" << html_escape(host) << "</span> ";
            }

            if (!ident.empty()) {
                out << "<span class='ident'>" << html_escape(ident)
                    << "[" << html_escape(pid) << "]</span>: ";
            }

            std::istringstream msg_stream(msg);
            std::string msg_line;
            bool first = true;

            while (std::getline(msg_stream, msg_line)) {
                if (first) {
                    out << "<span class='msg' style='color:" << color << ";'>"
                        << html_escape(msg_line) << "</span>";
                    first = false;
                } else {
                    out << "<br><span class='msg' style='color:" << color << ";'>&nbsp;&nbsp;&nbsp;&nbsp;"
                        << html_escape(msg_line) << "</span>";
                }
            }

            if (msg.empty()) {
                out << "<span class='msg' style='color:" << color << ";'></span>";
            }

            out << "</p>\n";
        } catch (const std::exception &ex) {
            out << "<p class='line err'>Ошибка разбора строки: "
                << html_escape(line) << "<br>"
                << html_escape(ex.what()) << "</p>\n";
        }
    }

    out << "</div></body></html>";
    return out.str();
}

std::string upload_page() {
    return R"HTML(<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>pretty_log</title>
  <style>
    body { font-family: sans-serif; padding: 24px; background: #111; color: #ddd; }
    .box { background: #1a1a1a; border: 1px solid #333; padding: 20px; border-radius: 10px; max-width: 700px; }
    input, button { font: inherit; }
  </style>
</head>
<body>
  <div class="box">
    <h1>pretty_log</h1>
    <form action="/upload" method="post" enctype="multipart/form-data">
      <p><input type="file" name="logfile" accept=".log,.txt,.json,*/*"></p>
      <p><button type="submit">Загрузить и показать</button></p>
    </form>
  </div>
</body>
</html>)HTML";
}
} // namespace

int main() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content(upload_page(), "text/html; charset=utf-8");
    });

    svr.Post("/upload", [](const httplib::Request &req, httplib::Response &res) {
        if (req.files.empty()) {
            res.status = 400;
            res.set_content("Файл не был загружен", "text/plain; charset=utf-8");
            return;
        }

        // Берём первый файл из формы
        const auto &file = req.files.begin()->second;
        auto html = render_log_file(file.content);
        res.set_content(html, "text/html; charset=utf-8");
    });

    std::cout << "Open http://127.0.0.1:8080\n";
    svr.listen("127.0.0.1", 8080);
    return 0;
}