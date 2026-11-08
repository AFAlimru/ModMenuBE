#pragma once

#include <memory>
#include <string>
#include <vector>

namespace modmenu::ui::markdown {

// A README reduced to blocks ImGui can draw. Inline HTML (centered headers,
// badges, images) and inline markup are stripped; links keep their text.
struct Block {
    enum class Kind { Heading, Paragraph, Bullet, Quote, Code, Rule, TableRow } kind = Kind::Paragraph;
    int         level = 0; // heading level, list nesting
    std::string text;
    std::vector<std::string> cells; // TableRow
};

struct Document {
    std::vector<Block> blocks;
    std::vector<std::pair<std::string, std::string>> links; // text, url (http/https only)
};

std::shared_ptr<Document const> parse(std::string const& markdown);

// Draws the document at the current cursor, wrapping to the content width.
void render(Document const& doc);

} // namespace modmenu::ui::markdown
