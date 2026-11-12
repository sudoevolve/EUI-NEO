const copy = {
  zh: {
    "nav.docs": "文档",
    "nav.rendering": "渲染",
    "nav.components": "组件",
    "nav.start": "开始",
    "hero.eyebrow": "C++17 · OpenGL / Vulkan · GLFW / SDL2",
    "hero.title": "EUI-NEO",
    "hero.lede": "一个基于 C++17、支持 GLFW/SDL2 和 OpenGL/Vulkan 的跨平台高性能轻量级 UI 框架。",
    "hero.start": "开始接入",
    "hero.github": "GitHub",
    "hero.qq": "加入 QQ 群",
    "hero.scroll": "开始接入",
    "stats.window": "窗口后端",
    "stats.render": "渲染后端",
    "stats.components": "组件",
    "why.eyebrow": "Why EUI-NEO",
    "why.title": "为 C++ 应用保留速度、控制力和现代 UI 体验。",
    "why.performance.title": "按需渲染",
    "why.performance.text": "静止时等待事件，有动画才推进帧；脏区、framebuffer cache 和保留层缓存减少重复绘制。",
    "why.backends.title": "后端可选",
    "why.backends.text": "GLFW / SDL2 窗口后端，OpenGL / Vulkan 渲染后端，同一套 DSL 输出。",
    "why.cpp.title": "C++ 直写",
    "why.cpp.text": "无需引入 WebView 或脚本运行时，直接在 C++17 项目里声明界面和状态。",
    "why.components.title": "数据与交互",
    "why.components.text": "虚拟表格支持排序、筛选和列宽调整；实时曲线按绘图区降采样，控件支持键盘导航与焦点反馈。",
    "docs.eyebrow": "Documentation",
    "docs.title": "从接入到开发，按主题查阅",
    "docs.lede": "使用指南、架构和平台说明对应当前源码；已发布版本请查阅 Releases。",
    "docs.searchLabel": "搜索",
    "docs.searchPlaceholder": "搜索 DSL、Vulkan、IME、组件、布局...",
    "docs.empty": "没有匹配的文档。",
    "reader.loading": "正在读取文档...",
    "reader.error": "文档读取失败，请检查本地服务或路径。",
    "rendering.eyebrow": "Rendering Core",
    "rendering.title": "统一 Runtime，双渲染后端",
    "rendering.lede": "窗口、输入、Runtime 和 GPU 后端各守边界；dirty rect、render cache 和保留层缓存已覆盖 OpenGL / Vulkan。",
    "flow.compose.title": "Compose",
    "flow.compose.text": "C++ DSL 构建 UI 树，Runtime 负责布局、状态同步和交互派发。",
    "flow.dirty.title": "Dirty Rect",
    "flow.dirty.text": "按 id 保留图元，变化时合并脏区；背景模糊会将受影响的采样区域一起重绘。",
    "flow.backend.title": "Backend",
    "flow.backend.text": "OpenGL 与 Vulkan 各自管理 pipeline、atlas、texture、render cache、保留层和 frame lifecycle。",
    "components.eyebrow": "Component Layer",
    "components.title": "为工具型界面准备的组件层",
    "components.lede": "从长文本输入、虚拟列表到表格和实时曲线，复用同一套主题、键盘导航和控件语义。文件保存与目录选择调用系统对话框。",
    "start.eyebrow": "Quick Start",
    "start.title": "快速开始",
    "start.lede": "只需一个 CMakeLists.txt 和一个 main.cpp，CMake 会自动拉取 EUI-NEO。",
    "start.cmake": "CMakeLists.txt",
    "start.app": "main.cpp",
    "start.build": "bash",
    "start.copy": "复制",
    "start.copied": "已复制",
    "start.copyFailed": "复制失败",
    "start.guide": "安装与进阶接入",
    "filter.all": "全部",
    "footer.contributors": "贡献者",
    "footer.rights": " · Apache License 2.0"
  },
  en: {
    "nav.docs": "Docs",
    "nav.rendering": "Rendering",
    "nav.components": "Components",
    "nav.start": "Start",
    "hero.eyebrow": "C++17 · OpenGL / Vulkan · GLFW / SDL2",
    "hero.title": "EUI-NEO",
    "hero.lede": "A cross-platform, high-performance, low-overhead C++17 UI framework with GLFW/SDL2 window backends and OpenGL/Vulkan render backends.",
    "hero.start": "Get Started",
    "hero.github": "GitHub",
    "hero.qq": "Join QQ Group",
    "hero.scroll": "Get Started",
    "stats.window": "window backends",
    "stats.render": "render backends",
    "stats.components": "components",
    "why.eyebrow": "Why EUI-NEO",
    "why.title": "Keep native C++ apps fast, controllable, and visually modern.",
    "why.performance.title": "On-demand rendering",
    "why.performance.text": "Idle apps wait for events; animated views advance frames only when needed, while dirty regions, framebuffer cache, and retained layers reduce redraw work.",
    "why.backends.title": "Backend choice",
    "why.backends.text": "Use GLFW or SDL2 for windows, OpenGL or Vulkan for rendering, with one shared DSL.",
    "why.cpp.title": "Native C++ workflow",
    "why.cpp.text": "No WebView or scripting runtime. Declare UI and state directly inside a C++17 project.",
    "why.components.title": "Data and interaction",
    "why.components.text": "Virtual tables support sorting, filtering, and column resizing. Live charts downsample to the plot width, while controls provide keyboard navigation and focus feedback.",
    "docs.eyebrow": "Documentation",
    "docs.title": "Find your guide, from integration to development",
    "docs.lede": "Guides, architecture, and platform references follow the current source. See Releases for published versions.",
    "docs.searchLabel": "Search",
    "docs.searchPlaceholder": "Search DSL, Vulkan, IME, components, layout...",
    "docs.empty": "No matching documents.",
    "reader.loading": "Loading document...",
    "reader.error": "Could not load this document. Check the local server or path.",
    "rendering.eyebrow": "Rendering Core",
    "rendering.title": "One Runtime, two render backends",
    "rendering.lede": "Windowing, input, Runtime, and GPU backends stay separated; dirty rects, render cache, and retained layer cache now cover OpenGL and Vulkan.",
    "flow.compose.title": "Compose",
    "flow.compose.text": "The C++ DSL builds the UI tree while Runtime owns layout, state sync, and interaction dispatch.",
    "flow.dirty.title": "Dirty Rect",
    "flow.dirty.text": "Primitives are retained by id and changes merge dirty regions; backdrop blur also repaints affected sampling regions.",
    "flow.backend.title": "Backend",
    "flow.backend.text": "OpenGL and Vulkan manage their own pipelines, atlases, textures, render cache, retained layers, and frame lifecycle.",
    "components.eyebrow": "Component Layer",
    "components.title": "A component layer for tool-grade interfaces",
    "components.lede": "Text editors, virtual lists, tables, and live charts share themes, keyboard navigation, and control semantics. File saving and directory selection use system dialogs.",
    "start.eyebrow": "Quick Start",
    "start.title": "Start",
    "start.lede": "Create one CMakeLists.txt and one main.cpp; CMake fetches EUI-NEO automatically.",
    "start.cmake": "CMakeLists.txt",
    "start.app": "main.cpp",
    "start.build": "bash",
    "start.copy": "Copy",
    "start.copied": "Copied",
    "start.copyFailed": "Copy failed",
    "start.guide": "Install and integrate",
    "filter.all": "All",
    "footer.contributors": "Contributors",
    "footer.rights": " · Apache License 2.0"
  }
};

