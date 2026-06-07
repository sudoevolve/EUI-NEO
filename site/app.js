const copy = {
  zh: {
    "nav.docs": "文档",
    "nav.rendering": "渲染",
    "nav.components": "组件",
    "nav.start": "开始",
    "hero.eyebrow": "C++17 · OpenGL / Vulkan · GLFW / SDL2",
    "hero.title": "EUI-NEO",
    "hero.lede": "一个面向高性能桌面工具、仪表盘和组件系统的轻量级 UI 框架。",
    "hero.docs": "查阅文档",
    "hero.github": "GitHub",
    "hero.qq": "加入 QQ 群",
    "hero.scroll": "查看文档索引",
    "stats.window": "窗口后端",
    "stats.render": "渲染后端",
    "stats.components": "组件",
    "why.eyebrow": "Why EUI-NEO",
    "why.title": "为 C++ 应用保留速度、控制力和现代 UI 体验。",
    "why.performance.title": "按需渲染",
    "why.performance.text": "静止时等待事件，有动画才推进帧；脏区缓存减少重复绘制。",
    "why.backends.title": "后端可选",
    "why.backends.text": "GLFW / SDL2 窗口后端，OpenGL / Vulkan 渲染后端，同一套 DSL 输出。",
    "why.cpp.title": "C++ 直写",
    "why.cpp.text": "无需引入 WebView 或脚本运行时，直接在 C++17 项目里声明界面和状态。",
    "why.components.title": "组件齐全",
    "why.components.text": "输入、弹层、选择器、表格、图表和滚动容器覆盖工具型应用常见场景。",
    "docs.eyebrow": "Documentation",
    "docs.title": "查找架构、组件和集成文档",
    "docs.lede": "按渲染、输入、组件、平台能力或构建流程快速定位仓库文档。",
    "docs.searchLabel": "搜索",
    "docs.searchPlaceholder": "搜索 DSL、Vulkan、IME、组件、布局...",
    "docs.empty": "没有匹配的文档。",
    "reader.loading": "正在读取文档...",
    "reader.error": "文档读取失败，请检查本地服务或路径。",
    "rendering.eyebrow": "Rendering Core",
    "rendering.title": "统一 Runtime，双渲染后端",
    "rendering.lede": "窗口、输入、Runtime 和 GPU 后端各守边界，让同一套 DSL 输出到 OpenGL 或 Vulkan。",
    "flow.compose.title": "Compose",
    "flow.compose.text": "C++ DSL 构建 UI 树，Runtime 负责布局、状态同步和交互派发。",
    "flow.dirty.title": "Dirty Rect",
    "flow.dirty.text": "按 id 缓存图元，变化时计算保守脏区，必要时升级 full redraw。",
    "flow.backend.title": "Backend",
    "flow.backend.text": "OpenGL 与 Vulkan 各自管理 pipeline、atlas、texture 和 frame lifecycle。",
    "components.eyebrow": "Component Layer",
    "components.title": "为工具型界面准备的组件层",
    "components.lede": "按钮、输入、弹层、选择器、图表和数据表都只组合 DSL 树，不穿透后端 primitive。",
    "start.eyebrow": "Quick Start",
    "start.title": "把 EUI-NEO 接入你的 CMake 项目",
    "start.cmake": "CMake 引入",
    "start.app": "实现入口",
    "start.build": "构建运行",
    "filter.all": "全部"
  },
  en: {
    "nav.docs": "Docs",
    "nav.rendering": "Rendering",
    "nav.components": "Components",
    "nav.start": "Start",
    "hero.eyebrow": "C++17 · OpenGL / Vulkan · GLFW / SDL2",
    "hero.title": "EUI-NEO",
    "hero.lede": "A lightweight UI framework for high-performance desktop tools, dashboards, and component systems.",
    "hero.docs": "Browse Docs",
    "hero.github": "GitHub",
    "hero.qq": "Join QQ Group",
    "hero.scroll": "Open the docs index",
    "stats.window": "window backends",
    "stats.render": "render backends",
    "stats.components": "components",
    "why.eyebrow": "Why EUI-NEO",
    "why.title": "Keep native C++ apps fast, controllable, and visually modern.",
    "why.performance.title": "On-demand rendering",
    "why.performance.text": "Idle apps wait for events; animated views advance frames only when needed, with dirty caching to reduce redraw work.",
    "why.backends.title": "Backend choice",
    "why.backends.text": "Use GLFW or SDL2 for windows, OpenGL or Vulkan for rendering, with one shared DSL.",
    "why.cpp.title": "Native C++ workflow",
    "why.cpp.text": "No WebView or scripting runtime. Declare UI and state directly inside a C++17 project.",
    "why.components.title": "Practical components",
    "why.components.text": "Inputs, popups, pickers, tables, charts, and scroll containers cover common tool-app workflows.",
    "docs.eyebrow": "Documentation",
    "docs.title": "Find architecture, component, and integration notes",
    "docs.lede": "Search rendering, input, components, platform capabilities, and build workflows directly from the repository docs.",
    "docs.searchLabel": "Search",
    "docs.searchPlaceholder": "Search DSL, Vulkan, IME, components, layout...",
    "docs.empty": "No matching documents.",
    "reader.loading": "Loading document...",
    "reader.error": "Could not load this document. Check the local server or path.",
    "rendering.eyebrow": "Rendering Core",
    "rendering.title": "One Runtime, two render backends",
    "rendering.lede": "Windowing, input, Runtime, and GPU backends stay separated while one DSL targets OpenGL or Vulkan.",
    "flow.compose.title": "Compose",
    "flow.compose.text": "The C++ DSL builds the UI tree while Runtime owns layout, state sync, and interaction dispatch.",
    "flow.dirty.title": "Dirty Rect",
    "flow.dirty.text": "Primitives are cached by id; visual changes produce conservative dirty regions or full redraws.",
    "flow.backend.title": "Backend",
    "flow.backend.text": "OpenGL and Vulkan manage their own pipelines, atlases, textures, and frame lifecycle.",
    "components.eyebrow": "Component Layer",
    "components.title": "A component layer for tool-grade interfaces",
    "components.lede": "Buttons, inputs, popups, pickers, charts, and data tables compose DSL trees without touching backend primitives.",
    "start.eyebrow": "Quick Start",
    "start.title": "Add EUI-NEO to your CMake project",
    "start.cmake": "Add CMake",
    "start.app": "Implement app",
    "start.build": "Build and run",
    "filter.all": "All"
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

const docs = [
  {
    category: "core",
    href: "../docs/DSL.md",
    zh: {
      title: "DSL 设计与当前实现",
      desc: "元素、布局属性、交互和 DSL app 入口。"
    },
    en: {
      title: "DSL Design",
      desc: "Elements, layout properties, interactions, and the DSL app entry."
    },
    tags: "dsl ui compose layout interaction"
  },
  {
    category: "components",
    href: "../docs/组件.md",
    zh: {
      title: "组件",
      desc: "组件命名、受控状态、输入框、选择器和图表。"
    },
    en: {
      title: "Components",
      desc: "Component naming, controlled state, inputs, pickers, and charts."
    },
    tags: "components button input chart picker datatable"
  },
  {
    category: "rendering",
    href: "../docs/渲染后端架构.md",
    zh: {
      title: "渲染后端架构与流程",
      desc: "GLFW/SDL2、OpenGL/Vulkan、Runtime 边界和渲染流程。"
    },
    en: {
      title: "Render Backend Architecture And Pipeline",
      desc: "Boundaries and pipeline across GLFW/SDL2, OpenGL/Vulkan, and Runtime."
    },
    tags: "opengl vulkan glfw sdl2 backend runtime render dirty rect cache blur fps"
  },
  {
    category: "rendering",
    href: "../docs/Slint渲染与优化对比.md",
    zh: {
      title: "Slint 渲染与优化对比",
      desc: "Renderer 选择、partial rendering 和 EUI-NEO 脏区优化。"
    },
    en: {
      title: "Slint Rendering Comparison",
      desc: "Renderer choices, partial rendering, and EUI-NEO dirty optimization."
    },
    tags: "slint renderer optimization dirty partial rendering"
  },
  {
    category: "platform",
    href: "../docs/事件.md",
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
    href: "../docs/布局.md",
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
    href: "../docs/动画.md",
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
    category: "platform",
    href: "../docs/平台能力.md",
    zh: {
      title: "平台能力",
      desc: "文件对话框、URL、托盘、剪贴板和窗口能力。"
    },
    en: {
      title: "Platform Capabilities",
      desc: "Dialogs, URLs, tray, clipboard, and window capabilities."
    },
    tags: "platform dialog tray clipboard window"
  },
  {
    category: "workflow",
    href: "../docs/集成指南.md",
    zh: {
      title: "集成指南",
      desc: "公共 facade、静态库、FetchContent 和嵌入式主循环。"
    },
    en: {
      title: "Integration Guide",
      desc: "Public facade, static library, FetchContent, and embedded loops."
    },
    tags: "integration cmake fetchcontent app"
  },
  {
    category: "workflow",
    href: "../docs/开发与发布.md",
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
    category: "platform",
    href: "../docs/网络.md",
    zh: {
      title: "网络",
      desc: "异步文本请求、图片请求和网络结果缓存。"
    },
    en: {
      title: "Network",
      desc: "Async text requests, image requests, and network result cache."
    },
    tags: "network async image request curl"
  }
];

const components = [
  "button",
  "input",
  "tabs",
  "dialog",
  "toast",
  "dropdown",
  "datatable",
  "linechart",
  "barchart",
  "piechart",
  "datepicker",
  "colorpicker",
  "scrollView",
  "carousel"
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
    observeReveal(card);
  });
}

