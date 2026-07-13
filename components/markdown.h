#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "core/render/text.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(EUI_HAS_MD4C)
#include <md4c.h>
#endif

namespace components {

struct MarkdownStyle {
    explicit MarkdownStyle(const theme::ThemeColorTokens& tokens = theme::dark())
        : text(theme::withOpacity(tokens.text, 0.78f)),
          heading(theme::withOpacity(tokens.text, 0.96f)),
          muted(theme::withOpacity(tokens.text, 0.58f)),
          accent(tokens.primary),
          codeText(tokens.dark ? theme::color(0.88f, 0.94f, 1.0f) : theme::color(0.10f, 0.13f, 0.18f)),
          codeBackground(tokens.dark ? theme::color(0.08f, 0.09f, 0.11f, 1.0f) : theme::color(0.93f, 0.94f, 0.96f, 1.0f)),
          quoteBackground(theme::withOpacity(tokens.primary, tokens.dark ? 0.12f : 0.08f)),
          divider(theme::withOpacity(tokens.border, 0.82f)),
          bodySize(tokens.metrics.typography.body),
          bodyLineHeight(tokens.metrics.typography.body + tokens.metrics.typography.lineGapWide),
          h1Size(tokens.metrics.typography.displayCompact),
          h2Size(tokens.metrics.typography.heading),
          h3Size(tokens.metrics.typography.subtitle),
          codeSize(tokens.metrics.typography.label),
          blockGap(tokens.metrics.spacing.content),
          listIndent(tokens.metrics.control.indicator),
          codePadding(tokens.metrics.spacing.content),
          quotePadding(tokens.metrics.spacing.content),
          tableCellPadding(tokens.metrics.spacing.control),
          radius(tokens.metrics.radius.small) {}

    core::Color text;
    core::Color heading;
    core::Color muted;
    core::Color accent;
    core::Color codeText;
    core::Color codeBackground;
    core::Color quoteBackground;
    core::Color divider;
    std::string fontFamily;
    std::string codeFontFamily = "monospace";
    float bodySize = 16.0f;
    float bodyLineHeight = 24.0f;
    float h1Size = 30.0f;
    float h2Size = 24.0f;
    float h3Size = 20.0f;
    float codeSize = 14.0f;
    float blockGap = 12.0f;
    float listIndent = 22.0f;
    float codePadding = 12.0f;
    float quotePadding = 12.0f;
    float tableCellPadding = 10.0f;
    float radius = 6.0f;
};

namespace detail {

enum class MarkdownBlockKind {
    Paragraph,
    Heading,
    ListItem,
    Code,
    Html,
    TableRow,
    Divider
};

enum class MarkdownRunKind {
    Text,
    InlineCode,
    Html,
    Entity,
    Link,
    Image,
    Math,
    WikiLink,
    Underline
};

enum class MarkdownAlign {
    Default,
    Left,
    Center,
    Right
};

struct MarkdownRunStyle {
    bool emphasis = false;
    bool strong = false;
    bool deleted = false;
    bool underline = false;
    bool link = false;
    bool image = false;
    bool math = false;
    bool wiki = false;
    bool html = false;
};

struct MarkdownRun {
    MarkdownRunKind kind = MarkdownRunKind::Text;
    MarkdownRunStyle style;
    std::string text;
    std::string href;
    std::string title;
};

struct MarkdownTableCell {
    std::vector<MarkdownRun> runs;
    MarkdownAlign align = MarkdownAlign::Default;
};

struct MarkdownBlock {
    MarkdownBlockKind kind = MarkdownBlockKind::Paragraph;
    std::vector<MarkdownRun> runs;
    std::vector<MarkdownTableCell> cells;
    int headingLevel = 0;
    int quoteDepth = 0;
    int listDepth = 0;
    bool ordered = false;
    unsigned number = 0;
    bool task = false;
    bool taskChecked = false;
    bool tableHeader = false;
    std::string codeInfo;
    std::string codeLang;
};

inline std::string trimMarkdownText(std::string value) {
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

inline std::string decodeMarkdownEntity(const std::string& text) {
    if (text == "&amp;") {
        return "&";
    }
    if (text == "&lt;") {
        return "<";
    }
    if (text == "&gt;") {
        return ">";
    }
    if (text == "&quot;") {
        return "\"";
    }
    if (text == "&apos;") {
        return "'";
    }
    return text;
}

inline std::string displayText(const MarkdownRun& run) {
    std::string text = run.text;
    if (run.kind == MarkdownRunKind::Entity) {
        text = decodeMarkdownEntity(run.text);
    } else if (run.kind == MarkdownRunKind::Image) {
        text = "[image: " + (run.text.empty() ? run.href : run.text) + "]";
    } else if (run.kind == MarkdownRunKind::WikiLink) {
        text = "[[" + (run.text.empty() ? run.href : run.text) + "]]";
    } else if (run.kind == MarkdownRunKind::Math || run.style.math) {
        text = "$" + run.text + "$";
    } else if (run.kind == MarkdownRunKind::Html || run.style.html) {
        text = run.text;
    } else if ((run.kind == MarkdownRunKind::Link || run.style.link) && run.text.empty()) {
        text = run.href;
    }
    return text;
}

inline std::string plainText(const std::vector<MarkdownRun>& runs) {
    std::string value;
    for (const MarkdownRun& run : runs) {
        value += displayText(run);
    }
    return value;
}

inline std::string plainText(const std::vector<MarkdownTableCell>& cells, std::size_t index) {
    return index < cells.size() ? plainText(cells[index].runs) : "";
}

inline float estimateMarkdownTextHeight(const std::string& text, float width, float fontSize, float lineHeight) {
    const float safeLineHeight = lineHeight > 0.0f ? lineHeight : fontSize + 6.0f;
    const float averageCharWidth = std::max(1.0f, fontSize * 0.56f);
    const int charsPerLine = std::max(1, static_cast<int>(std::floor(std::max(1.0f, width) / averageCharWidth)));
    int lines = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t count = end == std::string::npos ? text.size() - start : end - start;
        lines += std::max(1, static_cast<int>((count + static_cast<std::size_t>(charsPerLine) - 1) / static_cast<std::size_t>(charsPerLine)));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return static_cast<float>(std::max(1, lines)) * safeLineHeight;
}

inline std::vector<std::string> splitMarkdownTextForLayout(const std::string& text) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (begin < text.size()) {
        const unsigned char first = static_cast<unsigned char>(text[begin]);
        if (std::isspace(first) != 0) {
            if (text[begin] == '\n') {
                parts.push_back("\n");
            }
            ++begin;
            continue;
        }

        if ((first & 0x80U) != 0U) {
            std::size_t end = begin + 1;
            while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
                ++end;
            }
            parts.push_back(text.substr(begin, end - begin));
            begin = end;
            continue;
        }

        std::size_t end = begin;
        while (end < text.size()) {
            const unsigned char ch = static_cast<unsigned char>(text[end]);
            if (std::isspace(ch) != 0 || (ch & 0x80U) != 0U) {
                break;
            }
            ++end;
        }
        parts.push_back(text.substr(begin, end - begin));
        begin = end;
    }
    return parts;
}

inline std::vector<std::string> splitMarkdownTextIntoCodepoints(const std::string& text) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    while (begin < text.size()) {
        std::size_t end = begin + 1;
        const unsigned char first = static_cast<unsigned char>(text[begin]);
        if ((first & 0x80U) != 0U) {
            while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
                ++end;
            }
        }
        parts.push_back(text.substr(begin, end - begin));
        begin = end;
    }
    return parts;
}

