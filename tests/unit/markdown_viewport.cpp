#include "components/markdown.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>

namespace {
using Element = core::dsl::Element;
struct Snapshot {
    float height = 0, originY = 0;
    std::size_t nodes = 0;
    std::map<std::string, Element> leaves;
};
void visit(const Element& element, Snapshot& out, bool oracle) {
    ++out.nodes;
    if (oracle && (element.kind == core::dsl::ElementKind::Text || element.kind == core::dsl::ElementKind::Rect)) {
        // Element owns unique children; only copy the fields relevant to a leaf oracle.
        Element leaf;
        leaf.kind = element.kind;
        leaf.text = element.text;
        leaf.frame = element.frame;
        leaf.fontSize = element.fontSize;
        leaf.fontFamily = element.fontFamily;
        leaf.fontWeight = element.fontWeight;
        leaf.lineHeight = element.lineHeight;
        out.leaves.emplace(element.id, std::move(leaf));
    }
    for (const auto& child : element.children) visit(*child, out, oracle);
}
Snapshot build(const std::string& text, float width, const components::MarkdownStyle& style, bool sliced, float top,
               float height, bool oracle = true) {
    core::dsl::Ui ui;
    ui.begin("viewport-oracle");
    auto md = components::markdown(ui, "md").position(11, 7).margin(3, 5, 7, 9).width(width).style(style).markdown(text);
    if (sliced) md.viewport(top, height);
    md.build();
    ui.end();
    ui.layout(width + 22, 0);
    Snapshot result;
    result.height = ui.find("md")->frame.height;
    result.originY = ui.find("md")->frame.y;
    visit(*ui.find("md"), result, oracle);
    return result;
}
bool same(float a, float b) { return std::fabs(a - b) < 0.02f; }
bool compare(const Snapshot& full, const Snapshot& slice, float top, float height) {
    if (!same(full.height, slice.height)) {
        std::cerr << "full height changed\n";
        return false;
    }
    for (const auto& pair : full.leaves) {
        const auto& a = pair.second;
        if (a.frame.y + a.frame.height < top + full.originY || a.frame.y > top + height + full.originY) continue;
        const auto found = slice.leaves.find(pair.first);
        if (found == slice.leaves.end()) {
            std::cerr << "missing visible leaf " << pair.first << '\n';
            return false;
        }
        const auto& b = found->second;
        if (a.text != b.text || a.fontFamily != b.fontFamily || a.fontWeight != b.fontWeight ||
            !same(a.fontSize, b.fontSize) || !same(a.lineHeight, b.lineHeight) || !same(a.frame.x, b.frame.x) ||
            !same(a.frame.y, b.frame.y) || !same(a.frame.width, b.frame.width) ||
            !same(a.frame.height, b.frame.height)) {
            std::cerr << "visible geometry changed " << pair.first << '\n';
            return false;
        }
    }
    return true;
}
} // namespace
int main(int argc, char** argv) {
    const auto source = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const char* font = std::getenv("EUI_TEXT_TEST_FONT");
    core::TextPrimitive::setDefaultFontFiles(
        font ? font : (source / "assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf").string(), {});
    const std::string mixed =
        "# 标题 Latin 😀\n\n普通 **bold** _emphasis_ `code` [链接](https://example.com) ~~delete~~\n\n"
        "> 引用中文和 English，第二行内容继续。\n\n- 列表\n- [x] task\n\n---\n\n```cpp\nint main() {\n  return "
        "0;\n}\n```\n\n"
        "| A | 中文标题 |\n| :- | -: |\n| first | 正文 |\n| long long long value | 另一个值 |\n\n尾段。";
    for (const bool dark : {false, true})
        for (const float width : {190.25f, 520.0f}) {
            components::MarkdownStyle style(dark ? components::theme::dark() : components::theme::light());
            style.bodySize = dark ? 21.25f : 16.0f;
            style.bodyLineHeight = dark ? 29.0f : 24.0f;
            const auto full = build(mixed, width, style, false, 0, 0);
            for (float top = 0; top < full.height; top += 57.5f)
                if (!compare(full, build(mixed, width, style, true, top, 93.25f), top, 93.25f)) return 1;
            if (build(mixed, width, style, true, 0, 0).nodes != 1) return 1;
            if (build(mixed, width, style, true, full.height + 100, 60).nodes != 1) return 1;
        }
    components::MarkdownStyle style;
    std::string paragraph;
    while (paragraph.size() < 18000) paragraph += "中文 English 😀 e\xcc\x81 ";
    const auto full = build(paragraph, 420, style, false, 0, 0);
    for (float top : {0.0f, full.height / 2, full.height - 200}) {
        const auto slice = build(paragraph, 420, style, true, top, 180);
        if (!compare(full, slice, top, 180) || slice.nodes >= full.nodes / 4) return 1;
    }
    std::string table = "| left | right |\n| :- | -: |\n";
    for (int i = 0; i < 100; ++i) table += "| 中文 " + std::to_string(i) + " | **value** |\n";
    const auto fullTable = build(table, 420, style, false, 0, 0);
    const auto tableSlice = build(table, 420, style, true, fullTable.height / 2, 180);
    if (!compare(fullTable, tableSlice, fullTable.height / 2, 180) || tableSlice.nodes >= fullTable.nodes / 4) return 1;
    // Source edits/replacements, style/width and font configuration must invalidate.
    for (int i = 0; i < 12; ++i) {
        auto changed = mixed + std::to_string(i) + "\n\n" + paragraph.substr(0, 200 + i * 10);
        style.bodySize += 0.25f;
        style.codeSize += 0.5f;
        style.blockGap += 0.25f;
        const auto expected = build(changed, 230 + i, style, false, 0, 0);
        if (!compare(expected, build(changed, 230 + i, style, true, 80, 170), 80, 170)) return 1;
    }
    const auto revision = core::TextPrimitive::fontConfigurationRevision();
    core::TextPrimitive::setDefaultFontFiles({}, {});
    if (core::TextPrimitive::fontConfigurationRevision() == revision) return 1;
    const auto switched = build(mixed, 300, style, false, 0, 0);
    if (!compare(switched, build(mixed, 300, style, true, 0, 250), 0, 250)) return 1;
    if (argc > 1 && std::string(argv[1]) == "--benchmark") {
        core::TextPrimitive::setDefaultFontFiles(
            font ? font : (source / "assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf").string(), {});
        for (int scenario = 0; scenario < 3; ++scenario) {
            const bool slice = scenario != 0;
            std::vector<double> times;
            for (int i = 0; i < 65; ++i) {
                const auto text = scenario == 2 ? paragraph + std::to_string(i) : paragraph;
                const auto start = std::chrono::steady_clock::now();
                const auto result = build(text, 420, style, slice, 500, 180, false);
                if (i >= 5)
                    times.push_back(
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
                if (i == 64) std::cout << "{\"viewport\":" << slice << ",\"append\":" << (scenario == 2) << ",\"nodes\":" << result.nodes << ',';
            }
            std::sort(times.begin(), times.end());
            std::cout << "\"p50_ms\":" << times[30] << ",\"p95_ms\":" << times[57] << "}\n";
        }
    }
    std::cout << "Markdown viewport geometry, stable IDs and bounded construction passed\n";
}
