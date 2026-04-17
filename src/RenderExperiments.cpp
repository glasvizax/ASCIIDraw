#include <xm/xm.h>
#include <xm/math_helpers.h>

void nonLinearPerspectiveExample()
{
    xm::vec2 o(0.0f, -0.4f);

    float _near = 0.1f;

    xm::vec2 x(0.1f, 0.0f);
    xm::vec2 y(0.0f, 0.1f);

    float current_fov = 120.0f;
    float half_tan = std::tan(xm::to_radians(current_fov / 2));

    float plane_y = o.y + _near;
    float plane_r = _near * half_tan;
    float plane_l = -plane_r;

    xm::vec2 l(plane_l, plane_y);
    xm::vec2 r(plane_r, plane_y);

    xm::vec2 v0(-0.9f, -0.2f);
    xm::vec2 v1(0.5f, 0.9f);
    float alpha = 0.5f;

    xm::vec2 p = v0 * alpha + v1 * (1 - alpha);
    /*
    pushLine(getIntensitySymbolF(1.0f), v0, v1);
    pushLine(getIntensitySymbolF(1.0f), l, r);
    */
    xm::vec2 v0_view = v0 - o;
    xm::vec2 v1_view = v1 - o;
    xm::vec2 p_view = p - o;

    xm::vec2 v0_proj((v0_view.x * _near) / v0_view.y, 0);
    xm::vec2 v1_proj((v1_view.x * _near) / v1_view.y, 0);
    xm::vec2 p_proj((p_view.x * _near) / p_view.y, 0);

    xm::vec2 v0_ndc = v0_proj + o;
    xm::vec2 v1_ndc = v1_proj + o;

    // 0.87f !!!
    float new_alpha = (p_proj.x - v0_proj.x) / (v1_proj.x - v0_proj.x);

    /*
    pushPixel(getIntensitySymbolF(0.0f), o);
    pushPixel(getIntensitySymbolF(0.0f), xm::vec2(v0_ndc.x, plane_y));
    pushPixel(getIntensitySymbolF(0.0f), xm::vec2(v1_ndc.x, plane_y));
    */
}