// wxl-unit-outline: reaction-colored silhouette outline on the mouseover and target units.
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

#include "Outline.hpp"
#include "Hlsl.hpp"

#include "game/Unit.hpp"
#include "game/World.hpp"

#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef GetObject
#undef GetObject
#endif

namespace wxl::scripts::outline
{
    namespace gx    = wxl::game::gx;
    namespace ev    = wxl::events;
    namespace world = wxl::game::world;
    namespace unit  = wxl::game::unit;

    constexpr uint32_t kFmtA8R8G8B8 = 21;
    constexpr const char* kIniSection = "wxl-unit-outline";

    namespace
    {
        unsigned long long FileStamp(const std::string& path)
        {
            WIN32_FILE_ATTRIBUTE_DATA data;
            if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
                return 0;
            return (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                   data.ftLastWriteTime.dwLowDateTime;
        }

        std::string Trim(std::string text)
        {
            const size_t first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return std::string();
            const size_t last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

        float ReadFloat(const std::string& path, const char* key, float fallback, float lo, float hi)
        {
            char buf[64] = {};
            GetPrivateProfileStringA(kIniSection, key, "", buf, sizeof(buf), path.c_str());
            if (!buf[0])
                return fallback;
            char* end = nullptr;
            const float v = std::strtof(buf, &end);
            if (end == buf || v != v)
                return fallback;
            return std::clamp(v, lo, hi);
        }

        int ReadInt(const std::string& path, const char* key, int fallback, int lo, int hi)
        {
            const int v = static_cast<int>(GetPrivateProfileIntA(kIniSection, key, fallback, path.c_str()));
            return std::clamp(v, lo, hi);
        }

        // Accepts "R,G,B" (0-255 each) or "#RRGGBB" / "RRGGBB". Anything else leaves out untouched.
        void ReadColor(const std::string& path, const char* key, float out[3])
        {
            char buf[64] = {};
            GetPrivateProfileStringA(kIniSection, key, "", buf, sizeof(buf), path.c_str());
            const std::string text = Trim(buf);
            if (text.empty())
                return;

            int r = -1, g = -1, b = -1;
            if (text.find(',') != std::string::npos)
            {
                if (std::sscanf(text.c_str(), "%d , %d , %d", &r, &g, &b) != 3)
                    return;
            }
            else
            {
                const char* hex = text.c_str();
                if (*hex == '#')
                    ++hex;
                char* end = nullptr;
                const unsigned long v = std::strtoul(hex, &end, 16);
                if (end == hex || *end != '\0' || std::strlen(hex) != 6)
                    return;
                r = static_cast<int>((v >> 16) & 0xFF);
                g = static_cast<int>((v >> 8) & 0xFF);
                b = static_cast<int>(v & 0xFF);
            }

            out[0] = Clamp01(static_cast<float>(std::clamp(r, 0, 255)) / 255.0f);
            out[1] = Clamp01(static_cast<float>(std::clamp(g, 0, 255)) / 255.0f);
            out[2] = Clamp01(static_cast<float>(std::clamp(b, 0, 255)) / 255.0f);
        }

        void WriteInt(const std::string& path, const char* key, int value)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", value);
            WritePrivateProfileStringA(kIniSection, key, buf, path.c_str());
        }

        void WriteFloat(const std::string& path, const char* key, float value)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.3f", value);
            WritePrivateProfileStringA(kIniSection, key, buf, path.c_str());
        }

        // Writes as "#RRGGBB" so the file stays readable and round-trips through ReadColor.
        void WriteColor(const std::string& path, const char* key, const float rgb[3])
        {
            const int r = static_cast<int>(Clamp01(rgb[0]) * 255.0f + 0.5f);
            const int g = static_cast<int>(Clamp01(rgb[1]) * 255.0f + 0.5f);
            const int b = static_cast<int>(Clamp01(rgb[2]) * 255.0f + 0.5f);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
            WritePrivateProfileStringA(kIniSection, key, buf, path.c_str());
        }

