#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <winsock2.h>
#include <windows.h>
#include <ctime>

#include "server_ui.h"
#include "xml_database.h"

#pragma comment(lib, "ws2_32.lib")

#define IDI_ICON1 101
#define _CRT_SECURE_NO_WARNINGS

// ─── Globals ────────────────────────────────────────────────────────────────
HWND hwndPopup = NULL;
HWND hReqLight = NULL;
HWND hResLight = NULL;
bool reqActive = false;
bool resActive = false;
NOTIFYICONDATA nid = {0};
bool running = true;
SOCKET server_fd;

// ════════════════════════════════════════════════════════════════════════════
//  STATUS LOGIC
//  Stored in XML   : Incomplete | Complete | Ignore
//  Derived at render: Upcoming | Active | Started | Expired
//                     (+ Complete / Ignore passthrough)
//
//  With end_dt:
//    now  < start_dt             → Upcoming
//    start_dt ≤ now ≤ end_dt     → Active
//    now  > end_dt               → Expired   [outdated page]
//
//  Without end_dt:
//    now  < start_dt             → Upcoming
//    now ≥ start_dt              → Started
//
//  Complete → normal page   |   Ignore → outdated page
// ════════════════════════════════════════════════════════════════════════════

struct TaskStatus {
    std::string display;  // badge label
    bool        outdated; // true = goes to outdated.html
};

TaskStatus deriveStatus(const std::string& stored,
                        const std::string& start_dt,
                        const std::string& end_dt,
                        const std::string& nowStr)
{
    if (stored == "Complete") {
    // end_dt past වෙලා complete කරලා තිබුණොත් = LateComplete
    if (!end_dt.empty() && nowStr > end_dt)
        return {"LateComplete", true};   // outdated page එකට යවන්න
    return {"Complete", false};
}
    if (stored == "Ignore")   return {"Ignore",   true };

    // stored == "Incomplete" — derive from time fields
    if (start_dt.empty()) return {"Incomplete", false};

    bool hasEnd  = !end_dt.empty();
    bool started = (nowStr >= start_dt);
    bool ended   = hasEnd && (nowStr > end_dt);

    if (!started)         return {"Upcoming", false};
    if (hasEnd && !ended) return {"Active",   false};
    if (hasEnd &&  ended) return {"Expired",  true };
    return {"Started", false};  // no end_dt, already started
}

// ════════════════════════════════════════════════════════════════════════════
//  UTILITY
// ════════════════════════════════════════════════════════════════════════════

std::string htmlEscape(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  o += "&amp;";  break;
            case '<':  o += "&lt;";   break;
            case '>':  o += "&gt;";   break;
            case '"':  o += "&quot;"; break;
            case '\'': o += "&#39;";  break;
            default:   o += c;
        }
    }
    return o;
}

std::string jsonEscape(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else                o += c;
    }
    return o;
}

std::string urlDecode(const std::string& str) {
    std::string res;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int v; std::stringstream ss;
            ss << std::hex << str.substr(i + 1, 2); ss >> v;
            res += (char)v; i += 2;
        } else if (str[i] == '+') { res += ' ';
        } else { res += str[i]; }
    }
    return res;
}

std::map<std::string, std::string> parseQuery(const std::string& rawPath) {
    std::map<std::string, std::string> p;
    size_t pos = rawPath.find('?');
    if (pos == std::string::npos) return p;
    std::string q = rawPath.substr(pos + 1);
    std::stringstream ss(q); std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
            p[pair.substr(0, eq)] = urlDecode(pair.substr(eq + 1));
    }
    return p;
}

std::string getField(const std::vector<XMLField>& fields, const std::string& name) {
    for (const auto& f : fields) if (f.name == name) return f.value;
    return "";
}

