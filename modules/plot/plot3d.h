#pragma once

#include "eui/dsl.h"
#include "modules/plot/export.h"
#include "modules/plot/renderer.h"
#include "modules/plot/scene3d.h"

namespace modules::plot {

struct Measurement3D {
    Vec3 from, to;
    double distance = 0;
};
/** Interactive scientific viewport. Left drag orbits, Shift drag pans, wheel zooms,
 * right click resets, click selects, Ctrl clicks measure. UI thread only.
 * releaseGpu() must precede window destruction; retained UI references retire through core.
 */
class Plot3D {
  public:
    explicit Plot3D(RenderSettings3D settings = {}, Budget gpuBudget = {});
    ~Plot3D();
    Plot3D(const Plot3D &) = delete;
    Plot3D &operator=(const Plot3D &) = delete;
    void setScene(Scene3D scene);
    const Scene3D &scene() const;
    void setView(ViewState3D view);
    const ViewState3D &view() const;
    void setSettings(RenderSettings3D settings);
    void setEnabled(bool enabled);
    void resetView();
    std::optional<Pick3D> selection() const;
    std::optional<Measurement3D> measurement() const;
    void compose(eui::Ui &ui, const std::string &id, float width, float height, double dpi = 1);
    Rendered3D render(std::uint32_t width, std::uint32_t height) const;
    /** Rerender at requested pixel size. 3D is an embedded raster layer, labels remain text. */
    ExportScene exportScene(std::uint32_t width, std::uint32_t height, double textScale = 1) const;
    void releaseGpu();

  private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace modules::plot