const categories = {
  zh: ["全部", "核心", "渲染", "组件", "平台", "流程"],
  en: ["All", "Core", "Rendering", "Components", "Platform", "Workflow"]
};

const categoryMap = {
  "全部": "all",
  "核心": "core",
  "渲染": "rendering",
  "组件": "components",
  "平台": "platform",
  "流程": "workflow",
  "All": "all",
  "Core": "core",
  "Rendering": "rendering",
  "Components": "components",
  "Platform": "platform",
  "Workflow": "workflow"
};

// Ordered from onboarding to internals: integrate first, then daily-use
// component/framework docs, then advanced DSL/rendering internals last.
const docs = [
  {
    category: "workflow",
    href: "../docs/README.md",
    zh: { title: "文档目录", desc: "按使用指南、架构、平台和开发流程查找现行文档。" },
    en: { title: "Documentation Index", desc: "Current guides, architecture, platform references, and development workflows." },
    tags: "documentation index guide 文档 目录"
  },
  {
    category: "workflow",
    href: "../docs/guides/integration.md",
    zh: {
      title: "集成指南",
      desc: "Quick Start、安装 SDK 和 FetchContent，从零接入 EUI-NEO。"
    },
    en: {
      title: "Integration Guide",
      desc: "Quick Start, SDK installation, and FetchContent to bring EUI-NEO into a project."
    },
    tags: "integration cmake sdk install find_package fetchcontent app quick start"
  },
  {
    category: "workflow",
    href: "../docs/development/build-and-release.md",
    zh: {
      title: "开发与发布",
      desc: "构建、验证、release 和项目维护流程。"
    },
    en: {
      title: "Development And Release",
      desc: "Builds, verification, releases, and maintenance workflow."
    },
    tags: "development release build verify"
  },
  {
    category: "components",
    href: "../docs/guides/components.md",
    zh: {
      title: "组件",
      desc: "长文本输入、虚拟表格、排序筛选、列宽调整和实时曲线。"
    },
    en: {
      title: "Components",
      desc: "Text editing, virtual tables, sorting, filtering, column resizing, and live charts."
    },
    tags: "components button input chart picker datatable markdown tooltip context menu mouse area signal virtual sort filter resize downsample 撤销 预算 表格 虚拟 排序 筛选 列宽 曲线 降采样"
  },
  {
    category: "components",
    href: "../docs/guides/keyboard-accessibility.md",
    zh: { title: "键盘与控件语义", desc: "Tab 导航、焦点反馈、弹层焦点恢复与语义快照。" },
    en: { title: "Keyboard and Control Semantics", desc: "Tab navigation, focus feedback, popup focus restoration, and semantic snapshots." },
    tags: "accessibility keyboard focus tab semantics 无障碍 键盘 焦点 语义"
  },
  {
    category: "components",
    href: "../docs/guides/markdown-viewport.md",
    zh: { title: "Markdown 视口", desc: "大文档只构建可见段落，保留全文高度与稳定标识。" },
    en: { title: "Markdown Viewport", desc: "Build visible content while preserving full document bounds and stable IDs." },
    tags: "markdown viewport virtual document 文档 视口 虚拟化"
  },
  {
    category: "core",
    href: "../docs/guides/layout.md",
    zh: {
      title: "布局",
      desc: "Row、Column、Stack、wrapContent 和 flex 行为。"
    },
    en: {
      title: "Layout",
      desc: "Row, Column, Stack, wrapContent, and flex behavior."
    },
    tags: "layout row column stack flex"
  },
  {
    category: "core",
    href: "../docs/guides/state.md",
    zh: {
      title: "状态模型",
      desc: "Runtime 状态、受控组件、Signal 绑定和生命周期。"
    },
    en: {
      title: "State Model",
      desc: "Runtime state, controlled components, Signal bindings, and lifecycle."
    },
    tags: "state signal controlled binding lifecycle"
  },
  {
    category: "core",
    href: "../docs/guides/animation.md",
    zh: {
      title: "动画",
      desc: "Transition、缓动、状态动画和 transform。"
    },
    en: {
      title: "Animation",
      desc: "Transitions, easing, state animation, and transforms."
    },
    tags: "animation transition easing transform"
  },
  {
    category: "core",
    href: "../docs/guides/images.md",
    zh: {
      title: "图片",
      desc: "图片源、远程图片、SVG、GIF 和纹理缓存。"
    },
    en: {
      title: "Images",
      desc: "Image sources, remote images, SVG, GIF, and texture cache."
    },
    tags: "image svg gif texture cache remote"
  },
  {
    category: "platform",
    href: "../docs/guides/events.md",
    zh: {
      title: "事件",
      desc: "输入队列、hover/press/click、文本输入和焦点。"
    },
    en: {
      title: "Events",
      desc: "Input queues, hover/press/click, text input, and focus."
    },
    tags: "events input ime focus mouse keyboard"
  },
  {
    category: "core",
    href: "../docs/guides/audio.md",
    zh: {
      title: "音频",
      desc: "miniaudio 播放器、资源路径、错误处理和配乐卡点同步。"
    },
    en: {
      title: "Audio",
      desc: "miniaudio playback, resource paths, errors, and music-synchronized animation."
    },
    tags: "audio miniaudio player music beat sync position duration mp3 wav flac ogg"
  },
  {
    category: "platform",
    href: "../docs/guides/network.md",
    zh: {
      title: "网络",
      desc: "异步文本请求、图片请求和网络结果缓存。"
    },
    en: {
      title: "Network",
      desc: "Async text requests, image requests, and network result cache."
    },
    tags: "network async image request curl"
  },
  {
    category: "platform",
    href: "../docs/guides/async.md",
    zh: {
      title: "异步",
      desc: "异步任务、请求和 Runtime 结果回收。"
    },
    en: {
      title: "Async",
      desc: "Async tasks, requests, and Runtime result collection."
    },
    tags: "async task request runtime"
  },
  {
    category: "platform",
    href: "../docs/guides/platform.md",
    zh: {
      title: "平台能力",
      desc: "文件打开与保存、目录选择、URL、托盘和窗口能力。"
    },
    en: {
      title: "Platform Capabilities",
      desc: "Open and save files, select directories, and use URLs, tray, and windows."
    },
    tags: "platform dialog save directory folder tray clipboard window 保存 目录 文件 对话框"
  },
  {
    category: "core",
    href: "../docs/guides/modules.md",
    zh: {
      title: "模块",
      desc: "键盘、串口等可选功能模块的构建、使用和开发约束。"
    },
    en: {
      title: "Modules",
      desc: "Build, usage, and development rules for optional modules such as keyboard and serial."
    },
    tags: "modules keyboard serial optional feature"
  },
  {
    category: "core",
    href: "../docs/guides/dsl.md",
    zh: {
      title: "DSL 设计与当前实现",
      desc: "进阶：元素、布局属性、交互和 DSL app 入口的底层设计。"
    },
    en: {
      title: "DSL Design",
      desc: "Advanced: elements, layout properties, interactions, and the DSL app entry internals."
    },
    tags: "dsl ui compose layout interaction advanced internal"
  },
  {
    category: "rendering",
    href: "../docs/architecture/rendering.md",
    zh: {
      title: "渲染后端架构与流程",
      desc: "GLFW/SDL2、OpenGL/Vulkan、Runtime 边界和渲染流程。"
    },
    en: {
      title: "Render Backend Architecture And Pipeline",
      desc: "Boundaries and pipeline across GLFW/SDL2, OpenGL/Vulkan, and Runtime."
    },
    tags: "opengl vulkan glfw sdl2 backend runtime render dirty rect cache retained layer blur fps"
  },
  {
    category: "rendering",
    href: "../docs/guides/dynamic-textures.md",
    zh: {
      title: "动态纹理与图像流",
      desc: "RGBA8/BGRA8、NV12、I420、P010 动态帧提交，以及 OpenGL/Vulkan 后端行为。"
    },
    en: {
      title: "Dynamic Textures And Image Streams",
      desc: "Dynamic frame submission for RGBA8/BGRA8, NV12, I420, and P010 across OpenGL/Vulkan."
    },
    tags: "dynamic texture image stream rgba8 bgra8 nv12 i420 p010 yuv video remote desktop opengl vulkan"
  },
  {
    category: "rendering",
    href: "../docs/architecture/retained-layers.md",
    zh: {
      title: "保留层缓存",
      desc: "稳定静态子树和相邻绘制段的离屏图层缓存，以及 OpenGL / Vulkan 后端资源和限制。"
    },
    en: {
      title: "Retained Layer Cache",
      desc: "Offscreen layer caching for stable static subtrees, with OpenGL / Vulkan backend resources and limits."
    },
    tags: "retained layer cache static subtree opengl vulkan framebuffer texture"
  },
  {
    category: "rendering",
    href: "../docs/guides/shadertoy.md",
    zh: {
      title: "Shadertoy 底层图元",
      desc: "Pass graph、通道、uniform、预设以及 OpenGL / Vulkan 双后端契约。"
    },
    en: {
      title: "Shadertoy Primitive",
      desc: "Pass graphs, channels, uniforms, presets, and the shared OpenGL / Vulkan contract."
    },
    tags: "shadertoy shader graph pass channel uniform opengl vulkan"
  },
  {
    category: "platform",
    href: "../docs/platform/linux-ime-cursor.md",
    zh: { title: "Linux IME", desc: "GLFW X11 输入法候选框定位、环境要求和排查方法。" },
    en: { title: "Linux IME", desc: "GLFW X11 candidate-window positioning, requirements, and troubleshooting." },
    tags: "linux x11 ime xim cursor input method 输入法 候选框"
  },
  {
    category: "workflow",
    href: "../docs/skills/eui-neo-ui-replicator/SKILL.md",
    zh: { title: "UI 设计技能", desc: "辅助生成与复刻界面的布局、主题和组件使用约定。" },
    en: { title: "UI Design Skill", desc: "Layout, theme, and component conventions for generating and replicating interfaces." },
    tags: "skill design replicate UI 设计 复刻"
  }
];