        bool SameStyle(const OutlineStyle& a, const OutlineStyle& b)
        {
            return a.enabled == b.enabled && a.outlineMouseover == b.outlineMouseover &&
                   a.outlineTarget == b.outlineTarget && a.includeMount == b.includeMount &&
                   a.thickness == b.thickness && a.intensity == b.intensity &&
                   a.opacity == b.opacity && a.threshold == b.threshold &&
                   a.mouseoverBrightness == b.mouseoverBrightness &&
                   a.hostile[0] == b.hostile[0] && a.hostile[1] == b.hostile[1] && a.hostile[2] == b.hostile[2] &&
                   a.neutral[0] == b.neutral[0] && a.neutral[1] == b.neutral[1] && a.neutral[2] == b.neutral[2] &&
                   a.friendly[0] == b.friendly[0] && a.friendly[1] == b.friendly[1] && a.friendly[2] == b.friendly[2];
        }

        void ColorEdit(const WXL_Api& api, const char* label, float rgb[3])
        {
            float rgba[4] = { rgb[0], rgb[1], rgb[2], 1.0f };
            if (api.UiColorEdit(label, rgba))
            {
                rgb[0] = rgba[0];
                rgb[1] = rgba[1];
                rgb[2] = rgba[2];
            }
        }

        void* RootModel(void* model)
        {
            void* root = model;
            for (int hop = 0; root && hop < Outline::kMaxModelHops; ++hop)
            {
                void* parent = unit::ModelParent(root);
                if (!parent)
                    break;
                root = parent;
            }
            return root;
        }
    }

    class ScopedDeviceState final
    {
    public:
        explicit ScopedDeviceState(gx::Device9 dev)
        {
            auto* d = static_cast<IDirect3DDevice9*>(dev.raw());
            if (d && SUCCEEDED(d->CreateStateBlock(D3DSBT_ALL, &state_)) && state_)
                state_->Capture();
        }

        ~ScopedDeviceState() { Restore(); }

        void Restore()
        {
            if (!state_) return;
            state_->Apply();
            state_->Release();
            state_ = nullptr;
        }

    private:
        IDirect3DStateBlock9* state_ = nullptr;
    };

    Outline::Outline()
    {
        on<&Outline::OnWorldRenderEnd>(ev::Event::OnWorldRenderEnd);
        on<&Outline::OnEndScene>(ev::Event::OnEndScene);
        on<&Outline::OnM2Batch>(ev::Event::OnM2BatchDraw);
        on<&Outline::OnDeviceLost>(ev::Event::OnDeviceLost);
    }

    void Outline::Log(int level, const char* fmt, ...) const
    {
        if (!api_ || !api_->Log) return;
        va_list ap;
        va_start(ap, fmt);
        char msg[512];
        std::vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
        api_->Log(level, "wxl-unit-outline", "%s", msg);
    }

    void Outline::LoadConfig(const std::string& iniPath)
    {
        iniPath_     = iniPath;
        configStamp_ = FileStamp(iniPath_);
        LoadConfigNow();
    }

    void Outline::LoadConfigNow()
    {
        OutlineStyle s;
        s.enabled          = ReadInt(iniPath_, "Enable",            s.enabled ? 1 : 0, 0, 1) != 0;
        s.outlineMouseover = ReadInt(iniPath_, "OutlineMouseover",  s.outlineMouseover ? 1 : 0, 0, 1) != 0;
        s.outlineTarget    = ReadInt(iniPath_, "OutlineTarget",     s.outlineTarget ? 1 : 0, 0, 1) != 0;
        s.includeMount     = ReadInt(iniPath_, "IncludeMount",      s.includeMount ? 1 : 0, 0, 1) != 0;
        s.thickness        = ReadFloat(iniPath_, "Thickness",  s.thickness, 0.5f, 6.0f);
        s.intensity        = ReadFloat(iniPath_, "Intensity",  s.intensity, 0.5f, 4.0f);
        s.opacity          = ReadFloat(iniPath_, "Opacity",    s.opacity,   0.0f, 1.0f);
        s.threshold        = ReadFloat(iniPath_, "Threshold",  s.threshold, 0.0f, 0.5f);
        s.mouseoverBrightness = ReadFloat(iniPath_, "MouseoverBrightness", s.mouseoverBrightness, 0.0f, 4.0f);
        ReadColor(iniPath_, "ColorHostile",  s.hostile);
        ReadColor(iniPath_, "ColorNeutral",  s.neutral);
        ReadColor(iniPath_, "ColorFriendly", s.friendly);
        style_ = s;
        saved_ = s;
    }