async function openReader(doc) {
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
    readerBody.innerHTML = renderMarkdown(markdown);
  } catch (error) {
    readerBody.innerHTML = `<p>${escapeHtml(t("reader.error"))}</p>`;
  }
}

function closeReader() {
  reader.classList.remove("open");
  reader.setAttribute("aria-hidden", "true");
  document.body.classList.remove("reader-open");
}

function renderMarkdown(markdown) {
  const lines = markdown.replace(/\r\n/g, "\n").split("\n");
  let html = "";
  let inCode = false;
  let code = [];
  let listOpen = false;

  const closeList = () => {
    if (listOpen) {
      html += "</ul>";
      listOpen = false;
    }
  };

  for (const line of lines) {
    if (line.startsWith("```")) {
      if (inCode) {
        html += `<pre><code>${escapeHtml(code.join("\n"))}</code></pre>`;
        code = [];
        inCode = false;
      } else {
        closeList();
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
      continue;
    }
    const heading = line.match(/^(#{1,4})\s+(.+)$/);
    if (heading) {
      closeList();
      const level = heading[1].length;
      html += `<h${level}>${inlineMarkdown(heading[2])}</h${level}>`;
      continue;
    }
    const bullet = line.match(/^\s*[-*]\s+(.+)$/);
    if (bullet) {
      if (!listOpen) {
        html += "<ul>";
        listOpen = true;
      }
      html += `<li>${inlineMarkdown(bullet[1])}</li>`;
      continue;
    }
    closeList();
    html += `<p>${inlineMarkdown(line)}</p>`;
  }
  closeList();
  return html;
}

function inlineMarkdown(value) {
  return escapeHtml(value)
    .replace(/`([^`]+)`/g, "<code>$1</code>")
    .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
    .replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>');
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
  components.forEach((name) => {
    const item = document.createElement("span");
    item.className = "component-pill";
    item.textContent = name;
    list.appendChild(item);
  });
}

document.querySelectorAll(".lang-button").forEach((button) => {
  button.addEventListener("click", () => setLanguage(button.dataset.lang));
});

themeButton.addEventListener("click", () => {
  setTheme(currentTheme === "dark" ? "light" : "dark");
});

document.querySelectorAll("[data-close-reader]").forEach((node) => {
  node.addEventListener("click", closeReader);
});

window.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    closeReader();
  }
});

window.addEventListener("scroll", () => {
  const max = document.documentElement.scrollHeight - window.innerHeight;
  const progress = max > 0 ? window.scrollY / max : 0;
  progressBar.style.transform = `scaleX(${progress})`;
}, { passive: true });

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

function observeReveal(node) {
  node.classList.add("reveal");
  revealObserver.observe(node);
}

document.querySelectorAll("section, .doc-card, .flow-item").forEach((node) => {
  observeReveal(node);
});

searchInput.addEventListener("input", renderDocs);
renderComponents();
setTheme(currentTheme);
setLanguage(currentLang);