const components = [
  "button",
  "checkbox",
  "radio",
  "toggleSwitch",
  "progress",
  "slider",
  "input",
  "segmented",
  "stepper",
  "tabs",
  "scroll",
  "scrollView",
  "markdown",
  "dropdown",
  "datePicker",
  "timePicker",
  "colorPicker",
  "dataTable",
  "dialog",
  "sidebar",
  "toast",
  "tooltip",
  "contextMenu",
  "carousel",
  "lineChart",
  "barChart",
  "pieChart",
  "mouseArea",
  "panel",
  "card",
  "layoutDebugOverlay",
  "workshop::heartSwitch",
  "workshop::tiltCard"
];

let currentLang = localStorage.getItem("eui-site-lang") || "zh";
let currentTheme = localStorage.getItem("eui-site-theme") || "dark";
let activeCategory = "all";

const searchInput = document.querySelector("#docSearch");
const filters = document.querySelector("#docFilters");
const docsGrid = document.querySelector("#docsGrid");
const reader = document.querySelector("#docReader");
const readerTitle = document.querySelector("#readerTitle");
const readerCategory = document.querySelector("#readerCategory");
const readerBody = document.querySelector("#readerBody");
const themeButton = document.querySelector("#themeButton");
const progressBar = document.querySelector("#progressBar");
const stageSections = Array.from(document.querySelectorAll("main > section"));
let tickingScroll = false;

