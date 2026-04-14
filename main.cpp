#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <httplib.h>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

    std::string html_escape(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (char ch: s) {
            switch (ch) {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&#39;";
                    break;
                default:
                    out += ch;
                    break;
            }
        }
        return out;
    }

    std::string random_id() {
        static constexpr char hex[] = "0123456789abcdef";
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<int> dist(0, 15);

        std::string id;
        id.reserve(16);
        for (int i = 0; i < 16; ++i) {
            id.push_back(hex[dist(gen)]);
        }
        return id;
    }

    std::string color_for_priority(int p) {
        switch (p) {
            case 0:
            case 1:
            case 2:
                return "#ff5555";
            case 3:
                return "#ff4444";
            case 4:
                return "#ffaa00";
            case 5:
                return "#ffffff";
            case 6:
                return "#dddddd";
            case 7:
                return "#888888";
            default:
                return "#ffffff";
        }
    }

    std::string format_realtime_timestamp(std::string_view value) {
        if (value.empty())
            return {};

        auto const us = std::stoll(std::string(value));
        auto const sec = static_cast<std::time_t>(us / 1000000);
        auto const micros = static_cast<int>(us % 1000000);

        std::tm tm{};
        localtime_r(&sec, &tm);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(6) << std::setfill('0') << micros;
        return oss.str();
    }

    std::string render_log_file(std::string_view content) {
        std::ostringstream out;
        out << "<!doctype html><html><head><meta charset='utf-8'>"
            << "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            << "<title>LogView</title>"
            << "<style>"
            << "body{margin:0;background:#111;color:#ddd;font-family:'DejaVu Sans "
               "Mono',monospace;padding:24px;font-size:18px;}"
            << ".container{width:100%;max-width:none;margin:0;font-family:'DejaVu Sans Mono',monospace;}"
            << "h1{margin:0 0 16px;font-size:3rem;font-weight:700;color:#f2f2f2;font-family:'DejaVu Sans "
               "Mono',monospace;}"
            << "p{line-height:1.7;margin:0 0 16px;font-size:1rem;color:#ddd;font-family:'DejaVu Sans Mono',monospace;}"
            << ".line{white-space:pre-wrap;line-height:1.75;margin:0 0 10px;font-size:18px;font-family:'DejaVu Sans "
               "Mono',monospace;}"
            << ".ts{color:#888;font-family:'DejaVu Sans Mono',monospace;}"
            << ".host{color:#999;font-family:'DejaVu Sans Mono',monospace;}"
            << ".ident{color:#66d9ef;font-family:'DejaVu Sans Mono',monospace;}"
            << ".msg{white-space:pre-wrap;color:#e6e6e6;font-size:18px;font-family:'DejaVu Sans Mono',monospace;}"
            << ".err{color:#ff6b6b;font-family:'DejaVu Sans Mono',monospace;}"
            << ".box{width:100%;box-sizing:border-box;background:#1a1a1a;border:1px solid "
               "#333;padding:24px;border-radius:12px;font-family:'DejaVu Sans Mono',monospace;}"
            << ".buttons{margin:0 0 18px;display:flex;gap:12px;flex-wrap:wrap;font-family:'DejaVu Sans "
               "Mono',monospace;}"
            << ".btn{display:inline-flex;align-items:center;justify-content:center;padding:14px "
               "20px;border-radius:12px;text-decoration:none;font-family:'DejaVu Sans "
               "Mono',monospace;font-size:1.05rem;cursor:pointer;border:none;transition:transform .15s ease,box-shadow "
               ".15s ease,background .15s ease;}"
            << ".btn:hover{transform:translateY(-1px);}"
            << ".btn-secondary{background:#2a2a2a;color:#f2f2f2;border:1px solid #444;}"
            << ".btn-secondary:hover{box-shadow:0 8px 18px rgba(0,0,0,.25);}"
            << ".nav-fixed{position:fixed;right:16px;bottom:16px;display:flex;flex-direction:column;gap:10px;z-index:"
               "1000;}"
            << ".nav-btn{display:inline-flex;align-items:center;justify-content:center;width:54px;height:54px;border-"
               "radius:14px;text-decoration:none;font-family:'DejaVu Sans "
               "Mono',monospace;font-size:1.2rem;font-weight:700;border:1px solid "
               "#444;background:#2a2a2a;color:#f2f2f2;box-shadow:0 8px 18px rgba(0,0,0,.25);transition:transform .15s "
               "ease,box-shadow .15s ease,background .15s ease;}"
            << ".nav-btn:hover{transform:translateY(-1px);box-shadow:0 10px 22px rgba(0,0,0,.3);}"
            << "a{color:inherit;font-family:'DejaVu Sans Mono',monospace;}"
            << "</style></head><body>"
            << "<div class='container' id='top'>"
            << "<h1>LogView</h1>"
            << "<div class='buttons'>"
            << "<a class='btn btn-secondary' href='/'>← Загрузить другой файл</a>"
            << "</div>"
            << "<div class='box'>";

        std::istringstream input{std::string(content)};
        std::string line;

        while (std::getline(input, line)) {
            if (line.empty())
                continue;

            try {
                auto json = nlohmann::json::parse(line);

                auto ts = json.value("__REALTIME_TIMESTAMP", "");
                auto host = json.value("_HOSTNAME", "");
                auto ident = json.value("SYSLOG_IDENTIFIER", json.at("_COMM").get<std::string>());
                auto pid = json.value("_PID", "unknown");
                auto msg = json.value("MESSAGE", "");
                auto prio_str = json.value("PRIORITY", "6");

                int prio = 6;
                try {
                    prio = std::stoi(prio_str);
                } catch (...) {
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

                out << "<span class='ident'>" << html_escape(ident) << "[" << html_escape(pid) << "]</span>: ";

                std::istringstream msg_stream(msg);
                std::string msg_line;
                bool first = true;

                while (std::getline(msg_stream, msg_line)) {
                    if (first) {
                        out << "<span class='msg' style='color:" << color << ";'>" << html_escape(msg_line)
                            << "</span>";
                        first = false;
                    } else {
                        out << "<br><span class='msg' style='color:" << color << ";'>&nbsp;&nbsp;&nbsp;&nbsp;"
                            << html_escape(::fmt::format("\t\t\t\t\t{}", msg_line)) << "</span>";
                    }
                }

                if (msg.empty()) {
                    out << "<span class='msg' style='color:" << color << ";'></span>";
                }

                out << "</p>\n";
            } catch (const std::exception &ex) {
                out << "<p class='line err'>Ошибка разбора строки: " << html_escape(line) << "<br>"
                    << html_escape(ex.what()) << "</p>\n";
            }
        }

        out << "</div></div>"
            << "<div class='nav-fixed'>"
            << "<a class='nav-btn' href='#top' title='Наверх'>↑</a>"
            << "<a class='nav-btn' href='#bottom' title='Вниз'>↓</a>"
            << "</div>"
            << "<div id='bottom'></div>"
            << "</body></html>";
        return out.str();
    }

    std::string upload_page() {
        return R"HTML(<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LogView</title>
  <style>
    body {
      margin: 0;
      font-family: Cambria, serif;
      background: #ffffff;
      color: #222;
      padding: 48px 24px;
      font-size: 20px;
    }

    .container {
      max-width: 900px;
      margin: 0 auto;
    }

    h1 {
      margin: 0 0 18px;
      font-size: 3.2rem;
      font-weight: 700;
    }

    p {
      line-height: 1.7;
      margin: 0 0 16px;
      font-size: 1.15rem;
    }

    .section {
      margin-top: 28px;
      padding: 24px;
      border: 1px solid #e5e5e5;
      border-radius: 14px;
      background: #fafafa;
    }

    pre {
      margin: 12px 0 0;
      padding: 16px;
      background: #f4f4f4;
      border-radius: 10px;
      overflow: auto;
      font-family: Consolas, "Liberation Mono", monospace;
      font-size: 1.05rem;
      line-height: 1.5;
    }

    .file-row {
      margin-top: 14px;
      padding: 18px;
      border: 1px dashed #cfcfcf;
      border-radius: 12px;
      background: #fff;
    }

    input[type="file"] {
      font: inherit;
      width: 100%;
      font-size: 1.05rem;
    }

    .buttons {
      margin-top: 18px;
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
    }

    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 14px 20px;
      border-radius: 12px;
      text-decoration: none;
      font: inherit;
      font-size: 1.05rem;
      cursor: pointer;
      border: none;
      transition: transform 0.15s ease, box-shadow 0.15s ease, background 0.15s ease;
    }

    .btn:hover {
      transform: translateY(-1px);
    }

    .btn-primary {
      background: linear-gradient(180deg, #5c8df6, #3f6fe0);
      color: #fff;
      box-shadow: 0 8px 18px rgba(63, 111, 224, 0.25);
    }

    .btn-primary:hover {
      box-shadow: 0 10px 22px rgba(63, 111, 224, 0.32);
    }

    .muted {
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>LogView</h1>
    <p>Загрузите файл с логами и получите удобную ссылку для просмотра.</p>

    <div class="section">
      <form action="/upload" method="post" enctype="multipart/form-data">
        <div class="file-row">
          <input type="file" name="logfile" accept=".log,.txt,.json,*/*" required>
        </div>

        <div class="buttons">
          <button class="btn btn-primary" type="submit">Загрузить и получить ссылку</button>
        </div>
      </form>
    </div>

    <div class="section">
      <p><strong>Пример, как получить лог:</strong></p>
      <pre>journalctl --since="2026-04-13 23:50:53" --until="2026-04-13 23:54" --output=json</pre>
      <p class="muted">Сохраните вывод в файл и загрузите его сюда. Потом можно открыть ссылку ещё раз или отправить её другому человеку.</p>
    </div>
  </div>
</body>
</html>)HTML";
    }

} // namespace