    bool Outline::SaveConfig()
    {
        if (iniPath_.empty())
            return false;

        WriteInt(iniPath_,   "Enable",           style_.enabled ? 1 : 0);
        WriteInt(iniPath_,   "OutlineMouseover", style_.outlineMouseover ? 1 : 0);
        WriteInt(iniPath_,   "OutlineTarget",    style_.outlineTarget ? 1 : 0);
        WriteInt(iniPath_,   "IncludeMount",     style_.includeMount ? 1 : 0);
        WriteFloat(iniPath_, "Thickness",        style_.thickness);
        WriteFloat(iniPath_, "Intensity",        style_.intensity);
        WriteFloat(iniPath_, "Opacity",          style_.opacity);
        WriteFloat(iniPath_, "Threshold",        style_.threshold);
        WriteFloat(iniPath_, "MouseoverBrightness", style_.mouseoverBrightness);
        WriteColor(iniPath_, "ColorHostile",  style_.hostile);
        WriteColor(iniPath_, "ColorNeutral",  style_.neutral);
        WriteColor(iniPath_, "ColorFriendly", style_.friendly);

        // The write bumps the file stamp; adopt it so the self-write is not mistaken for an external
        // edit (which would overwrite a panel tweak made right after Save).
        configStamp_ = FileStamp(iniPath_);
        saved_       = style_;
        Log(WXL_LOG_INFO, "settings saved to %s", iniPath_.c_str());
        return true;
    }

    void Outline::RevertConfig()
    {
        LoadConfigNow();
        Log(WXL_LOG_INFO, "settings reverted to %s", iniPath_.c_str());
    }

    bool Outline::HasUnsavedChanges() const
    {
        return !SameStyle(style_, saved_);
    }

    void Outline::DrawPanel(const WXL_Api& api)
    {
        if (!api.UiCheckbox || !api.UiSliderFloat || !api.UiSeparator || !api.UiText ||
            !api.UiColorEdit || !api.UiButton || !api.UiSameLine)
            return;

        int enabled = style_.enabled ? 1 : 0;
        if (api.UiCheckbox("Enable", &enabled)) style_.enabled = enabled != 0;

        int mouseover = style_.outlineMouseover ? 1 : 0;
        if (api.UiCheckbox("Outline mouseover", &mouseover)) style_.outlineMouseover = mouseover != 0;

        int target = style_.outlineTarget ? 1 : 0;
        if (api.UiCheckbox("Outline target", &target)) style_.outlineTarget = target != 0;

        int mount = style_.includeMount ? 1 : 0;
        if (api.UiCheckbox("Include mount", &mount)) style_.includeMount = mount != 0;

        api.UiSeparator();
        api.UiSliderFloat("Thickness (px)", &style_.thickness, 0.5f, 6.0f);
        api.UiSliderFloat("Intensity", &style_.intensity, 0.5f, 4.0f);
        api.UiSliderFloat("Opacity", &style_.opacity, 0.0f, 1.0f);
        api.UiSliderFloat("Threshold", &style_.threshold, 0.0f, 0.5f);
        api.UiSliderFloat("Mouseover brightness", &style_.mouseoverBrightness, 0.0f, 4.0f);

        api.UiSeparator();
        ColorEdit(api, "Hostile color", style_.hostile);
        ColorEdit(api, "Neutral color", style_.neutral);
        ColorEdit(api, "Friendly color", style_.friendly);

        api.UiSeparator();
        if (api.UiButton("Save")) SaveConfig();
        api.UiSameLine();
        if (api.UiButton("Revert")) RevertConfig();
        api.UiText(HasUnsavedChanges() ? "Unsaved changes" : "Matches wxl-unit-outline.ini");
    }

    void Outline::ReloadConfigIfChanged()
    {
        if (iniPath_.empty())
            return;
        const unsigned long long stamp = FileStamp(iniPath_);
        if (stamp == configStamp_)
            return;
        configStamp_ = stamp;
        LoadConfigNow();
        Log(WXL_LOG_INFO, "config reloaded from %s", iniPath_.c_str());
    }

