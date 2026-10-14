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
    //   c1 = {opacity, threshold, occluder-on, unused}  (scale on the default alpha, clip cutoff,
    //        1 when the player mask was stamped this frame so a stale silhouette cannot punch the line)
    // The mask stores premultiplied color (rgb * weight, weight), where weight lets a target's edge
    // brightness vary (the mouseover multiplier). rgb/alpha recovers the color; the interior test uses
    // center-vs-average so it stays correct whatever the weight is.
    //
    // The tap ring is blown up to BLUR_TAP times the configured thickness and its weight is rolled off
    // towards the ring's outside, so a pointy model's edge (shoulders, spikes, weapon tips) lands as a
    // soft gradient instead of a hard 1-pixel step. The line still thins and thickens with the setting,
    // because the ring and the roll-off both scale with px.z.
    const char* kEdgeHLSL =
        "sampler2D m : register(s0);\n"
        "sampler2D occ : register(s1);\n"
        "float4 px : register(c0);\n"
        "float4 par : register(c1);\n"
        "#define BLUR_TAP 1.7\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 {\n"
        "  float2 o = px.xy * px.z * BLUR_TAP;\n"
        "  float4 a = tex2D(m,uv+float2(o.x,0)) + tex2D(m,uv+float2(-o.x,0))\n"
        "           + tex2D(m,uv+float2(0,o.y)) + tex2D(m,uv+float2(0,-o.y))\n"
        "           + tex2D(m,uv+float2(o.x,o.y)) + tex2D(m,uv+float2(-o.x,-o.y))\n"
        "           + tex2D(m,uv+float2(o.x,-o.y)) + tex2D(m,uv+float2(-o.x,o.y));\n"
        "  float center = tex2D(m, uv).a;\n"
        "  float inside = a.a > 0.0 ? saturate(center * 8.0 / a.a) : 0.0;\n"
        "  float outline = saturate(a.a * (0.125 / BLUR_TAP) * px.w) * (1.0 - inside);\n"
        "  clip(outline - par.y);\n"
        "  float3 col = a.a > 0.001 ? a.rgb / a.a : float3(1,1,1);\n"
        "  float om = tex2D(occ, uv).a * par.z;\n"
        "  return float4(col, saturate(outline * 1.4 * par.x) * (1.0 - om));\n"
        "}\n";

    // A separable gaussian tap for the softening pass: 5 explicit samples along one axis, written out
    // rather than looped because the module's pixel shaders are ps_2_0 and only compile reliably when
    // the tap positions are literals. Run once per axis and the silhouette the edge pass samples has a
    // gradient instead of a hard 1-pixel boundary; the interior is a flat plateau, so only the rim
    // actually widens. The spacing is a fixed fraction of a texel, not a multiple of Thickness: at that
    // scale Thickness reads as line shape, and blurring it proportionally would just mud the whole line.
    //   c0 = {offset.x, offset.y, unused, unused}
    const char* kBlurHLSL =
        "sampler2D src : register(s0);\n"
        "float4 dir : register(c0);\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 {\n"
        "  float4 s = tex2D(src, uv) * 0.2270270270;\n"
        "  s += (tex2D(src, uv + dir.xy) + tex2D(src, uv - dir.xy)) * 0.1945945946;\n"
        "  s += (tex2D(src, uv + dir.xy * 2.0) + tex2D(src, uv - dir.xy * 2.0)) * 0.1216216216;\n"
        "  s += (tex2D(src, uv + dir.xy * 3.0) + tex2D(src, uv - dir.xy * 3.0)) * 0.0540540541;\n"
        "  s += (tex2D(src, uv + dir.xy * 4.0) + tex2D(src, uv - dir.xy * 4.0)) * 0.0162162162;\n"
        "  return s;\n"
        "}\n";
}