int main() {
    fs::create_directories("storage");

    httplib::Server svr;

    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content(upload_page(), "text/html; charset=utf-8");
    });

    svr.Post("/upload", [](const httplib::Request &req, httplib::Response &res) {
        if (req.files.empty()) {
            res.status = 400;
            res.set_content("Файл не загружен", "text/plain; charset=utf-8");
            return;
        }

        const auto &file = req.files.begin()->second;
        std::string id = random_id();
        fs::path path = fs::path("storage") / id;

        {
            std::ofstream ofs(path, std::ios::binary);
            ofs << file.content;
        }

        std::ostringstream html;
        html << "<!doctype html><html><head><meta charset='utf-8'>"
             << "<meta name='viewport' content='width=device-width, initial-scale=1'>"
             << "<title>LogView</title>"
             << "<style>"
             << "body{margin:0;font-family:Cambria,serif;background:#fff;color:#222;padding:48px 24px;font-size:20px;}"
             << ".container{max-width:900px;margin:0 auto;}"
             << "p{line-height:1.7;margin:0 0 16px;font-size:1.15rem;}"
             << ".section{margin-top:28px;padding:24px;border:1px solid #e5e5e5;border-radius:14px;background:#fafafa;}"
             << ".btn{display:inline-flex;align-items:center;justify-content:center;padding:14px "
                "20px;border-radius:12px;text-decoration:none;font:inherit;font-size:1.05rem;cursor:pointer;border:"
                "none;transition:transform .15s ease,box-shadow .15s ease,background .15s ease;}"
             << ".btn:hover{transform:translateY(-1px);}"
             << ".btn-primary{background:linear-gradient(180deg,#5c8df6,#3f6fe0);color:#fff;box-shadow:0 8px 18px "
                "rgba(63,111,224,.25);}"
             << ".btn-primary:hover{box-shadow:0 10px 22px rgba(63,111,224,.32);}"
             << ".btn-secondary{background:#f3f3f3;color:#222;border:1px solid #d7d7d7;}"
             << "a{color:inherit;}"
             << "</style></head><body>"
             << "<div class='container'>"
             << "<div class='section'>"
             << "<p>Файл сохранён.</p>"
             << "<p>Ссылка: <a href='/view/" << id << "'>/view/" << id << "</a></p>"
             << "<p><a class='btn btn-secondary' href='/'>Загрузить ещё</a></p>"
             << "</div>"
             << "</div>"
             << "</body></html>";

        res.set_content(html.str(), "text/html; charset=utf-8");
    });

    svr.Get(R"(/view/([0-9a-fA-F]+))", [](const httplib::Request &req, httplib::Response &res) {
        std::string id = req.matches[1];
        fs::path path = fs::path("storage") / id;

        if (!fs::exists(path)) {
            res.status = 404;
            res.set_content("Файл не найден", "text/plain; charset=utf-8");
            return;
        }

        std::ifstream ifs(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << ifs.rdbuf();

        res.set_content(render_log_file(buffer.str()), "text/html; charset=utf-8");
    });

    std::cout << "Open http://127.0.0.1:8080\n";
    svr.listen("127.0.0.1", 8080);
    return 0;
}