inline float measureMarkdownTextWidth(const std::string& text,
                                      const std::string& fontFamily,
                                      float fontSize,
                                      int fontWeight) {
    const float measured = core::TextPrimitive::measureTextWidth(text, fontFamily, fontSize, fontWeight);
    if (measured > 0.0f) {
        return measured;
    }
    return static_cast<float>(text.size()) * std::max(1.0f, fontSize * 0.56f);
}

inline std::string codeBlockDisplayText(const MarkdownBlock& block) {
    std::string text = plainText(block.runs);
    if (!block.codeLang.empty()) {
        text = block.codeLang + "\n" + text;
    } else if (!block.codeInfo.empty()) {
        text = block.codeInfo + "\n" + text;
    }
    return text;
}

inline float markdownHeadingSize(const MarkdownStyle& style, int level) {
    if (level <= 1) {
        return style.h1Size;
    }
    if (level == 2) {
        return style.h2Size;
    }
    return style.h3Size;
}

inline std::string markdownListPrefix(const MarkdownBlock& block) {
    if (block.task) {
        return block.taskChecked ? "[x]" : "[ ]";
    }
    if (block.ordered) {
        return std::to_string(block.number) + ".";
    }
    return "-";
}

inline core::HorizontalAlign horizontalAlign(MarkdownAlign align) {
    if (align == MarkdownAlign::Center) {
        return core::HorizontalAlign::Center;
    }
    if (align == MarkdownAlign::Right) {
        return core::HorizontalAlign::Right;
    }
    return core::HorizontalAlign::Left;
}

#if defined(EUI_HAS_MD4C)
struct MarkdownListContext {
    bool ordered = false;
    unsigned nextNumber = 1;
};

struct MarkdownListItemContext {
    bool task = false;
    bool checked = false;
};

struct MarkdownSpanContext {
    MarkdownRunKind kind = MarkdownRunKind::Text;
    MarkdownRunStyle style;
    std::string href;
    std::string title;
};

struct MarkdownParseState {
    std::vector<MarkdownBlock> blocks;
    std::vector<MarkdownListContext> lists;
    std::vector<MarkdownListItemContext> listItems;
    std::vector<MarkdownSpanContext> spans;
    MarkdownBlock current;
    bool hasCurrent = false;
    int quoteDepth = 0;
    bool inTableHead = false;
    bool inTableCell = false;
    std::vector<MarkdownTableCell> tableRow;
    MarkdownTableCell tableCell;
};

inline MarkdownParseState& markdownState(void* userdata) {
    return *static_cast<MarkdownParseState*>(userdata);
}

inline std::string markdownAttributeText(const MD_ATTRIBUTE& attr) {
    if (attr.text == nullptr || attr.size == 0) {
        return "";
    }
    return std::string(attr.text, attr.size);
}

inline MarkdownAlign markdownAlignFromMd(MD_ALIGN align) {
    switch (align) {
    case MD_ALIGN_LEFT:
        return MarkdownAlign::Left;
    case MD_ALIGN_CENTER:
        return MarkdownAlign::Center;
    case MD_ALIGN_RIGHT:
        return MarkdownAlign::Right;
    default:
        return MarkdownAlign::Default;
    }
}

inline MarkdownRunStyle currentRunStyle(const MarkdownParseState& state) {
    MarkdownRunStyle style;
    for (const MarkdownSpanContext& span : state.spans) {
        style.emphasis = style.emphasis || span.style.emphasis;
        style.strong = style.strong || span.style.strong;
        style.deleted = style.deleted || span.style.deleted;
        style.underline = style.underline || span.style.underline;
        style.link = style.link || span.style.link;
        style.image = style.image || span.style.image;
        style.math = style.math || span.style.math;
        style.wiki = style.wiki || span.style.wiki;
        style.html = style.html || span.style.html;
    }
    return style;
}

inline MarkdownRunKind currentRunKind(const MarkdownParseState& state, MD_TEXTTYPE type) {
    if (!state.spans.empty()) {
        return state.spans.back().kind;
    }
    if (type == MD_TEXT_CODE) {
        return MarkdownRunKind::InlineCode;
    }
    if (type == MD_TEXT_HTML) {
        return MarkdownRunKind::Html;
    }
    if (type == MD_TEXT_ENTITY) {
        return MarkdownRunKind::Entity;
    }
    if (type == MD_TEXT_LATEXMATH) {
        return MarkdownRunKind::Math;
    }
    return MarkdownRunKind::Text;
}

inline void pushMarkdownRun(MarkdownParseState& state, MD_TEXTTYPE type, std::string text) {
    if (text.empty()) {
        return;
    }

    MarkdownRun run;
    run.kind = currentRunKind(state, type);
    run.style = currentRunStyle(state);
    run.text = std::move(text);
    if (!state.spans.empty()) {
        run.href = state.spans.back().href;
        run.title = state.spans.back().title;
    }

    if (state.inTableCell) {
        state.tableCell.runs.push_back(std::move(run));
    } else if (state.hasCurrent) {
        state.current.runs.push_back(std::move(run));
    }
}

inline void finishMarkdownBlock(MarkdownParseState& state) {
    if (!state.hasCurrent) {
        return;
    }

    if (!trimMarkdownText(plainText(state.current.runs)).empty() || state.current.kind == MarkdownBlockKind::Divider) {
        state.blocks.push_back(std::move(state.current));
    }
    state.current = {};
    state.hasCurrent = false;
}

inline void beginMarkdownTextBlock(MarkdownParseState& state, MarkdownBlockKind kind) {
    finishMarkdownBlock(state);
    state.current = {};
    state.current.kind = kind;
    state.current.quoteDepth = state.quoteDepth;
    state.current.listDepth = static_cast<int>(state.lists.size());
    if (!state.lists.empty()) {
        MarkdownListContext& list = state.lists.back();
        state.current.ordered = list.ordered;
        if (list.ordered) {
            state.current.number = list.nextNumber++;
        }
    }
    if (!state.listItems.empty()) {
        state.current.task = state.listItems.back().task;
        state.current.taskChecked = state.listItems.back().checked;
    }
    state.hasCurrent = true;
}