    void Outline::ColorForReaction(int reaction, float* c) const
    {
        const float* src;
        if (reaction < 2)      src = style_.hostile;
        else if (reaction < 4) src = style_.neutral;
        else                   src = style_.friendly;
        c[0] = src[0]; c[1] = src[1]; c[2] = src[2]; c[3] = 1.0f;
    }

    bool Outline::EnsureResources(gx::Device9 dev)
    {
        if (!colorPS_)      colorPS_ = gx::CompilePixelShader(dev, kColorHLSL, "ps_2_0");
        if (!cutoutPS_)     cutoutPS_ = gx::CompilePixelShader(dev, kCutoutColorHLSL, "ps_2_0");
        if (!edgePS_)       edgePS_  = gx::CompilePixelShader(dev, kEdgeHLSL,  "ps_2_0");
        if (!mask_.surface) gx::EnsureBackbufferTarget(dev, mask_, kFmtA8R8G8B8);
        return colorPS_ && cutoutPS_ && edgePS_ && mask_.surface;
    }

    int Outline::FindTarget(void* model) const
    {
        for (int hop = 0; model && hop < kMaxModelHops; ++hop, model = unit::ModelParent(model))
            for (int i = 0; i < count_; ++i)
                if (targets_[i].model == model) return i;
        return -1;
    }

    bool Outline::IsModel(void* model, void* want) const
    {
        if (!want) return false;
        for (int hop = 0; model && hop < kMaxModelHops; ++hop, model = unit::ModelParent(model))
            if (model == want) return true;
        return false;
    }

    void Outline::AddEntry(void* model, const float* color, bool isPlayer)
    {
        if (!model || count_ >= kMaxTargets) return;
        for (int i = 0; i < count_; ++i)
            if (targets_[i].model == model) return; // dedup

        targets_[count_].model    = model;
        targets_[count_].isPlayer = isPlayer;
        std::memcpy(targets_[count_].color, color, sizeof(float) * 4);
        ++count_;
    }

    // Records a selected unit's instance. A mounted unit's mount lives higher in the attachment chain
    // (the rider hangs off the mount), so with ancestors set the whole chain is recorded and the mount
    // is stamped with the same reaction color as the rider.
    void Outline::AddModel(void* model, const float* color, bool isPlayer, bool ancestors)
    {
        for (int hop = 0; model && hop <= kMaxModelHops; ++hop)
        {
            AddEntry(model, color, isPlayer);
            if (!ancestors) break;
            model = unit::ModelParent(model);
        }
    }

    void Outline::AddTarget(unsigned long long guid, void* player, bool mouseover)
    {
        if (!guid) return;
        const bool isPlayer = (guid >> 32) == 0;

        void* obj = world::ResolveObject(guid, isPlayer ? world::kTypeMaskPlayer : world::kTypeMaskUnit);
        if (!obj) return;

        void* model = unit::Model(obj);
        if (!model) return;

        const int reaction = player ? unit::Reaction(obj, player) : 5;
        float color[4];
        ColorForReaction(reaction, color);

        // The edge shader weights the outline by the mask alpha and recovers the line color as
        // rgb/alpha, so scaling the whole entry keeps the reaction color while making the mouseover
        // edge brighter or dimmer on top of the global Intensity.
        if (mouseover)
        {
            const float b = style_.mouseoverBrightness;
            color[0] *= b;
            color[1] *= b;
            color[2] *= b;
            color[3] *= b;
        }
        AddModel(model, color, isPlayer, style_.includeMount);
    }

    void Outline::RebuildTargets()
    {
        count_ = 0;
        if (!style_.enabled)
        {
            playerModel_ = nullptr;
            return;
        }

        void* player     = world::ResolveObject(world::ActivePlayerGuid(), world::kTypeMaskPlayer);
        void* playerBase = player ? unit::Model(player) : nullptr;
        // The occluder is the player's whole instance chain, so a mount that hides a target is punched
        // out of the mask just as the rider is.
        playerModel_ = style_.includeMount ? RootModel(playerBase) : playerBase;

        if (style_.outlineMouseover) AddTarget(world::MouseoverGuid(), player, true);
        if (style_.outlineTarget)    AddTarget(world::TargetGuid(),    player, false);
    }

