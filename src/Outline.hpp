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

#pragma once

#include "wxl/PluginApi.h"
#include "wxl/EventScript.hpp"
#include "game/Gx.hpp"

#include <string>

// Render script. It owns its shaders, its target list and its render targets, and draws purely
// through the core gx facade and the unit/world bindings. It never touches an offset or installs a
// hook: it binds member functions to render events and the core does the rest.
//
// Pipeline (one-frame): EndScene of frame N rebuilds the target list for frame N+1; during frame N+1 the
// per-batch event stamps each target's silhouette into a mask; WorldRenderEnd of N+1 edge-detects the mask
// into the frame at the world -> UI boundary (before the interface draws), then EndScene rebuilds again.
//
// A mounted unit is drawn as more than one M2 instance: the rider hangs off the mount in the attachment
// chain. AddTarget therefore records the whole ancestor chain of a selected unit's instance, so the mount
// gets outlined with the same reaction color as the unit riding it.
namespace wxl::scripts::outline
{
    /** @brief User-tunable look, read from wxl-unit-outline.ini beside the DLL. */
    struct OutlineStyle
    {
        bool  enabled          = true;   // master switch
        bool  outlineMouseover = true;   // outline the unit under the cursor
        bool  outlineTarget    = true;   // outline the current target
        bool  includeMount     = true;   // also outline a selected unit's mount

        float thickness = 2.5f;   // edge width in screen pixels
        float intensity = 1.6f;   // edge accumulation weight (brightness)
        float opacity   = 1.0f;   // final outline alpha scale
        float threshold = 0.02f;  // clip cutoff; trims faint pixels

        // Extra brightness multiplier applied only to the mouseover outline, on top of Intensity.
        // 1.0 matches a target outline; >1 glows brighter, <1 dims it.
        float mouseoverBrightness = 1.0f;

        // Depth-test the silhouette against the scene so terrain and buildings occlude it. Off by
        // default: on this client's D3D9On12 path the mask's depth test rejects every stamp at x1
        // (while at x2 the RT/depth sample mismatch drops the test), so the outline is only reliably
        // visible with it off. Turn on only if the client's depth path behaves.
        bool  occlusion = false;

        float hostile[3]  = { 1.0f, 0.0f, 0.0f };
        float neutral[3]  = { 1.0f, 1.0f, 0.0f };
        float friendly[3] = { 0.0f, 1.0f, 0.0f };
    };

    class Outline final : public wxl::ext::EventScript
    {
    public:
        Outline(); // binds the event handlers

        /** @brief Stores the core's service table (used for logging). */
        void SetApi(const WXL_Api* api) { api_ = api; }

        /** @brief Loads the look from an INI file; later edits are picked up automatically. */
        void LoadConfig(const std::string& iniPath);

        /** @brief Draws the module's overlay panel body; called by the core while the overlay is open. */
        void DrawPanel(const WXL_Api& api);

        /** @brief Writes the live look back to the INI. @return true when it was written. */
        bool SaveConfig();

        /** @brief Discards unsaved panel edits and re-reads the INI. */
        void RevertConfig();

        /** @brief True when the live look differs from what is on disk. */
        bool HasUnsavedChanges() const;

        static constexpr int kMaxTargets   = 16; // 2 selections, each + its mount chain
        static constexpr int kMaxModelHops = 6;  // attachment-chain depth

    private:
        // --- event handlers ---
        void OnWorldRenderEnd(const events::WorldRenderEndArgs& a);
        void OnEndScene(const events::EndSceneArgs& a);
        void OnM2Batch(const events::M2BatchDrawArgs& a);
        void OnDeviceLost(const events::DeviceResetArgs& a);

        // --- steps ---
        bool EnsureResources(game::gx::Device9 dev);  // compile shaders + create the mask RT, once
        void RebuildTargets();                        // mouseover + target (+ mounts) -> colored entries
        void StampSilhouette(game::gx::Device9 dev, const events::M2BatchDrawArgs& a, int idx);
        void StampOccluder(game::gx::Device9 dev, const events::M2BatchDrawArgs& a);
        void StampMask(game::gx::Device9 dev, const events::M2BatchDrawArgs& a, const float* color, bool clear,
                       game::gx::RenderTarget& rt, bool& cleared, bool depthTest);
        void EdgePass(game::gx::Device9 dev);         // composite the mask into the frame
        void DiagReadback(game::gx::Device9 dev);     // sample the mask back and log non-zero coverage

        // --- helpers ---
        bool ShouldStampBatch(game::gx::Device9 dev) const;
        int  FindTarget(void* model) const;           // model or any parent in the list
        bool IsModel(void* model, void* want) const;  // model or any parent equals want
        void AddTarget(unsigned long long guid, void* player, bool mouseover);
        void AddModel(void* model, const float* color, bool isPlayer, bool ancestors);
        void AddEntry(void* model, const float* color, bool isPlayer);
        void ColorForReaction(int reaction, float* outRgba) const;

        void LoadConfigNow();
        void ReloadConfigIfChanged();
        void Log(int level, const char* fmt, ...) const;

        struct Target { void* model; float color[4]; bool isPlayer; };

        Target                     targets_[kMaxTargets]{};
        int                        count_       = 0;
        void*                      playerModel_ = nullptr; // the local player's character, a mask occluder
        game::gx::RenderTarget     mask_{};                // target + mount silhouettes, edge-detected
        game::gx::RenderTarget     playerMask_{};          // player silhouette, subtracts the outline
        bool                       playerMaskCleared_ = false;
        void*                      colorPS_     = nullptr; // fills opaque batches into the silhouette mask
        void*                      cutoutPS_    = nullptr; // fills alpha-tested batches into the silhouette mask
        void*                      edgePS_      = nullptr; // edge-detects the mask into a line
        bool                       maskCleared_ = false;

        // Diagnostic counters: the first few stamps log the projection's depth orientation and the
        // draw result, so a silently rejected mask is visible in the log instead of just missing.
        unsigned                   diagLogged_ = 0;
        unsigned                   diagLastHr_ = 0;
        bool                       diagResourcesLogged_ = false;
        void*                      diagSys_ = nullptr;   // system-memory surface for the mask read-back
        int                        diagSysW_ = 0;
        int                        diagSysH_ = 0;
        unsigned                   diagTick_ = 0;

        OutlineStyle               style_{};
        OutlineStyle               saved_{}; // what the INI holds, for the unsaved-changes test
        std::string                iniPath_;
        unsigned long long         configStamp_ = 0;
        const WXL_Api*             api_         = nullptr;
    };
}