function t(key) {
  return copy[currentLang][key] || copy.zh[key] || key;
}

function setLanguage(lang) {
  currentLang = lang;
  localStorage.setItem("eui-site-lang", lang);
  document.documentElement.lang = lang === "zh" ? "zh-CN" : "en";
  document.querySelectorAll("[data-i18n]").forEach((node) => {
    node.textContent = t(node.dataset.i18n);
  });
  document.querySelectorAll("[data-i18n-placeholder]").forEach((node) => {
    node.placeholder = t(node.dataset.i18nPlaceholder);
  });
  document.querySelectorAll(".lang-button").forEach((button) => {
    button.classList.toggle("active", button.dataset.lang === lang);
  });
  renderFilters();
  renderDocs();
}

function setTheme(theme) {
  currentTheme = theme;
  document.documentElement.dataset.theme = theme;
  localStorage.setItem("eui-site-theme", theme);
  themeButton.textContent = theme === "dark" ? "☼" : "◐";
}

function renderFilters() {
  filters.innerHTML = "";
  categories[currentLang].forEach((label) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "filter-button";
    button.textContent = label;
    const value = categoryMap[label];
    button.classList.toggle("active", value === activeCategory);
    button.addEventListener("click", () => {
      activeCategory = value;
      renderFilters();
      renderDocs();
    });
    filters.appendChild(button);
  });
}