    bool Outline::ShouldStampBatch(gx::Device9 dev) const
    {
        if (!dev) return false;

        // Attached particles, glows and billboard effects render alpha-blended quads through the same
        // model context. Letting them into the mask makes the edge pass outline their broad cards instead
        // of the unit mesh.
        return dev.GetRenderState(gx::rs::kAlphaBlend) == 0;
    }

    // Draw the model into the mask render target, with full device-state save/restore. color fills the
    // mask; clear wipes the mask once before the first silhouette of the frame.
    void Outline::StampMask(gx::Device9 dev, const ev::M2BatchDrawArgs& a, const float* color, bool clear)
    {
        ScopedDeviceState state(dev);
        void* oldRT = nullptr; dev.GetRenderTarget(0, &oldRT);
        void* oldDS = nullptr; dev.GetDepthStencil(&oldDS);
        void* oldPS = nullptr; dev.GetPixelShader(&oldPS);
        unsigned char oldVP[24]; dev.GetViewport(oldVP);
        const unsigned alphaRef = dev.GetRenderState(D3DRS_ALPHAREF);
        const bool alphaCutout = dev.GetRenderState(D3DRS_ALPHATESTENABLE) != 0 && alphaRef >= 8;
        const unsigned sAB = dev.GetRenderState(gx::rs::kAlphaBlend);
        const unsigned sZE = dev.GetRenderState(gx::rs::kZEnable);
        const unsigned sZW = dev.GetRenderState(gx::rs::kZWrite);
        const unsigned sZF = dev.GetRenderState(gx::rs::kZFunc);

        dev.SetRenderTarget(0, mask_.surface);
        // Depth-test every target against the scene so terrain, buildings and other world geometry
        // occlude the mask (and therefore the outline). Depth writes stay off so the scene's own depth
        // buffer is untouched.
        dev.SetDepthStencil(oldDS);
        dev.SetRenderState(gx::rs::kZEnable, 1);
        dev.SetRenderState(gx::rs::kZWrite, 0);
        dev.SetRenderState(gx::rs::kZFunc, gx::cmp::kLessEqual);
        if (clear && !maskCleared_)
        {
            dev.Clear(0, nullptr, gx::clear::kTarget, 0x00000000, 1.0f, 0);
            maskCleared_ = true;
        }
        dev.SetRenderState(gx::rs::kAlphaBlend, 0);
        dev.SetPixelShader(alphaCutout ? cutoutPS_ : colorPS_);
        dev.SetPixelShaderConstantF(0, color, 1);
        dev.DrawIndexedPrimitive(a.primType, a.baseVertex, a.minIndex, a.numVerts, a.startIndex, a.primCount);

        dev.SetPixelShader(oldPS);
        dev.SetRenderTarget(0, oldRT);
        dev.SetDepthStencil(oldDS);
        dev.SetViewport(oldVP);
        dev.SetRenderState(gx::rs::kAlphaBlend, sAB);
        dev.SetRenderState(gx::rs::kZEnable, sZE);
        dev.SetRenderState(gx::rs::kZWrite, sZW);
        dev.SetRenderState(gx::rs::kZFunc, sZF);
        state.Restore();
        gx::Release(oldRT);
        gx::Release(oldDS);
        gx::Release(oldPS);
    }

    void Outline::StampSilhouette(gx::Device9 dev, const ev::M2BatchDrawArgs& a, int idx)
    {
        StampMask(dev, a, targets_[idx].color, /*clear=*/true);
    }

    // The local player character draws in the same M2 pass as everyone else and can be drawn after the
    // target it hides. Zero the mask over its depth-tested silhouette so a target seen through the
    // player is not outlined on top of it.
    void Outline::StampOccluder(gx::Device9 dev, const ev::M2BatchDrawArgs& a)
    {
        const float kZero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        StampMask(dev, a, kZero, /*clear=*/false);
    }