inline int enterMarkdownBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    MarkdownParseState& state = markdownState(userdata);
    switch (type) {
    case MD_BLOCK_QUOTE:
        ++state.quoteDepth;
        break;
    case MD_BLOCK_UL:
        state.lists.push_back({false, 1});
        break;
    case MD_BLOCK_OL: {
        unsigned start = 1;
        if (detail != nullptr) {
            start = static_cast<const MD_BLOCK_OL_DETAIL*>(detail)->start;
        }
        state.lists.push_back({true, start});
        break;
    }
    case MD_BLOCK_LI: {
        MarkdownListItemContext item;
        if (detail != nullptr) {
            const auto* li = static_cast<const MD_BLOCK_LI_DETAIL*>(detail);
            item.task = li->is_task != 0;
            item.checked = li->task_mark == 'x' || li->task_mark == 'X';
        }
        state.listItems.push_back(item);
        beginMarkdownTextBlock(state, MarkdownBlockKind::ListItem);
        break;
    }
    case MD_BLOCK_H:
        beginMarkdownTextBlock(state, MarkdownBlockKind::Heading);
        if (detail != nullptr) {
            state.current.headingLevel = std::clamp(static_cast<int>(static_cast<const MD_BLOCK_H_DETAIL*>(detail)->level), 1, 6);
        }
        break;
    case MD_BLOCK_P:
        if (state.lists.empty()) {
            beginMarkdownTextBlock(state, MarkdownBlockKind::Paragraph);
        } else if (!state.hasCurrent || state.current.kind != MarkdownBlockKind::ListItem || !trimMarkdownText(plainText(state.current.runs)).empty()) {
            beginMarkdownTextBlock(state, MarkdownBlockKind::ListItem);
        }
        break;
    case MD_BLOCK_CODE:
        beginMarkdownTextBlock(state, MarkdownBlockKind::Code);
        if (detail != nullptr) {
            const auto* code = static_cast<const MD_BLOCK_CODE_DETAIL*>(detail);
            state.current.codeInfo = markdownAttributeText(code->info);
            state.current.codeLang = markdownAttributeText(code->lang);
        }
        break;
    case MD_BLOCK_HTML:
        beginMarkdownTextBlock(state, MarkdownBlockKind::Html);
        break;
    case MD_BLOCK_HR:
        finishMarkdownBlock(state);
        state.blocks.push_back({MarkdownBlockKind::Divider});
        break;
    case MD_BLOCK_THEAD:
        state.inTableHead = true;
        break;
    case MD_BLOCK_TR:
        state.tableRow.clear();
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        state.inTableCell = true;
        state.tableCell = {};
        if (detail != nullptr) {
            state.tableCell.align = markdownAlignFromMd(static_cast<const MD_BLOCK_TD_DETAIL*>(detail)->align);
        }
        break;
    default:
        break;
    }
    return 0;
}

inline int leaveMarkdownBlock(MD_BLOCKTYPE type, void*, void* userdata) {
    MarkdownParseState& state = markdownState(userdata);
    switch (type) {
    case MD_BLOCK_H:
    case MD_BLOCK_P:
    case MD_BLOCK_CODE:
    case MD_BLOCK_HTML:
        finishMarkdownBlock(state);
        break;
    case MD_BLOCK_LI:
        finishMarkdownBlock(state);
        if (!state.listItems.empty()) {
            state.listItems.pop_back();
        }
        break;
    case MD_BLOCK_QUOTE:
        state.quoteDepth = std::max(0, state.quoteDepth - 1);
        break;
    case MD_BLOCK_UL:
    case MD_BLOCK_OL:
        if (!state.lists.empty()) {
            state.lists.pop_back();
        }
        break;
    case MD_BLOCK_THEAD:
        state.inTableHead = false;
        break;
    case MD_BLOCK_TH:
    case MD_BLOCK_TD:
        state.inTableCell = false;
        state.tableRow.push_back(std::move(state.tableCell));
        state.tableCell = {};
        break;
    case MD_BLOCK_TR:
        if (!state.tableRow.empty()) {
            MarkdownBlock row;
            row.kind = MarkdownBlockKind::TableRow;
            row.cells = std::move(state.tableRow);
            row.tableHeader = state.inTableHead;
            state.blocks.push_back(std::move(row));
        }
        state.tableRow.clear();
        break;
    default:
        break;
    }
    return 0;
}

inline int enterMarkdownSpan(MD_SPANTYPE type, void* detail, void* userdata) {
    MarkdownParseState& state = markdownState(userdata);
    MarkdownSpanContext span;
    span.style = currentRunStyle(state);
    switch (type) {
    case MD_SPAN_EM:
        span.style.emphasis = true;
        break;
    case MD_SPAN_STRONG:
        span.style.strong = true;
        break;
    case MD_SPAN_A:
        span.kind = MarkdownRunKind::Link;
        span.style.link = true;
        if (detail != nullptr) {
            const auto* link = static_cast<const MD_SPAN_A_DETAIL*>(detail);
            span.href = markdownAttributeText(link->href);
            span.title = markdownAttributeText(link->title);
        }
        break;
    case MD_SPAN_IMG:
        span.kind = MarkdownRunKind::Image;
        span.style.image = true;
        if (detail != nullptr) {
            const auto* image = static_cast<const MD_SPAN_IMG_DETAIL*>(detail);
            span.href = markdownAttributeText(image->src);
            span.title = markdownAttributeText(image->title);
        }
        break;
    case MD_SPAN_CODE:
        span.kind = MarkdownRunKind::InlineCode;
        break;
    case MD_SPAN_DEL:
        span.style.deleted = true;
        break;
    case MD_SPAN_LATEXMATH:
    case MD_SPAN_LATEXMATH_DISPLAY:
        span.kind = MarkdownRunKind::Math;
        span.style.math = true;
        break;
    case MD_SPAN_WIKILINK:
        span.kind = MarkdownRunKind::WikiLink;
        span.style.wiki = true;
        if (detail != nullptr) {
            span.href = markdownAttributeText(static_cast<const MD_SPAN_WIKILINK_DETAIL*>(detail)->target);
        }
        break;
    case MD_SPAN_U:
        span.kind = MarkdownRunKind::Underline;
        span.style.underline = true;
        break;
    default:
        break;
    }
    state.spans.push_back(std::move(span));
    return 0;
}

inline int leaveMarkdownSpan(MD_SPANTYPE, void*, void* userdata) {
    MarkdownParseState& state = markdownState(userdata);
    if (!state.spans.empty()) {
        state.spans.pop_back();
    }
    return 0;
}

inline int markdownText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
    MarkdownParseState& state = markdownState(userdata);
    switch (type) {
    case MD_TEXT_BR:
        pushMarkdownRun(state, type, "\n");
        break;
    case MD_TEXT_SOFTBR:
        pushMarkdownRun(state, type, " ");
        break;
    case MD_TEXT_NULLCHAR:
        pushMarkdownRun(state, type, "\xEF\xBF\xBD");
        break;
    default:
        pushMarkdownRun(state, type, text == nullptr ? "" : std::string(text, size));
        break;
    }
    return 0;
}