function docMatches(doc, query) {
  if (activeCategory !== "all" && doc.category !== activeCategory) {
    return false;
  }
  if (!query) {
    return true;
  }
  const localized = doc[currentLang];
  const haystack = `${localized.title} ${localized.desc} ${doc.tags}`.toLowerCase();
  return haystack.includes(query);
}

function renderDocs() {
  const query = searchInput.value.trim().toLowerCase();
  const matched = docs.filter((doc) => docMatches(doc, query));
  docsGrid.innerHTML = "";
  if (!matched.length) {
    const empty = document.createElement("div");
    empty.className = "empty-state";
    empty.textContent = t("docs.empty");
    docsGrid.appendChild(empty);
    return;
  }
  matched.forEach((doc, index) => {
    const localized = doc[currentLang];
    const card = document.createElement("a");
    card.className = "doc-card";
    card.href = doc.href;
    card.addEventListener("click", (event) => {
      event.preventDefault();
      openReader(doc);
    });
    card.innerHTML = `
      <span class="doc-number">${String(index + 1).padStart(2, "0")}</span>
      <span class="doc-category">${categoryLabel(doc.category)}</span>
      <div class="doc-copy">
        <h3>${escapeHtml(localized.title)}</h3>
        <p>${escapeHtml(localized.desc)}</p>
      </div>
      <span class="doc-arrow" aria-hidden="true">›</span>
    `;
    docsGrid.appendChild(card);
    observeReveal(card, "doc", index);
  });
}

async function openReader(doc, fragment = "") {
  const localized = doc[currentLang];
  readerTitle.textContent = localized.title;
  readerCategory.textContent = categoryLabel(doc.category);
  readerBody.innerHTML = `<p>${escapeHtml(t("reader.loading"))}</p>`;
  reader.classList.add("open");
  reader.setAttribute("aria-hidden", "false");
  document.body.classList.add("reader-open");
  try {
    const response = await fetch(doc.href);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const markdown = await response.text();
    const content = doc.section
      ? extractMarkdownSection(markdown, doc.section)
      : markdown;
    readerBody.innerHTML = renderMarkdown(content, doc.href);
    readerBody.scrollTop = 0;
    if (fragment) {
      const heading = readerBody.querySelector(`[id="${CSS.escape(decodeURIComponent(fragment.slice(1)))}"]`);
      if (heading) heading.scrollIntoView({ block: "start" });
    }
  } catch (error) {
    readerBody.innerHTML = `<p>${escapeHtml(t("reader.error"))}</p>`;
  }
}

function extractMarkdownSection(markdown, heading) {
  const lines = markdown.replace(/\r\n/g, "\n").split("\n");
  const start = lines.findIndex((line) => line.trim() === `## ${heading}`);
  if (start < 0) {
    return markdown;
  }
  const end = lines.findIndex((line, index) =>
    index > start && /^##\s+/.test(line)
  );
  return lines.slice(start, end < 0 ? undefined : end).join("\n");
}

function closeReader() {
  reader.classList.remove("open");
  reader.setAttribute("aria-hidden", "true");
  document.body.classList.remove("reader-open");
}