    void Outline::EdgePass(gx::Device9 dev)
    {
        ScopedDeviceState state(dev);
        const unsigned sZE = dev.GetRenderState(gx::rs::kZEnable);
        const unsigned sAB = dev.GetRenderState(gx::rs::kAlphaBlend);
        const unsigned sSB = dev.GetRenderState(gx::rs::kSrcBlend);
        const unsigned sDB = dev.GetRenderState(gx::rs::kDestBlend);
        const unsigned sCU = dev.GetRenderState(gx::rs::kCullMode);

        dev.SetRenderState(gx::rs::kZEnable, 0);
        dev.SetRenderState(gx::rs::kCullMode, gx::cull::kNone);
        dev.SetRenderState(gx::rs::kAlphaBlend, 1);
        dev.SetRenderState(gx::rs::kSrcBlend, gx::blend::kSrcAlpha);
        dev.SetRenderState(gx::rs::kDestBlend, gx::blend::kInvSrcAlpha);
        dev.SetVertexShader(nullptr);
        dev.SetTexture(0, mask_.texture);
        dev.SetPixelShader(edgePS_);

        const float c0[4] = { 1.0f / mask_.width, 1.0f / mask_.height, style_.thickness, style_.intensity };
        const float c1[4] = { style_.opacity, style_.threshold, 0.0f, 0.0f };
        dev.SetPixelShaderConstantF(0, c0, 1);
        dev.SetPixelShaderConstantF(1, c1, 1);
        gx::DrawFullscreenQuad(dev);

        dev.SetPixelShader(nullptr);
        dev.SetTexture(0, nullptr);
        dev.SetRenderState(gx::rs::kZEnable, sZE);
        dev.SetRenderState(gx::rs::kCullMode, sCU);
        dev.SetRenderState(gx::rs::kAlphaBlend, sAB);
        dev.SetRenderState(gx::rs::kSrcBlend, sSB);
        dev.SetRenderState(gx::rs::kDestBlend, sDB);
        state.Restore();
    }

    void Outline::OnM2Batch(const ev::M2BatchDrawArgs& a)
    {
        if (!style_.enabled || count_ == 0 || !colorPS_ || !cutoutPS_ || !mask_.surface) return;

        const int idx = FindTarget(a.model);
        if (idx >= 0)
        {
            gx::Device9 dev(a.device);
            if (!ShouldStampBatch(dev)) return;
            StampSilhouette(dev, a, idx);
            return;
        }

        // The player character is not a target; punch it out of a mask the target already stamped. If
        // the target has not stamped yet the scene depth handles the occluder on its own, so skip.
        if (playerModel_ && maskCleared_ && IsModel(a.model, playerModel_))
        {
            gx::Device9 dev(a.device);
            if (!ShouldStampBatch(dev)) return;
            StampOccluder(dev, a);
        }
    }

    // Composite the mask at the world -> UI boundary. EndScene runs after the client has drawn the
    // interface, and compositing there painted the outline over windows and the HUD; this slot is before
    // the interface so the UI stays on top of the outline.
    void Outline::OnWorldRenderEnd(const ev::WorldRenderEndArgs& a)
    {
        if (!style_.enabled) return;

        gx::Device9 dev(a.device);
        if (!dev) return;

        if (EnsureResources(dev) && maskCleared_)
            EdgePass(dev);
    }

    void Outline::OnDeviceLost(const ev::DeviceResetArgs&)
    {
        // The mask is D3DPOOL_DEFAULT (and so are the shaders), so everything must go before the
        // engine's IDirect3DDevice9::Reset. EnsureResources rebuilds lazily afterwards.
        gx::ReleaseResetResources();
        gx::Release(colorPS_);  colorPS_  = nullptr;
        gx::Release(cutoutPS_); cutoutPS_ = nullptr;
        gx::Release(edgePS_);   edgePS_   = nullptr;
        maskCleared_ = false;
        count_       = 0;
    }

    void Outline::OnEndScene(const ev::EndSceneArgs& a)
    {
        ReloadConfigIfChanged();

        gx::Device9 dev(a.device);
        if (!dev) return;

        // Rebuild the target list for the next frame; the mask itself was already consumed by
        // OnWorldRenderEnd this frame.
        RebuildTargets();
        maskCleared_ = false;
    }
}
