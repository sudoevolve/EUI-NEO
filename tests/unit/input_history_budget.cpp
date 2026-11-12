#include "components/input_model.h"

int main() {
    using Model = components::input_detail::InputModel;
    Model::InputState state;
    state.undoByteBudget = 4096;
    state.text = std::string(1000, 'a');
    for (int i = 0; i < 128; ++i) {
        Model::pushUndoState(state);
        state.text[0] = static_cast<char>('a' + i % 26);
        if (Model::historyBytes(state) > state.undoByteBudget) return 1;
    }
    const auto last = state.text;
    if (!Model::undoEdit(state) || !Model::redoEdit(state) || state.text != last) return 2;
    if (Model::historyBytes(state) > state.undoByteBudget) return 3;
    state.undoByteBudget = 0;
    Model::trimHistory(state);
    Model::pushUndoState(state);
    if (!state.undoStack.empty() || !state.redoStack.empty()) return 4;
    state.undoByteBudget = 64;
    Model::pushUndoState(state);
    if (!state.undoStack.empty()) return 5;
    state.undoByteBudget = 4096;
    state.text = "你好";
    state.cursor = static_cast<int>(state.text.size());
    Model::pushUndoState(state);
    state.text += "世界";
    if (!Model::undoEdit(state) || state.text != "你好") return 6;
    if (!Model::redoEdit(state) || state.text != "你好世界") return 7;
    return 0;
}