function renderMarkdown(markdown, sourceHref) {
  const lines = markdown.replace(/\r\n/g, "\n").split("\n");
  let html = "";
  let inCode = false;
  let code = [];
  let listOpen = false;
  let inTable = false;
  const headingIds = new Map();

  const closeList = () => {
    if (listOpen) {
      html += "</ul>";
      listOpen = false;
    }
  };
  const closeTable = () => {
    if (inTable) {
      html += "</tbody></table></div>";
      inTable = false;
    }
  };

  for (let index = 0; index < lines.length; index += 1) {
    const line = lines[index];
    if (line.startsWith("```")) {
      if (inCode) {
        html += `<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`;
        code = [];
        inCode = false;
      } else {
        closeList();
        closeTable();
        inCode = true;
      }
      continue;
    }
    if (inCode) {
      code.push(line);
      continue;
    }
    if (!line.trim()) {
      closeList();
      closeTable();
      continue;
    }
    if (!inTable && isMarkdownTableHeader(line, lines[index + 1] || "")) {
      closeList();
      const headers = splitMarkdownTableRow(line);
      html += "<div class=\"table-scroll\"><table><thead><tr>";
      html += headers.map((cell) => `<th>${inlineMarkdown(cell, sourceHref)}</th>`).join("");
      html += "</tr></thead><tbody>";
      inTable = true;
      index += 1;
      continue;
    }
    if (inTable && isMarkdownTableRow(line)) {
      const cells = splitMarkdownTableRow(line);
      html += "<tr>";
      html += cells.map((cell) => `<td>${inlineMarkdown(cell, sourceHref)}</td>`).join("");
      html += "</tr>";
      continue;
    }
    closeTable();
    const heading = line.match(/^(#{1,4})\s+(.+)$/);
    if (heading) {
      closeList();
      const level = heading[1].length;
      const slug = heading[2].replace(/`/g, "").toLowerCase().replace(/[^\p{L}\p{N}_ -]/gu, "").replace(/ /g, "-");
      const count = headingIds.get(slug) || 0;
      headingIds.set(slug, count + 1);
      const id = count ? `${slug}-${count}` : slug;
      html += `<h${level} id="${escapeHtml(id)}">${inlineMarkdown(heading[2], sourceHref)}</h${level}>`;
      continue;
    }
    const bullet = line.match(/^\s*[-*]\s+(.+)$/);
    if (bullet) {
      if (!listOpen) {
        html += "<ul>";
        listOpen = true;
      }
      html += `<li>${inlineMarkdown(bullet[1], sourceHref)}</li>`;
      continue;
    }
    closeList();
    html += `<p>${inlineMarkdown(line, sourceHref)}</p>`;
  }
  closeList();
  closeTable();
  return html;
}

function isMarkdownTableHeader(line, separator) {
  return isMarkdownTableRow(line) && /^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?\s*$/.test(separator);
}

function isMarkdownTableRow(line) {
  const trimmed = line.trim();
  return trimmed.includes("|") && trimmed.split("|").length >= 3;
}

function splitMarkdownTableRow(line) {
  let trimmed = line.trim();
  if (trimmed.startsWith("|")) {
    trimmed = trimmed.slice(1);
  }
  if (trimmed.endsWith("|")) {
    trimmed = trimmed.slice(0, -1);
  }
  return trimmed.split("|").map((cell) => cell.trim());
}

function inlineMarkdown(value, sourceHref) {
  return escapeHtml(value)
    .replace(/`([^`]+)`/g, "<code>$1</code>")
    .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
    .replace(/\[([^\]]+)\]\(([^)]+)\)/g, (_, label, href) =>
      `<a href="${escapeHtml(resolveMarkdownHref(href, sourceHref))}">${label}</a>`);
}

function resolveMarkdownHref(href, sourceHref) {
  try {
    const sourceUrl = new URL(sourceHref, window.location.href);
    const target = new URL(href, sourceUrl);
    const docsAt = sourceUrl.pathname.indexOf("/docs/");
    const basePath = sourceUrl.pathname.slice(0, docsAt + 1);
    if (docsAt >= 0 && target.origin === sourceUrl.origin && target.pathname.startsWith(basePath)) {
      const path = target.pathname.slice(basePath.length);
      // Source examples and repository files are not part of the Pages artifact.
      if (!path.startsWith("docs/") && !path.startsWith("assets/")) {
        return `https://github.com/sudoevolve/EUI-NEO/blob/main/${path}${target.hash}`;
      }
    }
    return target.href;
  } catch (error) {
    return href;
  }
}

function findDocumentByHref(href) {
  const targetUrl = new URL(href, window.location.href);
  targetUrl.hash = "";
  return docs.find((doc) =>
    new URL(doc.href, window.location.href).href === targetUrl.href);
}

function categoryLabel(value) {
  const list = categories[currentLang];
  return list.find((label) => categoryMap[label] === value) || value;
}

function escapeHtml(value) {
  return value.replace(/[&<>"']/g, (character) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    "\"": "&quot;",
    "'": "&#039;"
  }[character]));
}

function renderComponents() {
  const list = document.querySelector("#componentList");
  list.innerHTML = "";
  components.forEach((name, index) => {
    const item = document.createElement("span");
    item.className = "component-pill";
    item.textContent = name;
    list.appendChild(item);
    observeReveal(item, "component", index);
  });
}

const CONTRIBUTORS_ENDPOINT =
  "https://api.github.com/repos/sudoevolve/EUI-NEO/contributors?per_page=40&anon=false";
const CONTRIBUTORS_CACHE_KEY = "eui-site-contributors";
const CONTRIBUTORS_CACHE_TTL = 6 * 60 * 60 * 1000;

function readContributorCache() {
  try {
    const raw = localStorage.getItem(CONTRIBUTORS_CACHE_KEY);
    if (!raw) {
      return null;
    }
    const parsed = JSON.parse(raw);
    if (!parsed || !Array.isArray(parsed.list) || Date.now() - parsed.time > CONTRIBUTORS_CACHE_TTL) {
      return null;
    }
    return parsed.list;
  } catch (error) {
    return null;
  }
}

function writeContributorCache(list) {
  try {
    localStorage.setItem(CONTRIBUTORS_CACHE_KEY, JSON.stringify({ time: Date.now(), list }));
  } catch (error) {
    // Private mode or quota limits should not block rendering.
  }
}

function renderContributors(list) {
  const track = document.querySelector("#contributorTrack");
  const wrapper = document.querySelector(".footer-contributors");
  if (!track || !wrapper) {
    return;
  }
  if (!list.length) {
    wrapper.classList.add("is-empty");
    return;
  }
  const markup = list
    .map(
      (person) =>
        `<a class="contributor" href="${person.url}" target="_blank" rel="noopener">` +
        `<img src="${person.avatar}" alt="" loading="lazy" width="26" height="26">` +
        `<span>${escapeHtml(person.login)}</span></a>`
    )
    .join("");
  // Duplicate the list once so the marquee loop has no visible seam.
  track.innerHTML = markup + markup;
}

async function loadContributors() {
  const cached = readContributorCache();
  if (cached) {
    renderContributors(cached);
    return;
  }
  try {
    const response = await fetch(CONTRIBUTORS_ENDPOINT, {
      headers: { Accept: "application/vnd.github+json" }
    });
    if (!response.ok) {
      throw new Error(String(response.status));
    }
    const data = await response.json();
    const list = data
      .filter((entry) => entry && entry.login && entry.type !== "Bot")
      .map((entry) => ({ login: entry.login, avatar: `${entry.avatar_url}&s=64`, url: entry.html_url }));
    writeContributorCache(list);
    renderContributors(list);
  } catch (error) {
    document.querySelector(".footer-contributors")?.classList.add("is-empty");
  }
}

document.querySelectorAll(".lang-button").forEach((button) => {
  button.addEventListener("click", () => setLanguage(button.dataset.lang));
});

themeButton.addEventListener("click", () => {
  setTheme(currentTheme === "dark" ? "light" : "dark");
});

document.querySelectorAll(".start-links a").forEach((link) => {
  link.addEventListener("click", (event) => {
    const doc = findDocumentByHref(link.href);
    if (!doc) {
      return;
    }
    event.preventDefault();
    openReader(doc, new URL(link.href).hash);
  });
});

document.querySelectorAll(".code-copy").forEach((button) => {
  button.addEventListener("click", async () => {
    const code = button.parentElement.querySelector("code");
    if (!code) {
      return;
    }
    try {
      await copyText(code.textContent);
      button.textContent = t("start.copied");
      window.setTimeout(() => {
        button.textContent = t("start.copy");
      }, 1400);
    } catch (error) {
      button.textContent = t("start.copyFailed");
      window.setTimeout(() => {
        button.textContent = t("start.copy");
      }, 1800);
    }
  });
});

async function copyText(value) {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(value);
    return;
  }
  const textarea = document.createElement("textarea");
  textarea.value = value;
  textarea.setAttribute("readonly", "");
  textarea.style.position = "fixed";
  textarea.style.opacity = "0";
  document.body.appendChild(textarea);
  textarea.select();
  const copied = document.execCommand("copy");
  textarea.remove();
  if (!copied) {
    throw new Error("Clipboard unavailable");
  }
}

document.querySelectorAll("[data-close-reader]").forEach((node) => {
  node.addEventListener("click", closeReader);
});

readerBody.addEventListener("click", (event) => {
  const link = event.target.closest("a");
  if (!link) {
    return;
  }
  const doc = findDocumentByHref(link.href);
  if (doc) {
    event.preventDefault();
    openReader(doc, new URL(link.href).hash);
  }
});

window.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    closeReader();
  }
});