inline std::vector<MarkdownBlock> parseMarkdownBlocks(const std::string& source) {
    MarkdownParseState state;
    MD_PARSER parser = {};
    parser.abi_version = 0;
    parser.flags = MD_DIALECT_GITHUB |
                   MD_FLAG_LATEXMATHSPANS |
                   MD_FLAG_WIKILINKS |
                   MD_FLAG_UNDERLINE |
                   MD_FLAG_PERMISSIVEATXHEADERS;
    parser.enter_block = enterMarkdownBlock;
    parser.leave_block = leaveMarkdownBlock;
    parser.enter_span = enterMarkdownSpan;
    parser.leave_span = leaveMarkdownSpan;
    parser.text = markdownText;

    const int result = md_parse(source.c_str(), static_cast<MD_SIZE>(source.size()), &parser, &state);
    finishMarkdownBlock(state);
    if (result == 0) {
        return state.blocks;
    }

    MarkdownBlock block;
    block.kind = MarkdownBlockKind::Paragraph;
    block.runs.push_back({MarkdownRunKind::Text, {}, source});
    return {block};
}
#else
inline std::vector<MarkdownBlock> parseMarkdownBlocks(const std::string& source) {
    MarkdownBlock block;
    block.kind = MarkdownBlockKind::Paragraph;
    block.runs.push_back({MarkdownRunKind::Text, {}, source});
    return {block};
}
#endif

} // namespace detail

class MarkdownBuilder {
public:
    MarkdownBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    static float estimateHeight(const std::string& markdown,
                                float width,
                                const MarkdownStyle& style = MarkdownStyle()) {
        const std::vector<detail::MarkdownBlock> blocks = detail::parseMarkdownBlocks(markdown);
        float height = 0.0f;
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            if (blocks[index].kind == detail::MarkdownBlockKind::TableRow) {
                std::size_t end = index + 1;
                while (end < blocks.size() && blocks[end].kind == detail::MarkdownBlockKind::TableRow) {
                    ++end;
                }
                height += estimateTableHeight(blocks, index, end, width, style);
                index = end - 1;
            } else {
                height += estimateBlockHeight(blocks[index], width, style);
            }
            if (index + 1 < blocks.size()) {
                height += style.blockGap;
            }
        }
        return height;
    }

    MarkdownBuilder& markdown(std::string value) { markdown_ = std::move(value); return *this; }
    MarkdownBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    MarkdownBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    MarkdownBuilder& position(float xValue, float yValue) {
        x_ = xValue;
        y_ = yValue;
        hasX_ = true;
        hasY_ = true;
        return *this;
    }
    MarkdownBuilder& margin(float value) {
        margin_ = core::EdgeInsets::all(std::max(0.0f, value));
        return *this;
    }
    MarkdownBuilder& margin(float horizontal, float vertical) {
        margin_ = {std::max(0.0f, horizontal), std::max(0.0f, vertical), std::max(0.0f, horizontal), std::max(0.0f, vertical)};
        return *this;
    }
    MarkdownBuilder& margin(float left, float top, float right, float bottom) {
        margin_ = {std::max(0.0f, left), std::max(0.0f, top), std::max(0.0f, right), std::max(0.0f, bottom)};
        return *this;
    }
    MarkdownBuilder& size(float width, float height) {
        width_ = std::max(0.0f, width);
        height_ = core::SizeValue::fixed(std::max(0.0f, height));
        return *this;
    }
    MarkdownBuilder& width(float value) { width_ = std::max(0.0f, value); return *this; }
    MarkdownBuilder& height(float value) {
        height_ = core::SizeValue::fixed(std::max(0.0f, value));
        return *this;
    }
    MarkdownBuilder& wrapContentHeight() { height_ = core::SizeValue::wrapContent(); return *this; }
    MarkdownBuilder& style(const MarkdownStyle& value) { style_ = value; return *this; }
    MarkdownBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = MarkdownStyle(tokens); return *this; }
    MarkdownBuilder& zIndex(int value) { zIndex_ = value; return *this; }

    void build() {
        const std::vector<detail::MarkdownBlock>& blocks = parsedBlocks();
        auto root = ui_.column(id_)
            .width(width_)
            .height(height_)
            .margin(margin_.left, margin_.top, margin_.right, margin_.bottom)
            .gap(style_.blockGap)
            .zIndex(zIndex_);
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                for (std::size_t index = 0; index < blocks.size(); ++index) {
                    if (blocks[index].kind == detail::MarkdownBlockKind::TableRow) {
                        std::size_t end = index + 1;
                        while (end < blocks.size() && blocks[end].kind == detail::MarkdownBlockKind::TableRow) {
                            ++end;
                        }
                        renderTable(blocks, index, end);
                        index = end - 1;
                        continue;
                    }
                    renderBlock(blocks[index], index);
                }
            })
            .build();
    }

