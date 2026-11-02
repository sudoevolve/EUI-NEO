#include "modules/plot/axes.h"
#include "modules/plot/field.h"
#include "modules/plot/figure.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance = 1e-10) {
    require(std::abs(actual - expected) <= tolerance, "numeric mismatch");
}
template <class Exception, class Function> void throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("expected exception");
}
} // namespace

int main() {
    using namespace modules::plot;
    try {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        Data empty;
        require(empty.size() == 0 && empty.blockCount() == 0, "empty data");
        throws<std::invalid_argument>([] { Data({1}, {}); });
        throws<std::out_of_range>([&] { empty.at(0); });
        std::vector<double> values(Data::blockCapacity * 3 + 5, 2);
        Data original(values, values);
        Data edited = original.replace(Data::blockCapacity - 1, {3, 4}, {5, nan});
        require(original.at(Data::blockCapacity).x == 2, "snapshot was mutated");
        require(edited.at(Data::blockCapacity).x == 4, "cross-block edit");
        require(std::isnan(edited.points()[Data::blockCapacity].y), "missing data export");
        require(original.blockIdentity(2) == edited.blockIdentity(2), "unmodified block copied");
        require(original.blockIdentity(0) != edited.blockIdentity(0), "modified block shared");
        const auto appended = edited.append({9}, {10});
        require(appended.blockIdentity(2) == edited.blockIdentity(2), "append copied full block");
        require(appended.at(appended.size() - 1).y == 10, "append lost sample");
        require(appended.revision() == original.revision() + 2, "revision");
        throws<std::out_of_range>([&] { original.replace(original.size(), {1}, {2}); });

        Axes axes;
        axes.fit({Data({0, 0.1, 3, 1, nan}, {-1, 2, 0, -3, 1e99})});
        near(axes.x.range().max, 3);
        near(axes.y.range().min, -3);
        near(axes.y.range().max, 2);
        const Viewport viewport{30, 40, 500, 200};
        const Point p{0.1, -1};
        const auto screen = axes.toScreen(p, viewport);
        require(screen.has_value(), "screen mapping");
        const auto back = axes.toData(*screen, viewport);
        near(back->x, p.x);
        near(back->y, p.y);
        axes.x.setReversed(true);
        near(axes.toScreen({3, 2}, viewport)->x, 30);
        near(axes.toScreen({3, 2}, viewport)->y, 40);
        require(!axes.toScreen({nan, 1}, viewport), "NaN mapped");
        require(!axes.toData(p, {0, 0, 0, 1}), "zero viewport mapped");
        axes.x.setReversed(false);
        axes.x.setRange({1e12, 1e12 + 0.01});
        const double midpoint = 1e12 + 0.005;
        near(*axes.x.denormalize(*axes.x.normalize(midpoint)), midpoint, 0);
        axes.x.setScale(Scale::Log10);
        near(*axes.x.denormalize(*axes.x.normalize(midpoint)), midpoint, 0);
        require(!axes.x.normalize(0) && !axes.x.normalize(-1), "nonpositive log point mapped");
        axes.x.setRange({1e-200, 1e200});
        near(*axes.x.normalize(1), 0.5);
        near(*axes.x.denormalize(0.5), 1, 1e-12);
        require(axes.x.ticks().size() <= 20, "unbounded log ticks");
        axes.x.setScale(Scale::Linear);
        axes.x.setRange({-1e308, 1e308});
        near(*axes.x.normalize(0), 0.5);
        near(*axes.x.denormalize(0.5), 0);
        axes.x.resetAuto();
        axes.x.fit(Range{std::numeric_limits<double>::max(), std::numeric_limits<double>::max()});
        require(std::isfinite(axes.x.range().max), "constant max overflow");
        axes.x.fit(Range{1e-300, 1e-300});
        require(axes.x.range().min < 1e-300 && axes.x.range().max > 1e-300, "tiny constant range");
        axes.x.fit(std::nullopt);
        near(axes.x.range().min, 0);
        near(axes.x.range().max, 1);
        axes.x.formatter = [](double) { return "custom"; };
        for (const auto& tick : axes.x.ticks())
            require(!tick.major || tick.label == "custom", "custom formatter");
        throws<std::invalid_argument>([&] { axes.x.setRange({1, 1}); });
        axes.x.setRange({-1, 1});
        throws<std::invalid_argument>([&] { axes.x.setScale(Scale::Log10); });
        require(axes.x.scale() == Scale::Linear, "failed scale changed state");
        axes.y.setRange({-1, 1});
        require(axes.equalize({0, 0, 200, 100}), "equal aspect failed");
        near((axes.x.range().max - axes.x.range().min) / 200,
             (axes.y.range().max - axes.y.range().min) / 100);

           Figure figure;
           auto& first = figure.addPlot();
           auto& second = figure.addPlot();
           Axes firstAxes;
           firstAxes.x.setRange({-2, 6});
           first.setAxes(firstAxes);
           Axes secondAxes;
           secondAxes.x.setRange({10, 20});
           second.setAxes(secondAxes);
           figure.linkX({0, 1});
        figure.linkCursor({0, 1});
           figure.synchronizeLinks();
           near(second.axes().x.range().min, -2);
           near(second.axes().x.range().max, 6);
           firstAxes = first.axes();
           firstAxes.x.setRange({1, 3});
           first.setAxes(firstAxes);
           figure.synchronizeLinks();
           near(second.axes().x.range().min, 1);
           near(second.axes().x.range().max, 3);
        first.setCursor({{2, -1}});
        figure.synchronizeLinks();
        require(second.cursor() && second.cursor()->x == 2 && second.cursor()->y == -1,
            "linked cursor not synchronized");
        second.setCursor({{4, 3}});
        figure.synchronizeLinks();
        require(first.cursor() && first.cursor()->x == 4 && first.cursor()->y == 3,
            "linked cursor source not synchronized");
        first.setCursor(std::nullopt);
        figure.synchronizeLinks();
        require(!second.cursor(), "linked cursor not cleared");
           throws<std::invalid_argument>([&] { figure.linkY({0}); });
           throws<std::out_of_range>([&] { figure.linkY({0, 2}); });
        throws<std::invalid_argument>([&] { first.setCursor({{nan, 0}}); });

        Plot dualAxis;
        Axis secondaryAxis;
        dualAxis.setSecondaryYAxis(secondaryAxis);
        Series primary;
        primary.data = Data({0, 1}, {-2, 4});
        Series secondary;
        secondary.data = Data({0, 1}, {100, 300});
        secondary.yAxis = YAxis::Secondary;
        dualAxis.setSeries({primary, secondary});
        near(dualAxis.axes().x.range().min, 0);
        near(dualAxis.axes().x.range().max, 1);
        near(dualAxis.axes().y.range().min, -2);
        near(dualAxis.axes().y.range().max, 4);
        require(dualAxis.secondaryYAxis() && dualAxis.secondaryYAxis()->range().min == 100 &&
                dualAxis.secondaryYAxis()->range().max == 300,
            "secondary axis fit");

        PolarAxes polar;
        polar.setAngleUnit(AngleUnit::Degrees);
        polar.fit({Data({0, 90, 180, 270}, {1, 2, 3, -1})});
        near(polar.radius.range().min, 0);
        near(polar.radius.range().max, 3);
        const Viewport polarViewport{0, 0, 200, 200};
        const auto polarScreen = polar.toScreen({90, 3}, polarViewport);
        require(polarScreen.has_value(), "polar screen mapping");
        near(polarScreen->x, 100);
        near(polarScreen->y, 0);
        const auto polarData = polar.toData(*polarScreen, polarViewport);
        require(polarData.has_value(), "polar data mapping");
        near(polarData->x, 90);
        near(polarData->y, 3);
        polar.setClockwise(true);
        near(polar.toScreen({90, 3}, polarViewport)->y, 200);
        require(!polar.toScreen({0, -1}, polarViewport), "negative polar radius mapped");
        require(!polar.toData({0, 0}, polarViewport), "outside polar disk mapped");

        ScalarField field(2, 3, {1, nan, 3, 4, 5, std::numeric_limits<double>::infinity()}, {-2, 4},
                  {10, 20}, FieldOrigin::UpperLeft, FieldSampling::GridPoints);
        require(field.rows() == 2 && field.columns() == 3 && std::isnan(field.value(0, 1)),
            "scalar field row-major layout");
        require(field.origin() == FieldOrigin::UpperLeft && field.sampling() == FieldSampling::GridPoints,
            "scalar field metadata");
        require(field.finiteRange() && field.finiteRange()->min == 1 && field.finiteRange()->max == 5,
            "scalar field finite range");
        throws<std::invalid_argument>([] { ScalarField(0, 1, {}); });
        throws<std::invalid_argument>([] { ScalarField(2, 2, {1, 2, 3}); });
        throws<std::out_of_range>([&] { field.value(2, 0); });

        ColorScale colors;
        colors.fit(field.finiteRange());
        require(colors.range().min == 1 && colors.range().max == 5, "automatic color range");
        const auto low = colors.map(1), high = colors.map(5), missing = colors.map(nan);
        require(low != high && missing == colors.missingColor(), "continuous color scale");
        colors.setDiscreteLevels(2);
        require(colors.map(2) == colors.map(1) && colors.map(4) == colors.map(5), "discrete color scale");
        colors.setMode(ColorScaleMode::Log10);
        colors.setRange({1, 100});
        require(colors.map(0) == colors.missingColor() && colors.map(1) != colors.map(100),
            "logarithmic color scale");
        ColorScale automaticLogColors;
        automaticLogColors.setMode(ColorScaleMode::Log10);
        automaticLogColors.fit(ScalarField(1, 3, {-1, 1, 100}));
        require(automaticLogColors.range().min == 1 && automaticLogColors.range().max == 100,
            "logarithmic scalar field range");
        std::cout << "plot_numeric: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "plot_numeric: " << error.what() << '\n';
        return 1;
    }
}