std::string contentTypeFor(const std::string& p) {
    if (p.find(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (p.find(".css")  != std::string::npos) return "text/css";
    if (p.find(".js")   != std::string::npos) return "application/javascript";
    if (p.find(".png")  != std::string::npos) return "image/png";
    if (p.find(".jpg")  != std::string::npos ||
        p.find(".jpeg") != std::string::npos)  return "image/jpeg";
    if (p.find(".xml")  != std::string::npos) return "application/xml";
    if (p.find(".json") != std::string::npos) return "application/json";
    if (p.find(".ico")  != std::string::npos) return "image/x-icon";
    return "application/octet-stream";
}

std::string getRefererPage(const std::string& request) {
    size_t pos = request.find("Referer: http://localhost:8080/");
    if (pos == std::string::npos) return "program.html";
    pos += 30;
    size_t end = request.find("\r\n", pos);
    std::string page = request.substr(pos, end - pos);
    size_t q = page.find('?');
    if (q != std::string::npos) page = page.substr(0, q);
    return page.empty() ? "app.html" : page;
}

std::string nowString() {
    time_t now = time(0); char buf[25]; struct tm ti;
    localtime_s(&ti, &now);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M", &ti);
    return std::string(buf);
}

// Short display: "MM-DD HH:MM"
std::string shortDt(const std::string& dt) {
    // dt format: "YYYY-MM-DDTHH:MM"
    if (dt.size() < 16) return dt;

    int year  = std::stoi(dt.substr(0, 4));
    int month = std::stoi(dt.substr(5, 2));
    int day   = std::stoi(dt.substr(8, 2));
    std::string timeStr = dt.substr(11, 5); // "HH:MM"

    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                             "Jul","Aug","Sep","Oct","Nov","Dec"};
    std::string mon = (month >= 1 && month <= 12) ? months[month-1] : "???";

    char buf[32];
    sprintf(buf, "%s %02d, %d  %s", mon.c_str(), day, year, timeStr.c_str());
    return std::string(buf);
}

// ════════════════════════════════════════════════════════════════════════════
//  HTTP HELPERS
// ════════════════════════════════════════════════════════════════════════════

void sendResponse(SOCKET sock, int code, const std::string& status,
                  const std::string& ct, const std::string& body)
{
    std::string h = "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n"
                    "Content-Type: "   + ct   + "\r\n"
                    "Content-Length: " + std::to_string(body.length()) + "\r\n"
                    "Connection: close\r\n\r\n";
    send(sock, h.c_str(),    (int)h.length(),    0);
    send(sock, body.c_str(), (int)body.length(), 0);
}

void sendRedirect(SOCKET sock, const std::string& loc) {
    std::string r = "HTTP/1.1 302 Found\r\nLocation: " + loc + "\r\n\r\n";
    send(sock, r.c_str(), (int)r.length(), 0);
}

// ════════════════════════════════════════════════════════════════════════════
//  TASK CARD BUILDER
// ════════════════════════════════════════════════════════════════════════════

// pageCtx: 0 = program.html, 1 = outdated (expired), 2 = archive (ignore)
std::string buildTaskCard(const XMLRow& row,
                          const std::string& nowStr,
                          int pageCtx = 0)
{
    std::string title    = htmlEscape(getField(row.fields, "title"));
    std::string type     = htmlEscape(getField(row.fields, "type"));
    std::string desc     = htmlEscape(getField(row.fields, "desc"));
    std::string start_dt = getField(row.fields, "start_dt");
    std::string end_dt   = getField(row.fields, "end_dt");

    auto ts = deriveStatus(getField(row.fields, "status"), start_dt, end_dt, nowStr);

    std::string badgeClass = "badge-incomplete";
    if      (ts.display == "Active")   badgeClass = "badge-active";
    else if (ts.display == "Upcoming") badgeClass = "badge-upcoming";
    else if (ts.display == "Started")  badgeClass = "badge-started";
    else if (ts.display == "Complete") badgeClass = "badge-complete";
    else if (ts.display == "Expired")  badgeClass = "badge-expired";
    else if (ts.display == "Ignore")   badgeClass = "badge-ignore";

    std::string dateDisplay = shortDt(start_dt);
    if (!end_dt.empty()) dateDisplay += " → " + shortDt(end_dt);

    // ── Action buttons — all use JS doAction() so redirect is HTML-side ──
    std::string actions;
    if (pageCtx == 0) {
        // program.html: Complete + Ignore only for non-Complete tasks
        if (ts.display != "Complete") {
            actions +=
                "<button onclick=\"doAction('/update?id=" + row.pk_value + "&s=Complete','/program.html?tab=Incomplete')\" "
                    "class='act-btn act-done' title='Mark Complete'>✔</button>"
                "<button onclick=\"doAction('/update?id=" + row.pk_value + "&s=Ignore','/archive')\" "
                    "class='act-btn act-ignore' title='Move to Archive'>✖</button>";
        } else {
            // Complete task — offer restore only
            actions +=
                "<button onclick=\"doAction('/update?id=" + row.pk_value + "&s=Incomplete','/program.html')\" "
                    "class='act-btn act-restore' title='Restore to Incomplete'>↩</button>";
        }
        actions +=
            "<button onclick=\"if(confirm('Delete this task?')) doAction('/delete?id=" + row.pk_value + "','/program.html')\" "
                "class='act-btn act-delete' title='Delete'>🗑</button>";
				
		} else if (pageCtx == 1) {
			if (ts.display == "Expired") {
				// expired incomplete — complete කරන්න පුළුවන්, restore නෑ
				actions =
					"<button onclick=\"doAction('/update?id=" + row.pk_value + "&s=Complete','/outdated')\" "
						"class='act-btn act-done' title='Mark Complete (Late)'>✔</button>";
			} else if (ts.display == "LateComplete") {
				// late complete — කිසිම restore/complete නෑ
				actions = "";
			} else {
				// Complete / Incomplete — restore only
				actions = "";
			}
			actions +=
				"<button onclick=\"if(confirm('Permanently delete this task?')) doAction('/delete?id=" + row.pk_value + "','/outdated')\" "
					"class='act-btn act-delete' title='Delete permanently'>🗑</button>";
		} else {
        // archive (pageCtx == 2 — Ignore): restore → program, delete stays on archive
        actions =
            "<button onclick=\"doAction('/update?id=" + row.pk_value + "&s=Incomplete','/program.html')\" "
                "class='act-btn act-restore' title='Restore to Tasks'>↩</button>"
            "<button onclick=\"if(confirm('Permanently delete this task?')) doAction('/delete?id=" + row.pk_value + "','/archive')\" "
                "class='act-btn act-delete' title='Delete permanently'>🗑</button>";
    }

    std::stringstream html;
    html << "<div class='task-item " << ts.display
         << "' data-status='" << ts.display << "'>"
         << "<div class='task-header'>"
         << "  <span class='task-arrow'>›</span>"
         << "  <div class='task-title'>" << title << "</div>"
         << "  <div class='task-type'>"  << type  << "</div>"
         << "  <span class='badge " << badgeClass << "'>" << ts.display << "</span>"
         << "  <div class='task-actions'>" << actions << "</div>"
         << "  <div class='task-date'>" << dateDisplay << "</div>"
         << "</div>"
         << "<div class='task-details'><div class='task-details-inner'>"
         << "  <div class='detail-meta'>"
         << "    <span class='detail-meta-item'>"
         << "      <span class='k'>START</span>"
         << "      <span class='v'>" << (start_dt.empty() ? "—" : shortDt(start_dt)) << "</span>"
         << "    </span>";

    if (!end_dt.empty()) {
        html << "    <span class='detail-meta-item'>"
             << "      <span class='k'>END</span>"
             << "      <span class='v'>" << shortDt(end_dt) << "</span>"
             << "    </span>";
    }

    html << "    <span class='detail-meta-item'>"
         << "      <span class='k'>ID</span>"
         << "      <span class='v'>" << row.pk_value << "</span>"
         << "    </span>"
         << "  </div>";

    if (desc.empty())
        html << "<div class='desc-empty'>No description provided.</div>";
    else
        html << "<div class='desc-content'>" << desc << "</div>";

    html << "</div></div></div>";
    return html.str();
}

// ════════════════════════════════════════════════════════════════════════════
//  PAGE + JSON BUILDERS
// ════════════════════════════════════════════════════════════════════════════

std::string buildMainCards(const std::vector<XMLRow>& rows, const std::string& nowStr) {
    std::stringstream html; int count = 0;
    for (const auto& row : rows) {
        auto ts = deriveStatus(getField(row.fields, "status"),
                               getField(row.fields, "start_dt"),
                               getField(row.fields, "end_dt"), nowStr);
        if (ts.outdated) continue;
        html << buildTaskCard(row, nowStr, 0); count++;
    }
    if (count == 0)
        html << "<div style='text-align:center;padding:80px 20px;"
                "color:#3a4060;font-family:monospace;'>"
                "<div style='font-size:36px;margin-bottom:16px;'>◈</div>"
                "<p>No active tasks. Press <strong style='color:#00d2b4;'>"
                "+ NEW TASK</strong> to get started.</p></div>";
    return html.str();
}

std::string buildOutdatedCards(const std::vector<XMLRow>& rows, const std::string& nowStr) {
    std::stringstream html; int count = 0;
    for (const auto& row : rows) {
        auto ts = deriveStatus(getField(row.fields, "status"),
                               getField(row.fields, "start_dt"),
                               getField(row.fields, "end_dt"), nowStr);
        // outdated page = Expired + Complete + Incomplete
        if (ts.display != "Expired" && ts.display != "Complete" && ts.display != "LateComplete") continue;
        html << buildTaskCard(row, nowStr, 1); count++;
    }
    if (count == 0)
        html << "<div style='text-align:center;padding:80px 20px;"
                "color:#3a4060;font-family:monospace;'>"
                "<div style='font-size:36px;margin-bottom:16px;'>◈</div>"
                "<p>No expired tasks.</p></div>";
    return html.str();
}

std::string buildArchiveCards(const std::vector<XMLRow>& rows, const std::string& nowStr) {
    std::stringstream html; int count = 0;
    for (const auto& row : rows) {
        auto ts = deriveStatus(getField(row.fields, "status"),
                               getField(row.fields, "start_dt"),
                               getField(row.fields, "end_dt"), nowStr);
        if (ts.display != "Ignore") continue;    // archive page = Ignored only
        html << buildTaskCard(row, nowStr, 2); count++;
    }
    if (count == 0)
        html << "<div style='text-align:center;padding:80px 20px;"
                "color:#3a4060;font-family:monospace;'>"
                "<div style='font-size:36px;margin-bottom:16px;'>◈</div>"
                "<p>Archive is empty.</p></div>";
    return html.str();
}

std::string buildStatsJson(const std::vector<XMLRow>& rows, const std::string& nowStr) {
    int total=0, active=0, upcoming=0, started=0,
        complete=0, expired=0, incomplete=0, ignore=0;
    std::stringstream taskList;
    taskList << "\"tasks\":["; bool first = true;

    for (const auto& row : rows) {
        total++;
        auto ts = deriveStatus(getField(row.fields, "status"),
                               getField(row.fields, "start_dt"),
                               getField(row.fields, "end_dt"), nowStr);
        const std::string& d = ts.display;
        if      (d=="Active")     active++;
        else if (d=="Upcoming")   upcoming++;
        else if (d=="Started")    started++;
        else if (d=="Complete")   complete++;
        else if (d=="Expired")    expired++;
        else if (d=="Ignore")     ignore++;
        else                      incomplete++;

        if (!ts.outdated) {
            if (!first) taskList << ","; first = false;
            taskList << "{\"title\":\""     << jsonEscape(getField(row.fields,"title"))
                     << "\",\"status\":\""   << jsonEscape(ts.display)
                     << "\",\"start_dt\":\"" << jsonEscape(getField(row.fields,"start_dt"))
                     << "\"}";
        }
    }
    taskList << "]";

    std::stringstream j;
    j << "{"
      << "\"total\":"      << total      << ","
      << "\"active\":"     << active     << ","
      << "\"upcoming\":"   << upcoming   << ","
      << "\"started\":"    << started    << ","
      << "\"complete\":"   << complete   << ","
      << "\"expired\":"    << expired    << ","
      << "\"incomplete\":" << incomplete << ","
      << "\"ignore\":"     << ignore     << ","
      << taskList.str() << "}";
    return j.str();
}

// ════════════════════════════════════════════════════════════════════════════
//  SERVER THREAD
// ════════════════════════════════════════════════════════════════════════════

DWORD WINAPI runServer(LPVOID lpParam) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        AddLog("WSAStartup failed"); return 1;
    }

    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) { AddLog("socket() failed"); WSACleanup(); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    server.sin_family = AF_INET; server.sin_addr.s_addr = INADDR_ANY; server.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        AddLog("bind() failed - port 8080 in use?"); closesocket(server_fd); WSACleanup(); return 1;
    }
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        AddLog("listen() failed"); closesocket(server_fd); WSACleanup(); return 1;
    }
    AddLog("Server listening on http://localhost:8080");

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of("\\/"));
    std::string dbPath = exeDir + "\\container\\master_db.xml";

    XMLDatabase db(dbPath);
    db.createTable("Tasks", "tid");
    AddLog(("DB path: " + dbPath).c_str());


    while (running) {
        SOCKET sock = accept(server_fd, (struct sockaddr*)&client, &c);
        if (sock == INVALID_SOCKET) continue;

        char buffer[8192] = {0};
        int valread = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (valread <= 0) { closesocket(sock); continue; }

        std::string request(buffer);
        size_t rStart = request.find("GET /");
        if (rStart == std::string::npos) { closesocket(sock); continue; }

        rStart += 5;
        size_t rEnd = request.find(" ", rStart);
        std::string rawPath = request.substr(rStart, rEnd - rStart);

        auto params = parseQuery(rawPath);
        size_t qPos = rawPath.find('?');
        std::string fileName = urlDecode(qPos != std::string::npos
                                         ? rawPath.substr(0, qPos) : rawPath);
        if (fileName.empty() || fileName == "/") fileName = "app.html";

        const std::string nowStr = nowString();

        // ══════════════════════════════════════════════════════════════════
        //  /update  — change stored status of one task
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "update") {
            if (params.count("id") && params.count("s")) {
                const std::string& id = params["id"];
                const std::string& ns = params["s"];
                if (ns == "Complete" || ns == "Ignore" || ns == "Incomplete") {
                    auto all = db.selectAll("Tasks");
                    for (const auto& row : all) {
                        if (row.pk_value != id) continue;
                        std::vector<XMLField> upd;
                        for (const auto& f : row.fields)
                            { XMLField xf; xf.name = f.name; xf.value = (f.name == "status") ? ns : f.value; upd.push_back(xf); }
                        db.update("Tasks", id, upd);
                        AddLog(("Updated: " + id + " → " + ns).c_str());
                        break;
                    }
                }
            }
            sendResponse(sock, 200, "OK", "application/json", "{\"ok\":true}");
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  /delete  — remove one task permanently
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "delete") {
            if (params.count("id")) {
                const std::string& id = params["id"];
                if (db.deleteRow("Tasks", id))
                    AddLog(("Deleted: " + id).c_str());
                else
                    AddLog(("Delete not found: " + id).c_str());
            }
            sendResponse(sock, 200, "OK", "application/json", "{\"ok\":true}");
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  /clear  — delete all tasks EXCEPT Expired ones
        //  "Expired" is derived, so we check deriveStatus, not stored value.
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "clear") {
            auto all = db.selectAll("Tasks"); int removed = 0;
            for (const auto& row : all) {
                auto ts = deriveStatus(getField(row.fields, "status"),
                                       getField(row.fields, "start_dt"),
                                       getField(row.fields, "end_dt"), nowStr);
                if (ts.display != "Expired") {
                    db.deleteRow("Tasks", row.pk_value); removed++;
                }
            }
            AddLog(("Clear all: removed " + std::to_string(removed) + " task(s)").c_str());
            sendRedirect(sock, "/program.html");
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  /clear-archive  — delete only Ignore tasks from outdated page
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "clear-archive") {
            int n = db.deleteWhere("Tasks", "status", {"Ignore"});
            AddLog(("Clear archive: removed " + std::to_string(n) + " task(s)").c_str());
            sendRedirect(sock, "/archive");
            closesocket(sock); continue;
        }
		
		// ══════════════════════════════════════════════════════════════════
        //  /clear-outdate  — delete only Ignore tasks from outdated page
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "clear-outdated") {
            auto all = db.selectAll("Tasks"); int removed = 0;
            for (const auto& row : all) {
                auto ts = deriveStatus(getField(row.fields, "status"),
                                       getField(row.fields, "start_dt"),
                                       getField(row.fields, "end_dt"), nowStr);
                if (ts.display != "Ignore") {
                    db.deleteRow("Tasks", row.pk_value); removed++;
                }
            }
            AddLog(("Clear Expired: removed " + std::to_string(removed) + " task(s)").c_str());
            sendRedirect(sock, "/outdated");
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  /stats  — JSON for app.html dashboard
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "stats") {
            auto rows = db.selectAll("Tasks");
            sendResponse(sock, 200, "OK", "application/json",
                         buildStatsJson(rows, nowStr));
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  /outdated  — archive page served from outdated.html template
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "outdated") {
            std::ifstream tf("container/outdated.html", std::ios::binary);
            if (!tf.is_open()) {
                sendResponse(sock, 404, "Not Found", "text/html",
                             "<h2>outdated.html not found in container/</h2>");
                closesocket(sock); continue;
            }
            std::stringstream ss; ss << tf.rdbuf();
            auto rows = db.selectAll("Tasks");
            std::string page = ss.str();
            std::string cards = buildOutdatedCards(rows, nowStr);
            size_t ph = page.find("{{CONTENT}}");
            if (ph != std::string::npos) page.replace(ph, 11, cards);
            sendResponse(sock, 200, "OK", "text/html; charset=utf-8", page);
            AddLog("Served: outdated (expired)");
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  /archive  — ignored tasks page served from archive.html template
        // ══════════════════════════════════════════════════════════════════
        if (fileName == "archive") {
            std::ifstream tf("container/archive.html", std::ios::binary);
            if (!tf.is_open()) {
                sendResponse(sock, 404, "Not Found", "text/html",
                             "<h2>archive.html not found in container/</h2>");
                closesocket(sock); continue;
            }
            std::stringstream ss; ss << tf.rdbuf();
            auto rows = db.selectAll("Tasks");
            std::string page = ss.str();
            std::string cards = buildArchiveCards(rows, nowStr);
            size_t ph = page.find("{{CONTENT}}");
            if (ph != std::string::npos) page.replace(ph, 11, cards);
            sendResponse(sock, 200, "OK", "text/html; charset=utf-8", page);
            AddLog("Served: archive (ignored)");
            closesocket(sock); continue;
        }

        // ══════════════════════════════════════════════════════════════════
        //  Static file serving
        // ══════════════════════════════════════════════════════════════════
        std::string fullPath = "container/" + fileName;
        std::ifstream file(fullPath, std::ios::binary);

        if (!file.is_open()) {
            std::string err =
                "<html><body style='background:#080a0f;color:white;"
                "font-family:monospace;padding:40px;'>"
                "<h2 style='color:#ff3c5a;'>404 — Not Found</h2>"
                "<p style='color:#8890aa;'>/" + htmlEscape(fileName) + "</p>"
                "<a href='/app.html' style='color:#00d2b4;'>← Dashboard</a>"
                "</body></html>";
            sendResponse(sock, 404, "Not Found", "text/html; charset=utf-8", err);
            closesocket(sock); continue;
        }

        reqActive = true;
        AddLog(("Served: " + fileName).c_str());

        std::stringstream fs; fs << file.rdbuf();
        std::string content = fs.str();
        file.close();

        std::string dataContent;

        // ── program.html — insert task + inject cards ─────────────────────
        if (fileName == "program.html") {
            if (params.count("title") && !params["title"].empty() &&
                params.count("sid")   && !params["sid"].empty()) {
                std::vector<XMLField> f;
                { XMLField xf; xf.name="title"; xf.value=params["title"]; f.push_back(xf); }
                { XMLField xf; xf.name="type"; xf.value=params.count("type")?params["type"]:""; f.push_back(xf); }
                { XMLField xf; xf.name="desc"; xf.value=params.count("desc")?params["desc"]:""; f.push_back(xf); }
                { XMLField xf; xf.name="start_dt"; xf.value=params.count("start_dt")?params["start_dt"]:""; f.push_back(xf); }
                { XMLField xf; xf.name="end_dt"; xf.value=params.count("end_dt")?params["end_dt"]:""; f.push_back(xf); }
                { XMLField xf; xf.name="status"; xf.value="Incomplete"; f.push_back(xf); }
                db.insert("Tasks", params["sid"], f);
                AddLog(("New task: " + params["title"]).c_str());
            }
            auto rows = db.selectAll("Tasks");
            dataContent = buildMainCards(rows, nowStr);
        }

        size_t ph = content.find("{{CONTENT}}");
        if (ph != std::string::npos) content.replace(ph, 11, dataContent);

        sendResponse(sock, 200, "OK", contentTypeFor(fileName), content);

        resActive = true;
        if (hwndPopup) InvalidateRect(hwndPopup, NULL, TRUE);
        Sleep(50);
        reqActive = false; resActive = false;
        if (hwndPopup) InvalidateRect(hwndPopup, NULL, TRUE);

        closesocket(sock);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}