private:
    struct ParseCache {
        std::string markdown;
        std::vector<detail::MarkdownBlock> blocks;
        bool valid = false;
    };

    const std::vector<detail::MarkdownBlock>& parsedBlocks() {
        ParseCache& cache = ui_.state<ParseCache>(id_ + ".parse");
        if (!cache.valid || cache.markdown != markdown_) {
            cache.markdown = markdown_;
            cache.blocks = detail::parseMarkdownBlocks(markdown_);
            cache.valid = true;
        }
        return cache.blocks;
    }

    static bool runIsChip(const detail::MarkdownRun& run) {
        return run.kind == detail::MarkdownRunKind::InlineCode ||
               run.kind == detail::MarkdownRunKind::Image ||
               run.kind == detail::MarkdownRunKind::Math ||
               run.kind == detail::MarkdownRunKind::WikiLink ||
               run.kind == detail::MarkdownRunKind::Html ||
               run.style.html;
    }

    static float estimateInlineSegmentWidth(const detail::MarkdownRun& run,
                                            const std::string& text,
                                            float maxWidth,
                                            float fontSize,
                                            bool heading,
                                            const MarkdownStyle& style) {
        if (text == "\n") {
            return maxWidth;
        }
        const bool code = run.kind == detail::MarkdownRunKind::InlineCode;
        const bool chip = runIsChip(run);
        const std::string fontFamily = code ? style.codeFontFamily : style.fontFamily;
        const float runFontSize = code ? std::max(1.0f, fontSize - 1.0f) : fontSize;
        const int weight = heading || run.style.strong ? 700 : 400;
        const float padding = chip ? 12.0f : 0.0f;
        return std::min(maxWidth, detail::measureMarkdownTextWidth(text, fontFamily, runFontSize, weight) + padding);
    }

    struct InlineSegment {
        detail::MarkdownRun run;
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        bool chip = false;
    };

    struct InlineLayoutEntry {
        std::vector<InlineSegment> segments;
        float measuredHeight = 0.0f;
    };

    struct InlineLayoutCache {
        std::unordered_map<std::string, InlineLayoutEntry> entries;
    };

    static std::string inlineLayoutKey(const std::string& partId,
                                       const std::vector<detail::MarkdownRun>& runs,
                                       float width,
                                       float fontSize,
                                       float lineHeight,
                                       bool heading,
                                       core::HorizontalAlign align,
                                       const MarkdownStyle& style) {
        std::string key = partId;
        key += "|w=" + std::to_string(static_cast<int>(width * 10.0f + 0.5f));
        key += "|fs=" + std::to_string(static_cast<int>(fontSize * 10.0f + 0.5f));
        key += "|lh=" + std::to_string(static_cast<int>(lineHeight * 10.0f + 0.5f));
        key += heading ? "|h=1" : "|h=0";
        key += "|a=" + std::to_string(static_cast<int>(align));
        key += "|font=" + style.fontFamily;
        key += "|code=" + style.codeFontFamily;
        for (const detail::MarkdownRun& run : runs) {
            key += "|";
            key += std::to_string(static_cast<int>(run.kind));
            key += run.style.emphasis ? "e" : "";
            key += run.style.strong ? "s" : "";
            key += run.style.deleted ? "d" : "";
            key += run.style.underline ? "u" : "";
            key += run.style.link ? "l" : "";
            key += run.style.image ? "i" : "";
            key += run.style.math ? "m" : "";
            key += run.style.wiki ? "w" : "";
            key += run.style.html ? "h" : "";
            key += ":";
            key += detail::displayText(run);
        }
        return key;
    }

    static std::vector<InlineSegment> layoutInlineRunsWithStyle(const std::vector<detail::MarkdownRun>& runs,
                                                                float width,
                                                                float fontSize,
                                                                float lineHeight,
                                                                bool heading,
                                                                const MarkdownStyle& style,
                                                                float& outHeight) {
        std::vector<InlineSegment> segments;
        const float gap = 4.0f;
        float cursorX = 0.0f;
        float cursorY = 0.0f;
        bool hasLineItem = false;

        auto rawSegmentWidth = [&](const detail::MarkdownRun& run, const std::string& text) {
            if (text == "\n") {
                return width;
            }
            const bool code = run.kind == detail::MarkdownRunKind::InlineCode;
            const bool chip = runIsChip(run);
            const std::string fontFamily = code ? style.codeFontFamily : style.fontFamily;
            const float runFontSize = code ? std::max(1.0f, fontSize - 1.0f) : fontSize;
            const int weight = heading || run.style.strong ? 700 : 400;
            const float padding = chip ? 12.0f : 0.0f;
            return detail::measureMarkdownTextWidth(text, fontFamily, runFontSize, weight) + padding;
        };

        auto pushFittingSegment = [&](detail::MarkdownRun run, std::string text) {
            if (text.empty()) {
                return;
            }
            if (text == "\n") {
                cursorX = 0.0f;
                cursorY += lineHeight;
                hasLineItem = false;
                return;
            }
            const bool chip = runIsChip(run);
            const float segmentWidth = estimateInlineSegmentWidth(run, text, width, fontSize, heading, style);
            if (hasLineItem && width > 0.0f && cursorX + gap + segmentWidth > width) {
                cursorX = 0.0f;
                cursorY += lineHeight;
                hasLineItem = false;
            }
            if (hasLineItem) {
                cursorX += gap;
            }
            segments.push_back({std::move(run), std::move(text), cursorX, cursorY, segmentWidth, chip});
            cursorX += segmentWidth;
            hasLineItem = true;
        };

        auto pushSegment = [&](const detail::MarkdownRun& run, const std::string& text) {
            if (text.empty()) {
                return;
            }
            if (text == "\n" || width <= 0.0f || rawSegmentWidth(run, text) <= width) {
                pushFittingSegment(run, text);
                return;
            }

            std::string chunk;
            for (const std::string& part : detail::splitMarkdownTextIntoCodepoints(text)) {
                if (part.empty()) {
                    continue;
                }
                const std::string next = chunk + part;
                if (!chunk.empty() && rawSegmentWidth(run, next) > width) {
                    pushFittingSegment(run, chunk);
                    chunk.clear();
                }
                chunk += part;
            }
            if (!chunk.empty()) {
                pushFittingSegment(run, chunk);
            }
        };

        for (const detail::MarkdownRun& run : runs) {
            const std::string text = detail::displayText(run);
            if (text.empty()) {
                continue;
            }
            if (runIsChip(run)) {
                pushSegment(run, text);
                continue;
            }
            for (const std::string& part : detail::splitMarkdownTextForLayout(text)) {
                pushSegment(run, part);
            }
        }

        outHeight = segments.empty() ? lineHeight : cursorY + lineHeight;
        return segments;
    }

    const InlineLayoutEntry& cachedInlineLayout(const std::string& partId,
                                                const std::vector<detail::MarkdownRun>& runs,
                                                float width,
                                                float fontSize,
                                                float lineHeight,
                                                bool heading,
                                                core::HorizontalAlign align) {
        InlineLayoutCache& cache = ui_.state<InlineLayoutCache>(id_ + ".inlineLayout");
        const std::string key = inlineLayoutKey(partId, runs, width, fontSize, lineHeight, heading, align, style_);
        const auto existing = cache.entries.find(key);
        if (existing != cache.entries.end()) {
            return existing->second;
        }

        if (cache.entries.size() >= 256) {
            cache.entries.clear();
        }

        InlineLayoutEntry entry;
        entry.measuredHeight = lineHeight;
        entry.segments = layoutInlineRunsWithStyle(runs, width, fontSize, lineHeight, heading, style_, entry.measuredHeight);
        alignInlineSegments(entry.segments, width, align);
        const auto inserted = cache.entries.emplace(key, std::move(entry));
        return inserted.first->second;
    }

    static float estimateInlineRunsHeight(const std::vector<detail::MarkdownRun>& runs,
                                          float width,
                                          float fontSize,
                                          float lineHeight,
                                          bool heading,
                                          const MarkdownStyle& style) {
        float height = lineHeight;
        (void)layoutInlineRunsWithStyle(runs, width, fontSize, lineHeight, heading, style, height);
        return height;
    }

    static float estimateTableRowHeight(const detail::MarkdownBlock& block,
                                        float contentWidth,
                                        const MarkdownStyle& style) {
        const std::size_t cellCount = std::max<std::size_t>(1, block.cells.size());
        const float cellWidth = contentWidth / static_cast<float>(cellCount);
        const float textWidth = std::max(0.0f, cellWidth - style.tableCellPadding * 2.0f);
        float height = 0.0f;
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            height = std::max(height, estimateInlineRunsHeight(
                cell < block.cells.size() ? block.cells[cell].runs : std::vector<detail::MarkdownRun>{},
                textWidth,
                style.bodySize,
                style.bodyLineHeight,
                false,
                style));
        }
        return height + style.tableCellPadding * 2.0f;
    }

    static float estimateTableHeight(const std::vector<detail::MarkdownBlock>& blocks,
                                     std::size_t begin,
                                     std::size_t end,
                                     float width,
                                     const MarkdownStyle& style) {
        if (begin >= end) {
            return 0.0f;
        }
        const detail::MarkdownBlock& first = blocks[begin];
        const float quoteInset = static_cast<float>(std::max(0, first.quoteDepth)) * 8.0f;
        const float listInset = static_cast<float>(std::max(0, first.listDepth - 1)) * style.listIndent;
        const float contentWidth = std::max(0.0f, width - quoteInset - listInset);
        float height = 0.0f;
        for (std::size_t row = begin; row < end; ++row) {
            height += estimateTableRowHeight(blocks[row], contentWidth, style);
        }
        return height;
    }

    static float estimateBlockHeight(const detail::MarkdownBlock& block,
                                     float width,
                                     const MarkdownStyle& style) {
        const float quoteInset = static_cast<float>(std::max(0, block.quoteDepth)) * 8.0f;
        const float listInset = static_cast<float>(std::max(0, block.listDepth - 1)) * style.listIndent;
        const float contentWidth = std::max(0.0f, width - quoteInset - listInset);

        if (block.kind == detail::MarkdownBlockKind::Divider) {
            return 9.0f;
        }
        if (block.kind == detail::MarkdownBlockKind::Code || block.kind == detail::MarkdownBlockKind::Html) {
            const std::string text = block.kind == detail::MarkdownBlockKind::Code
                ? detail::codeBlockDisplayText(block)
                : detail::plainText(block.runs);
            const float textWidth = std::max(0.0f, contentWidth - style.codePadding * 2.0f);
            return detail::estimateMarkdownTextHeight(text, textWidth, style.codeSize, style.codeSize + 6.0f) + style.codePadding * 2.0f;
        }
        if (block.kind == detail::MarkdownBlockKind::ListItem) {
            const float markerWidth = block.task ? 34.0f : 24.0f;
            const float textWidth = std::max(0.0f, contentWidth - markerWidth - 8.0f);
            return std::max(style.bodyLineHeight, estimateInlineRunsHeight(block.runs, textWidth, style.bodySize, style.bodyLineHeight, false, style));
        }

        const bool heading = block.kind == detail::MarkdownBlockKind::Heading;
        const float fontSize = heading ? detail::markdownHeadingSize(style, block.headingLevel) : style.bodySize;
        const float lineHeight = heading ? fontSize + 6.0f : style.bodyLineHeight;
        const float textHeight = estimateInlineRunsHeight(block.runs, contentWidth, fontSize, lineHeight, heading, style);
        if (block.quoteDepth > 0) {
            return textHeight + style.quotePadding * 2.0f;
        }
        return textHeight;
    }

    void renderBlock(const detail::MarkdownBlock& block, std::size_t index) {
        const std::string partId = id_ + ".block." + std::to_string(index);
        const float quoteInset = static_cast<float>(std::max(0, block.quoteDepth)) * 8.0f;
        const float listInset = static_cast<float>(std::max(0, block.listDepth - 1)) * style_.listIndent;
        const float contentWidth = std::max(0.0f, width_ - quoteInset - listInset);

        if (block.kind == detail::MarkdownBlockKind::Divider) {
            ui_.rect(partId)
                .width(contentWidth)
                .height(1.0f)
                .margin(quoteInset + listInset, 4.0f, 0.0f, 4.0f)
                .color(style_.divider)
                .build();
            return;
        }

        if (block.kind == detail::MarkdownBlockKind::Heading) {
            const float fontSize = detail::markdownHeadingSize(style_, block.headingLevel);
            renderInlineBlock(partId, block, quoteInset, listInset, contentWidth, fontSize, fontSize + 6.0f, true);
            return;
        }

        if (block.kind == detail::MarkdownBlockKind::Code) {
            renderCodeBlock(partId, block, quoteInset, listInset, contentWidth);
            return;
        }

        if (block.kind == detail::MarkdownBlockKind::Html) {
            renderMutedTextBlock(partId, block, quoteInset, listInset, contentWidth);
            return;
        }

        if (block.kind == detail::MarkdownBlockKind::ListItem) {
            renderListItem(partId, block, quoteInset, listInset, contentWidth);
            return;
        }

        renderInlineBlock(partId, block, quoteInset, listInset, contentWidth, style_.bodySize, style_.bodyLineHeight, false);
    }

    void renderListItem(const std::string& partId,
                        const detail::MarkdownBlock& block,
                        float quoteInset,
                        float listInset,
                        float contentWidth) {
        const float markerWidth = block.task ? 34.0f : 24.0f;
        const float textWidth = std::max(0.0f, contentWidth - markerWidth - 8.0f);
        const float height = std::max(style_.bodyLineHeight,
                                      inlineRunsHeight(partId + ".text",
                                                       block.runs,
                                                       textWidth,
                                                       style_.bodySize,
                                                       style_.bodyLineHeight,
                                                       false));
        ui_.stack(partId)
            .width(contentWidth)
            .height(height)
            .margin(quoteInset + listInset, 0.0f, 0.0f, 0.0f)
            .content([&] {
                ui_.text(partId + ".mark")
                    .size(markerWidth, style_.bodyLineHeight)
                    .text(detail::markdownListPrefix(block))
                    .fontFamily(style_.fontFamily)
                    .fontSize(style_.bodySize)
                    .lineHeight(style_.bodyLineHeight)
                    .horizontalAlign(core::HorizontalAlign::Right)
                    .verticalAlign(core::VerticalAlign::Center)
                    .color(block.task ? style_.accent : style_.muted)
                    .build();
                renderInlineRuns(partId + ".text", block.runs, textWidth, height, style_.bodySize, style_.bodyLineHeight, false, markerWidth + 8.0f, 0.0f);
            })
            .build();
    }

    void renderInlineBlock(const std::string& partId,
                           const detail::MarkdownBlock& block,
                           float quoteInset,
                           float listInset,
                           float contentWidth,
                           float fontSize,
                           float lineHeight,
                           bool heading) {
        const float blockHeight = inlineRunsHeight(partId + ".text", block.runs, contentWidth, fontSize, lineHeight, heading);
        if (block.quoteDepth > 0) {
            const float barWidth = 3.0f;
            const float barGap = 8.0f;
            const float panelX = barWidth + barGap;
            const float panelWidth = std::max(0.0f, contentWidth - panelX);
            const float textX = panelX + style_.quotePadding;
            const float textWidth = std::max(0.0f, panelWidth - style_.quotePadding * 2.0f);
            const float textHeight = inlineRunsHeight(partId + ".text", block.runs, textWidth, fontSize, lineHeight, heading);
            const float height = textHeight + style_.quotePadding * 2.0f;
            ui_.stack(partId)
                .width(contentWidth)
                .height(height)
                .margin(quoteInset + listInset, 0.0f, 0.0f, 0.0f)
                .content([&] {
                    ui_.rect(partId + ".bar")
                        .size(barWidth, height)
                        .color(style_.accent)
                        .radius(1.5f)
                        .build();
                    ui_.rect(partId + ".bg")
                        .x(panelX)
                        .width(panelWidth)
                        .height(height)
                        .color(style_.quoteBackground)
                        .radius(style_.radius)
                        .build();
                    renderInlineRuns(partId + ".text", block.runs, textWidth, textHeight, fontSize, lineHeight, heading, textX, style_.quotePadding);
                })
                .build();
            return;
        }

        ui_.stack(partId)
            .width(contentWidth)
            .height(blockHeight)
            .margin(quoteInset + listInset, 0.0f, 0.0f, 0.0f)
            .content([&] {
                renderInlineRuns(partId + ".text", block.runs, contentWidth, blockHeight, fontSize, lineHeight, heading);
            })
            .build();
    }

    core::Color inlineRunColor(const detail::MarkdownRun& run, bool heading) const {
        if (run.kind == detail::MarkdownRunKind::InlineCode) {
            return style_.codeText;
        }
        if (run.style.link || run.kind == detail::MarkdownRunKind::Link ||
            run.kind == detail::MarkdownRunKind::Image ||
            run.kind == detail::MarkdownRunKind::Math ||
            run.kind == detail::MarkdownRunKind::WikiLink) {
            return style_.accent;
        }
        if (run.kind == detail::MarkdownRunKind::Html || run.style.html) {
            return style_.muted;
        }
        return heading ? style_.heading : style_.text;
    }

    float inlineRunsHeight(const std::string& partId,
                           const std::vector<detail::MarkdownRun>& runs,
                           float width,
                           float fontSize,
                           float lineHeight,
                           bool heading,
                           core::HorizontalAlign align = core::HorizontalAlign::Left) {
        return cachedInlineLayout(partId, runs, width, fontSize, lineHeight, heading, align).measuredHeight;
    }

    void renderInlineRuns(const std::string& partId,
                          const std::vector<detail::MarkdownRun>& runs,
                          float width,
                          float height,
                          float fontSize,
                          float lineHeight,
                          bool heading,
                          float x = 0.0f,
                          float y = 0.0f,
                          core::HorizontalAlign align = core::HorizontalAlign::Left) {
        const InlineLayoutEntry& layout = cachedInlineLayout(partId, runs, width, fontSize, lineHeight, heading, align);
        ui_.stack(partId)
            .position(x, y)
            .size(width, height)
            .content([&] {
                for (std::size_t index = 0; index < layout.segments.size(); ++index) {
                    renderInlineSegment(partId + ".seg." + std::to_string(index), layout.segments[index], fontSize, lineHeight, heading);
                }
            })
            .build();
    }

    void renderInlineSegment(const std::string& partId,
                             const InlineSegment& segment,
                             float fontSize,
                             float lineHeight,
                             bool heading) {
        const bool code = segment.run.kind == detail::MarkdownRunKind::InlineCode;
        const float runFontSize = code ? std::max(1.0f, fontSize - 1.0f) : fontSize;
        const int weight = heading || segment.run.style.strong ? 700 : 400;
        const float textInset = segment.chip ? 6.0f : 0.0f;
        const core::Color color = inlineRunColor(segment.run, heading);

        ui_.stack(partId)
            .position(segment.x, segment.y)
            .size(segment.width, lineHeight)
            .content([&] {
                if (segment.chip) {
                    const float bgHeight = std::max(16.0f, lineHeight - 4.0f);
                    ui_.rect(partId + ".bg")
                        .y((lineHeight - bgHeight) * 0.5f)
                        .size(segment.width, bgHeight)
                        .color(code ? style_.codeBackground : style_.quoteBackground)
                        .radius(style_.radius)
                        .build();
                }
                ui_.text(partId + ".text")
                    .x(textInset)
                    .width(std::max(0.0f, segment.width - textInset * 2.0f))
                    .height(lineHeight)
                    .text(segment.text)
                    .fontFamily(code ? style_.codeFontFamily : style_.fontFamily)
                    .fontSize(runFontSize)
                    .fontWeight(weight)
                    .lineHeight(lineHeight)
                    .verticalAlign(core::VerticalAlign::Center)
                    .color(color)
                    .build();
                if (segment.run.style.deleted || segment.run.style.underline || segment.run.style.link) {
                    const float lineY = segment.run.style.deleted ? lineHeight * 0.52f : lineHeight - 4.0f;
                    ui_.rect(partId + ".decor")
                        .x(textInset)
                        .y(lineY)
                        .size(std::max(0.0f, segment.width - textInset * 2.0f), 1.0f)
                        .color(segment.run.style.link ? style_.accent : color)
                        .build();
                }
            })
            .build();
    }

    void alignInlineSegments(std::vector<InlineSegment>& segments,
                             float width,
                             core::HorizontalAlign align) const {
        if (align == core::HorizontalAlign::Left || width <= 0.0f) {
            return;
        }
        std::size_t lineBegin = 0;
        while (lineBegin < segments.size()) {
            const float lineY = segments[lineBegin].y;
            std::size_t lineEnd = lineBegin;
            float lineRight = 0.0f;
            while (lineEnd < segments.size() && std::fabs(segments[lineEnd].y - lineY) <= 0.001f) {
                lineRight = std::max(lineRight, segments[lineEnd].x + segments[lineEnd].width);
                ++lineEnd;
            }
            const float extra = std::max(0.0f, width - lineRight);
            const float offset = align == core::HorizontalAlign::Center ? extra * 0.5f : extra;
            for (std::size_t index = lineBegin; index < lineEnd; ++index) {
                segments[index].x += offset;
            }
            lineBegin = lineEnd;
        }
    }

    void renderCodeBlock(const std::string& partId,
                         const detail::MarkdownBlock& block,
                         float quoteInset,
                         float listInset,
                         float contentWidth) {
        const std::string text = detail::codeBlockDisplayText(block);
        const float textWidth = std::max(0.0f, contentWidth - style_.codePadding * 2.0f);
        const float textHeight = detail::estimateMarkdownTextHeight(text, textWidth, style_.codeSize, style_.codeSize + 6.0f);
        const float blockHeight = textHeight + style_.codePadding * 2.0f;
        ui_.stack(partId)
            .width(contentWidth)
            .height(blockHeight)
            .margin(quoteInset + listInset, 0.0f, 0.0f, 0.0f)
            .content([&] {
                ui_.rect(partId + ".bg")
                    .size(contentWidth, blockHeight)
                    .color(style_.codeBackground)
                    .radius(style_.radius)
                    .build();
                ui_.text(partId + ".text")
                    .x(style_.codePadding)
                    .y(style_.codePadding)
                    .width(textWidth)
                    .height(textHeight)
                    .text(text)
                    .fontFamily(style_.codeFontFamily)
                    .fontSize(style_.codeSize)
                    .lineHeight(style_.codeSize + 6.0f)
                    .wrap()
                    .color(style_.codeText)
                    .build();
            })
            .build();
    }

    void renderMutedTextBlock(const std::string& partId,
                              const detail::MarkdownBlock& block,
                              float quoteInset,
                              float listInset,
                              float contentWidth) {
        const std::string text = detail::plainText(block.runs);
        const float textWidth = std::max(0.0f, contentWidth - style_.codePadding * 2.0f);
        const float textHeight = detail::estimateMarkdownTextHeight(text, textWidth, style_.codeSize, style_.codeSize + 6.0f);
        const float blockHeight = textHeight + style_.codePadding * 2.0f;
        ui_.stack(partId)
            .width(contentWidth)
            .height(blockHeight)
            .margin(quoteInset + listInset, 0.0f, 0.0f, 0.0f)
            .content([&] {
                ui_.rect(partId + ".bg")
                    .size(contentWidth, blockHeight)
                    .color(style_.quoteBackground)
                    .radius(style_.radius)
                    .build();
                ui_.text(partId + ".text")
                    .x(style_.codePadding)
                    .y(style_.codePadding)
                    .width(textWidth)
                    .height(textHeight)
                    .text(text)
                    .fontFamily(style_.codeFontFamily)
                    .fontSize(style_.codeSize)
                    .lineHeight(style_.codeSize + 6.0f)
                    .wrap()
                    .color(style_.muted)
                    .build();
            })
            .build();
    }

    void renderTable(const std::vector<detail::MarkdownBlock>& blocks, std::size_t begin, std::size_t end) {
        if (begin >= end) {
            return;
        }

        const detail::MarkdownBlock& first = blocks[begin];
        const std::string tableId = id_ + ".table." + std::to_string(begin);
        const float quoteInset = static_cast<float>(std::max(0, first.quoteDepth)) * 8.0f;
        const float listInset = static_cast<float>(std::max(0, first.listDepth - 1)) * style_.listIndent;
        const float contentWidth = std::max(0.0f, width_ - quoteInset - listInset);
        float tableHeight = 0.0f;

        for (std::size_t row = begin; row < end; ++row) {
            tableHeight += tableRowHeight(tableId + ".row." + std::to_string(row - begin), blocks[row], contentWidth);
        }

        ui_.column(tableId)
            .width(contentWidth)
            .height(tableHeight)
            .margin(quoteInset + listInset, 0.0f, 0.0f, 0.0f)
            .gap(0.0f)
            .content([&] {
                for (std::size_t row = begin; row < end; ++row) {
                    renderTableRow(tableId + ".row." + std::to_string(row - begin), blocks[row], contentWidth);
                }
            })
            .build();
    }

    float tableRowHeight(const std::string& partId,
                         const detail::MarkdownBlock& block,
                         float contentWidth) {
        const std::size_t cellCount = std::max<std::size_t>(1, block.cells.size());
        const float cellWidth = contentWidth / static_cast<float>(cellCount);
        const float textWidth = std::max(0.0f, cellWidth - style_.tableCellPadding * 2.0f);
        const std::vector<detail::MarkdownRun> emptyRuns;
        float height = 0.0f;
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            const bool hasCell = cell < block.cells.size();
            const std::vector<detail::MarkdownRun>& runs = hasCell ? block.cells[cell].runs : emptyRuns;
            const core::HorizontalAlign align = hasCell
                ? detail::horizontalAlign(block.cells[cell].align)
                : core::HorizontalAlign::Left;
            height = std::max(height, inlineRunsHeight(
                partId + ".cell." + std::to_string(cell) + ".text",
                runs,
                textWidth,
                style_.bodySize,
                style_.bodyLineHeight,
                block.tableHeader,
                align));
        }
        return height + style_.tableCellPadding * 2.0f;
    }

    void renderTableRow(const std::string& partId,
                        const detail::MarkdownBlock& block,
                        float contentWidth) {
        const std::size_t cellCount = std::max<std::size_t>(1, block.cells.size());
        const float cellWidth = contentWidth / static_cast<float>(cellCount);
        const float rowHeight = tableRowHeight(partId, block, contentWidth);
        const core::Color fill = block.tableHeader
            ? theme::withOpacity(style_.accent, 0.16f)
            : style_.quoteBackground;

        ui_.stack(partId)
            .width(contentWidth)
            .height(rowHeight)
            .content([&] {
                for (std::size_t cell = 0; cell < cellCount; ++cell) {
                    const std::string cellId = partId + ".cell." + std::to_string(cell);
                    ui_.stack(cellId)
                        .x(static_cast<float>(cell) * cellWidth)
                        .size(cellWidth, rowHeight)
                        .content([&] {
                            ui_.rect(cellId + ".bg")
                                .size(cellWidth, rowHeight)
                                .color(fill)
                                .border(1.0f, style_.divider)
                                .build();
                            if (cell < block.cells.size()) {
                                renderTableCell(cellId + ".text", block.cells[cell], block.tableHeader, cellWidth, rowHeight);
                            }
                        })
                        .build();
                }
            })
            .build();
    }

    void renderTableCell(const std::string& partId,
                         const detail::MarkdownTableCell& cell,
                         bool header,
                         float cellWidth,
                         float rowHeight) {
        const float textWidth = std::max(0.0f, cellWidth - style_.tableCellPadding * 2.0f);
        const core::HorizontalAlign align = detail::horizontalAlign(cell.align);
        const float textHeight = inlineRunsHeight(partId, cell.runs, textWidth, style_.bodySize, style_.bodyLineHeight, header, align);
        float textX = style_.tableCellPadding;
        if (cell.align == detail::MarkdownAlign::Center) {
            textX = style_.tableCellPadding;
        } else if (cell.align == detail::MarkdownAlign::Right) {
            textX = style_.tableCellPadding;
        }
        const float textY = std::max(0.0f, (rowHeight - textHeight) * 0.5f);

        renderInlineRuns(partId,
                         cell.runs,
                         textWidth,
                         textHeight,
                         style_.bodySize,
                         style_.bodyLineHeight,
                          header,
                          textX,
                          textY,
                         align);
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string markdown_;
    MarkdownStyle style_;
    float width_ = 420.0f;
    core::SizeValue height_ = core::SizeValue::wrapContent();
    core::EdgeInsets margin_;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool hasX_ = false;
    bool hasY_ = false;
    int zIndex_ = 0;
};

inline MarkdownBuilder markdown(core::dsl::Ui& ui, const std::string& id) {
    return MarkdownBuilder(ui, id);
}

} // namespace components