function updateScrollState() {
  const max = document.documentElement.scrollHeight - window.innerHeight;
  const progress = max > 0 ? window.scrollY / max : 0;
  const breath = (Math.sin(progress * Math.PI * 2.8) + 1) * 0.5;
  const activeIndex = activeStageIndex();

  progressBar.style.transform = `scaleX(${progress})`;
  document.documentElement.style.setProperty("--scroll-progress", (0.72 + progress * 0.28).toFixed(4));
  document.documentElement.style.setProperty("--breath", breath.toFixed(4));
  document.documentElement.style.setProperty("--stage", String(activeIndex));
  document.documentElement.style.setProperty("--breath-shift", `${(breath * 10).toFixed(2)}px`);
  document.documentElement.style.setProperty("--breath-shift-neg", `${(breath * -10).toFixed(2)}px`);
  document.documentElement.style.setProperty("--breath-small-up", `${(breath * -4).toFixed(2)}px`);
  document.documentElement.style.setProperty("--breath-small-down", `${(breath * 3).toFixed(2)}px`);
  document.documentElement.style.setProperty("--breath-scale", (1 + breath * 0.012).toFixed(4));
  document.documentElement.style.setProperty("--breath-grid-opacity", (0.48 + breath * 0.22).toFixed(4));
  document.documentElement.style.setProperty("--breath-glow", `${(12 + breath * 20).toFixed(2)}px`);
  document.documentElement.style.setProperty("--stage-primary-mix", `${(22 + breath * 34).toFixed(2)}%`);

  stageSections.forEach((section, index) => {
    section.classList.toggle("active-stage", index === activeIndex);
  });
  tickingScroll = false;
}

