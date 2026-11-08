#include "ui/Markdown.h"

#include "core/Util.h"
#include "ui/Fonts.h"
#include "ui/OreTheme.h"

#include <imgui.h>

#include <regex>
#include <sstream>

namespace modmenu::ui::markdown {

namespace {

using ore::s;
namespace col = ore::color;

std::string trim(std::string const& v) {
    size_t a = v.find_first_not_of(" \t\r");
    if (a == std::string::npos) return {};
    size_t b = v.find_last_not_of(" \t\r");
    return v.substr(a, b - a + 1);
}

std::string replaceAll(std::string text, std::regex const& re, std::string const& with) {
    return std::regex_replace(text, re, with);
}

std::string decodeEntities(std::string t) {
    static std::pair<char const*, char const*> const kEntities[] = {
        {"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&copy;", "©"},
    };
    for (auto const& [from, to] : kEntities) {
        size_t pos = 0;
        while ((pos = t.find(from, pos)) != std::string::npos) {
            t.replace(pos, strlen(from), to);
            pos += strlen(to);
        }
    }
    return t;
}

// Turns the HTML that READMEs commonly use into markdown-ish lines.
std::string flattenHtml(std::string t) {
    static std::regex const comments(R"(<!--[\s\S]*?-->)");
    static std::regex const heading(R"(<h([1-6])[^>]*>([\s\S]*?)</h\1>)", std::regex::icase);
    static std::regex const anchor(R"re(<a\s[^>]*href\s*=\s*"([^"]*)"[^>]*>([\s\S]*?)</a>)re", std::regex::icase);
    static std::regex const img(R"(<img[^>]*>)", std::regex::icase);
    static std::regex const br(R"(<br\s*/?>)", std::regex::icase);
    static std::regex const block(R"(</?(p|div|details|summary|table|tr|picture|center)[^>]*>)", std::regex::icase);
    static std::regex const tag(R"(</?[a-zA-Z][^>]*>)");
    t = replaceAll(t, comments, "");
    t = replaceAll(t, img, "");
    // Heading tags may wrap anchors; flatten anchors first.
    t = replaceAll(t, anchor, "[$2]($1)");
    std::string out;
    std::sregex_iterator it(t.begin(), t.end(), heading), end;
    size_t last = 0;
    for (; it != end; ++it) {
        out += t.substr(last, size_t(it->position()) - last);
        int level = std::stoi((*it)[1]);
        out += "\n" + std::string(size_t(level), '#') + " " + trim((*it)[2].str()) + "\n";
        last = size_t(it->position() + it->length());
    }
    out += t.substr(last);
    out = replaceAll(out, br, "\n");
    out = replaceAll(out, block, "\n");
    out = replaceAll(out, tag, "");
    return decodeEntities(out);
}

// Inline markup -> plain text; collects links.
std::string inlineText(std::string t, Document& doc) {
    static std::regex const image(R"(!\[[^\]]*\]\([^)]*\))");
    static std::regex const imageRef(R"(!\[[^\]]*\]\[[^\]]*\])");
    static std::regex const link(R"(\[([^\]]*)\]\(([^)\s]*)[^)]*\))");
    static std::regex const strong(R"((\*\*|__)(.+?)\1)");
    static std::regex const em(R"((^|[\s(])[*_]([^*_\s][^*_]*?)[*_](?=[\s).,!?:;]|$))");
    static std::regex const code("`([^`]*)`");
    t = replaceAll(t, image, "");
    t = replaceAll(t, imageRef, "");
    std::string out;
    std::sregex_iterator it(t.begin(), t.end(), link), end;
    size_t last = 0;
    for (; it != end; ++it) {
        out += t.substr(last, size_t(it->position()) - last);
        std::string text = trim((*it)[1].str());
        std::string url  = (*it)[2].str();
        if (text.empty()) text = url;
        auto lower = toLower(url);
        if (lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0) doc.links.emplace_back(text, url);
        out += text;
        last = size_t(it->position() + it->length());
    }
    out += t.substr(last);
    out = replaceAll(out, strong, "$2");
    out = replaceAll(out, em, "$1$2");
    out = replaceAll(out, code, "$1");
    return trim(out);
}

bool isRule(std::string const& line) {
    std::string t = trim(line);
    if (t.size() < 3) return false;
    char c = t[0];
    if (c != '-' && c != '*' && c != '_') return false;
    for (char x : t)
        if (x != c && x != ' ') return false;
    return true;
}

bool isTableSeparator(std::string const& line) {
    std::string t = trim(line);
    if (t.empty() || t.find('-') == std::string::npos) return false;
    for (char x : t)
        if (x != '|' && x != '-' && x != ':' && x != ' ') return false;
    return true;
}

} // namespace

std::shared_ptr<Document const> parse(std::string const& markdown) {
    auto doc = std::make_shared<Document>();
    std::istringstream in(flattenHtml(markdown));
    std::string        paragraph;
    bool               inCode = false;
    std::string        code;

    auto flush = [&] {
        auto text = inlineText(paragraph, *doc);
        if (!text.empty()) doc->blocks.push_back({Block::Kind::Paragraph, 0, text, {}});
        paragraph.clear();
    };

    for (std::string raw; std::getline(in, raw);) {
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::string line = trim(raw);
        if (line.rfind("```", 0) == 0 || line.rfind("~~~", 0) == 0) {
            if (inCode) {
                if (!code.empty() && code.back() == '\n') code.pop_back();
                doc->blocks.push_back({Block::Kind::Code, 0, code, {}});
                code.clear();
            } else {
                flush();
            }
            inCode = !inCode;
            continue;
        }
        if (inCode) {
            code += raw + "\n";
            continue;
        }
        if (line.empty()) {
            flush();
            continue;
        }
        if (line[0] == '#') {
            flush();
            size_t level = line.find_first_not_of('#');
            auto   text  = inlineText(level == std::string::npos ? "" : line.substr(level), *doc);
            if (!text.empty()) doc->blocks.push_back({Block::Kind::Heading, int(std::min<size_t>(level, 6)), text, {}});
            continue;
        }
        if (isRule(line)) {
            flush();
            doc->blocks.push_back({Block::Kind::Rule, 0, {}, {}});
            continue;
        }
        if (line[0] == '|') {
            flush();
            if (isTableSeparator(line)) continue;
            Block row{Block::Kind::TableRow, 0, {}, {}};
            std::string cell;
            for (size_t i = 1; i < line.size(); ++i) {
                if (line[i] == '|' && line[i - 1] != '\\') {
                    row.cells.push_back(inlineText(cell, *doc));
                    cell.clear();
                } else {
                    cell += line[i];
                }
            }
            if (!trim(cell).empty()) row.cells.push_back(inlineText(cell, *doc));
            doc->blocks.push_back(std::move(row));
            continue;
        }
        if (line[0] == '>') {
            flush();
            std::string text = trim(line.substr(1));
            if (text.rfind("[!", 0) == 0 && text.back() == ']') text = text.substr(2, text.size() - 3); // [!NOTE]
            text = inlineText(text, *doc);
            if (!doc->blocks.empty() && doc->blocks.back().kind == Block::Kind::Quote) {
                if (!text.empty()) doc->blocks.back().text += (doc->blocks.back().text.empty() ? "" : " ") + text;
            } else {
                doc->blocks.push_back({Block::Kind::Quote, 0, text, {}});
            }
            continue;
        }
        static std::regex const bullet(R"(^(\s*)([-*+]|\d+[.)])\s+(.*)$)");
        std::smatch m;
        if (std::regex_match(raw, m, bullet)) {
            flush();
            int  indent = int(m[1].length() / 2);
            auto marker = m[2].str();
            auto text   = inlineText(m[3].str(), *doc);
            if (isdigit(uint8_t(marker[0]))) text = marker + " " + text;
            doc->blocks.push_back({Block::Kind::Bullet, std::min(indent, 4), text, {}});
            continue;
        }
        // Continuation of a list item.
        if (paragraph.empty() && !doc->blocks.empty() && doc->blocks.back().kind == Block::Kind::Bullet
            && (raw.rfind("  ", 0) == 0 || raw.rfind("\t", 0) == 0)) {
            doc->blocks.back().text += " " + inlineText(line, *doc);
            continue;
        }
        paragraph += (paragraph.empty() ? "" : " ") + line;
    }
    if (inCode && !code.empty()) doc->blocks.push_back({Block::Kind::Code, 0, code, {}});
    flush();

    for (auto const& b : doc->blocks) {
        fonts::note(b.text);
        for (auto const& c : b.cells) fonts::note(c);
    }
    return doc;
}

void render(Document const& doc) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (auto const& b : doc.blocks) {
        switch (b.kind) {
        case Block::Kind::Heading: {
            ImGui::Dummy({0, b.level <= 2 ? s(8) : s(4)});
            ImFont* f = b.level <= 1 ? fonts::title() : fonts::bold();
            ImGui::PushFont(f);
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(b.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
            if (b.level <= 2) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                dl->AddRectFilled({p.x, p.y - s(2)}, {p.x + ImGui::GetContentRegionAvail().x, p.y}, col::PanelEdge);
                ImGui::Dummy({0, s(2)});
            }
            break;
        }
        case Block::Kind::Paragraph:
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(b.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Dummy({0, s(4)});
            break;
        case Block::Kind::Bullet: {
            float indent = s(14) + b.level * s(16);
            ImGui::Indent(indent);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float  h = ImGui::GetFontSize();
            dl->AddRectFilled({p.x - s(10), p.y + h * 0.42f}, {p.x - s(5), p.y + h * 0.42f + s(5)}, col::TextDim);
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(b.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Unindent(indent);
            break;
        }
        case Block::Kind::Quote: {
            ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::Indent(s(12));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col::TextDim));
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(b.text.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::Unindent(s(12));
            ImVec2 end = ImGui::GetCursorScreenPos();
            dl->AddRectFilled({start.x, start.y}, {start.x + s(3), end.y - s(4)}, col::PanelEdge);
            ImGui::Dummy({0, s(4)});
            break;
        }
        case Block::Kind::Code: {
            ImVec2 start = ImGui::GetCursorScreenPos();
            float  w     = ImGui::GetContentRegionAvail().x;
            dl->ChannelsSplit(2);
            dl->ChannelsSetCurrent(1);
            ImGui::SetCursorScreenPos({start.x + s(8), start.y + s(6)});
            ImGui::PushTextWrapPos(start.x + w - s(8) - ImGui::GetWindowPos().x);
            ImGui::TextUnformatted(b.text.c_str());
            ImGui::PopTextWrapPos();
            ImVec2 end = ImGui::GetCursorScreenPos();
            dl->ChannelsSetCurrent(0);
            dl->AddRectFilled(start, {start.x + w, end.y + s(2)}, col::Inset);
            dl->ChannelsMerge();
            ImGui::SetCursorScreenPos({start.x, end.y + s(8)});
            ImGui::Dummy({0, 0});
            break;
        }
        case Block::Kind::Rule: {
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled({p.x, p.y + s(4)}, {p.x + ImGui::GetContentRegionAvail().x, p.y + s(6)}, col::PanelEdge);
            ImGui::Dummy({0, s(10)});
            break;
        }
        case Block::Kind::TableRow: {
            if (b.cells.empty()) break;
            ImGui::PushID(&b);
            if (ImGui::BeginTable("row", int(b.cells.size()), ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                for (auto const& c : b.cells) {
                    ImGui::TableNextColumn();
                    ImGui::PushTextWrapPos(0);
                    ImGui::TextUnformatted(c.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::EndTable();
            }
            ImGui::PopID();
            break;
        }
        }
    }
}

} // namespace modmenu::ui::markdown
