// wxl-unit-outline: shaders for the reaction-colored silhouette outline.
// Copyright (C) 2026 WarcraftXL
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

namespace
{
    // Writes opaque model batches into the mask. Do not sample texture alpha here: many diffuse
    // alpha channels carry material/detail data and would turn the silhouette into internal noise.
    const char* kColorHLSL =
        "float4 c0 : register(c0);\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 {\n"
        "  return c0;\n"
        "}\n";

    // Writes alpha-tested batches into the mask, following real cutouts (wings, hair cards, etc.).
    const char* kCutoutColorHLSL =
        "sampler2D s0 : register(s0);\n"
        "float4 c0 : register(c0);\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 {\n"
        "  clip(tex2D(s0, uv).a - 0.5);\n"
        "  return c0;\n"
        "}\n";

    // 8-tap edge detect on the mask: a thin anti-aliased line outside the silhouette.
    //   c0 = {1/width, 1/height, thickness, intensity}   (intensity weights the accumulated edge)
    //   c1 = {opacity, threshold, unused, unused}        (scale on the default alpha, clip cutoff)
    // The mask stores premultiplied color (rgb * weight, weight), where weight lets a target's edge
    // brightness vary (the mouseover multiplier). rgb/alpha recovers the color; the interior test uses
    // center-vs-average so it stays correct whatever the weight is.
    const char* kEdgeHLSL =
        "sampler2D m : register(s0);\n"
        "float4 px : register(c0);\n"
        "float4 par : register(c1);\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 {\n"
        "  float2 o = px.xy * px.z;\n"
        "  float4 a = tex2D(m,uv+float2(o.x,0)) + tex2D(m,uv+float2(-o.x,0))\n"
        "           + tex2D(m,uv+float2(0,o.y)) + tex2D(m,uv+float2(0,-o.y))\n"
        "           + tex2D(m,uv+float2(o.x,o.y)) + tex2D(m,uv+float2(-o.x,-o.y))\n"
        "           + tex2D(m,uv+float2(o.x,-o.y)) + tex2D(m,uv+float2(-o.x,o.y));\n"
        "  float center = tex2D(m, uv).a;\n"
        "  float inside = a.a > 0.0 ? saturate(center * 8.0 / a.a) : 0.0;\n"
        "  float outline = saturate(a.a * 0.125 * px.w) * (1.0 - inside);\n"
        "  clip(outline - par.y);\n"
        "  float3 col = a.a > 0.001 ? a.rgb / a.a : float3(1,1,1);\n"
        "  return float4(col, saturate(outline * 1.4 * par.x));\n"
        "}\n";
}