function requestScrollState() {
  if (tickingScroll) {
    return;
  }
  tickingScroll = true;
  window.requestAnimationFrame(updateScrollState);
}

function activeStageIndex() {
  const probe = window.innerHeight * 0.42;
  let bestIndex = 0;
  let bestDistance = Number.POSITIVE_INFINITY;
  stageSections.forEach((section, index) => {
    const rect = section.getBoundingClientRect();
    const distance = Math.abs(rect.top + rect.height * 0.28 - probe);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestIndex = index;
    }
  });
  return bestIndex;
}

window.addEventListener("scroll", requestScrollState, { passive: true });
window.addEventListener("resize", requestScrollState, { passive: true });

window.addEventListener("pointermove", (event) => {
  const x = (event.clientX / window.innerWidth - 0.5) * 18;
  const y = (event.clientY / window.innerHeight - 0.5) * 18;
  document.documentElement.style.setProperty("--mx", `${x}px`);
  document.documentElement.style.setProperty("--my", `${y}px`);
}, { passive: true });

const revealObserver = new IntersectionObserver((entries) => {
  entries.forEach((entry) => {
    if (entry.isIntersecting) {
      entry.target.classList.add("revealed");
    }
  });
}, { threshold: 0.14 });

function revealKindForSection(node) {
  if (node.classList.contains("hero") || node.classList.contains("manifest")) {
    return "hero";
  }
  if (node.classList.contains("doc-search")) {
    return "docs";
  }
  if (node.classList.contains("rendering")) {
    return "render";
  }
  if (node.classList.contains("showcase")) {
    return "showcase";
  }
  if (node.classList.contains("components-band")) {
    return "components";
  }
  if (node.classList.contains("start")) {
    return "build";
  }
  return "hero";
}

function observeReveal(node, kind = "section", index = 0) {
  node.classList.add("reveal");
  node.style.setProperty("--reveal-index", String(Math.min(index, 18)));
  const revealKind = kind === "section" ? revealKindForSection(node) : kind;
  node.classList.add(`reveal-stage-${revealKind}`);
  revealObserver.observe(node);
}

document.querySelectorAll("main > section").forEach((node, index) => {
  observeReveal(node, "section", index);
});
document.querySelectorAll(".flow-item").forEach((node, index) => {
  observeReveal(node, "flow", index);
});
document.querySelectorAll(".start-step").forEach((node, index) => {
  observeReveal(node, "code", index);
});

document.querySelectorAll("[data-start-tab]").forEach((tab) => {
  tab.addEventListener("click", () => {
    const selected = tab.dataset.startTab;
    document.querySelectorAll("[data-start-tab]").forEach((item) => {
      const active = item === tab;
      item.classList.toggle("active", active);
      item.setAttribute("aria-selected", active ? "true" : "false");
    });
    document.querySelectorAll("[data-start-panel]").forEach((panel) => {
      const active = panel.dataset.startPanel === selected;
      panel.classList.toggle("active", active);
      panel.hidden = !active;
    });
  });
});

searchInput.addEventListener("input", renderDocs);
renderComponents();
setTheme(currentTheme);
setLanguage(currentLang);
updateScrollState();

const footerYear = document.querySelector("#footerYear");
if (footerYear) {
  footerYear.textContent = String(new Date().getFullYear());
}
loadContributors();
