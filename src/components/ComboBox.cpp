#include "ComboBox.h"

namespace EUINEO {

ComboBox::ComboBox(std::string placeholderText, float x, float y, float w, float h) 
    : placeholder(placeholderText) {
    this->x = x;
    this->y = y;
    this->width = w;
    this->height = h;
}

void ComboBox::AddItem(const std::string& item) {
    items.push_back(item);
    itemHoverAnims.push_back(0.0f);
}

void ComboBox::Update() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);

    bool hoveredMain = IsHovered();
    
    // 主框悬停动效
    float targetHover = hoveredMain ? 1.0f : 0.0f;
    if (hoverAnim != targetHover) {
        hoverAnim = Lerp(hoverAnim, targetHover, State.deltaTime * 15.0f);
        if (std::abs(hoverAnim - targetHover) < 0.01f) hoverAnim = targetHover;
        Renderer::RequestRepaint();
    }
    
    // 展开动效
    float targetOpen = isOpen ? 1.0f : 0.0f;
    if (openAnim != targetOpen) {
        openAnim = Lerp(openAnim, targetOpen, State.deltaTime * 20.0f);
        if (std::abs(openAnim - targetOpen) < 0.01f) openAnim = targetOpen;
        Renderer::RequestRepaint();
    }
    
    // 列表项悬停动效
    if (isOpen || openAnim > 0.0f) {
        float listY = absY + height;
        for (size_t i = 0; i < items.size(); ++i) {
            float itemY = listY + i * height;
            bool itemHovered = State.mouseX >= absX && State.mouseX <= absX + width &&
                               State.mouseY >= itemY && State.mouseY <= itemY + height;
            float targetItemHover = (itemHovered && isOpen) ? 1.0f : 0.0f;
            if (itemHoverAnims[i] != targetItemHover) {
                itemHoverAnims[i] = Lerp(itemHoverAnims[i], targetItemHover, State.deltaTime * 15.0f);
                if (std::abs(itemHoverAnims[i] - targetItemHover) < 0.01f) itemHoverAnims[i] = targetItemHover;
                Renderer::RequestRepaint();
            }
        }
    }
    
    if (State.mouseClicked) {
        if (isOpen) {
            // Check if clicked inside dropdown list
            float listY = absY + height;
            float listH = items.size() * height;
            bool hoveredList = State.mouseX >= absX && State.mouseX <= absX + width &&
                               State.mouseY >= listY && State.mouseY <= listY + listH;
                               
            if (hoveredList) {
                int index = (int)((State.mouseY - listY) / height);
                if (index >= 0 && index < items.size()) {
                    selectedIndex = index;
                    if (onSelectionChanged) onSelectionChanged(selectedIndex);
                }
            }
            isOpen = false; // Close on any click
            Renderer::RequestRepaint();
        } else if (hoveredMain) {
            isOpen = true;
            Renderer::RequestRepaint();
        }
    }
}

void ComboBox::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);

    // Background
    Color baseColor = Lerp(CurrentTheme->surface, CurrentTheme->surfaceActive, openAnim);
    Color hoverColor = Lerp(CurrentTheme->surfaceHover, CurrentTheme->surfaceActive, openAnim);
    Color bgColor = Lerp(baseColor, hoverColor, hoverAnim);
    
    Renderer::DrawRect(absX, absY, width, height, bgColor, 6.0f);
    
    // Border
    if (openAnim > 0.01f) {
        Renderer::DrawRect(absX, absY, width, 1.0f, CurrentTheme->border, 0.0f); 
        Color focusColor = CurrentTheme->primary;
        focusColor.a = openAnim;
        Renderer::DrawRect(absX, absY, width, 2.0f, focusColor, 0.0f); 
    } else {
        Renderer::DrawRect(absX, absY, width, 1.0f, CurrentTheme->border, 0.0f); // bottom border
    }
    
    // Text
    float textScale = fontSize / 24.0f;
    float textY = absY + height / 2.0f + (fontSize / 4.0f); // 垂直居中 baseline
    float textX = absX + 10.0f;
    
    std::string displayText = (selectedIndex >= 0 && selectedIndex < items.size()) ? items[selectedIndex] : placeholder;
    
    Color textColor = (selectedIndex >= 0) ? CurrentTheme->text : Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.5f);
    Renderer::DrawTextStr(displayText, textX, textY, textColor, textScale);
    
    // Arrow icon
    Renderer::DrawTextStr(isOpen ? "\xEF\x84\x86" : "\xEF\x84\x87", absX + width - 25.0f, textY, CurrentTheme->text, textScale); // 假设 font awesome 的上/下箭头 (fa-angle-up/down)
    
    // Draw dropdown list
    if (openAnim > 0.0f) {
        float listY = absY + height;
        float listH = items.size() * height * openAnim; // 高度展开动画
        
        // Background of list
        Renderer::DrawRect(absX, listY, width, listH, CurrentTheme->surface, 6.0f);
        
        // 使用一个剪裁区域避免文字溢出（极简实现中可以直接限制绘制的 alpha 或位置）
        for (size_t i = 0; i < items.size(); ++i) {
            float itemY = listY + i * height;
            // 改进的可见性判断：只有当整个 item 完全超出动画高度时才不绘制
            // 并在展开过程中对溢出部分的 item 进行透明度渐变，模拟丝滑展开
            float itemBottomY = itemY + height;
            float currentListBottomY = listY + listH;
            
            if (itemY > currentListBottomY) break; // 完全超出，不绘制
            
            // 计算当前 item 的局部透明度 (用于丝滑裁剪效果)
            float itemAlpha = 1.0f;
            if (itemBottomY > currentListBottomY) {
                // 部分超出，根据可见高度计算透明度
                float visibleHeight = currentListBottomY - itemY;
                itemAlpha = visibleHeight / height;
            }
            
            Color itemBgColor = Lerp(CurrentTheme->surface, CurrentTheme->surfaceHover, itemHoverAnims[i]);
            if (itemHoverAnims[i] > 0.01f) {
                itemBgColor.a *= itemAlpha;
                Renderer::DrawRect(absX, itemY, width, height, itemBgColor, 4.0f);
            }
            
            Color itemColor = (i == selectedIndex) ? CurrentTheme->primary : CurrentTheme->text;
            itemColor.a *= (openAnim * itemAlpha); // 文本结合展开动画和局部裁剪透明度
            Renderer::DrawTextStr(items[i], textX, itemY + height / 2.0f + (fontSize / 4.0f), itemColor, textScale);
        }
    }
}

} // namespace EUINEO