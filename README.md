# wxl-mana-unit-outline

Outline features from World of Warcraft retail for WarcraftXL: a reaction-colored silhouette
outline on the unit under the cursor and on the current target.

The module is a render script. It subscribes to the core's render events, stamps the selected
units' silhouettes into an off-screen mask, and edge-detects that mask back into the frame at the
world -> UI boundary, so the outline sits under the interface like the retail glow.

## Features

- Reaction-colored outline: red for hostile, yellow for neutral, green for friendly. A neutral unit
  that is fighting the player is shown red, matching how the client's own unit frame reacts to combat.
- Outlines both the mouseover unit and the current target.
- **Mounted units outline their mount too.** The rider hangs off the mount in the model attachment
  chain, so the whole ancestor chain of a selected unit is outlined with the same color.
- Alpha-tested batches (wings, hair cards, cloaks) are cut out correctly.
- The local player is punched out of the mask, so a target hidden behind the player is not outlined
  on top of them.
- **Every outline fades in**: it appears at full strength and eases down to 50% over 0.4s, then
  holds there. A quest giver is the exception and never fades: it stays at full strength so a
  "quest here" marker cannot be confused with a selected unit. Both are documented under
  `ColorQuestGiver` below.
- Fully configurable look through `wxl-unit-outline.ini` (see below), reloaded live. The INI is
  polled a few times a second rather than stat-ed every frame.

## Configuration

`wxl-unit-outline.ini` sits next to `wxl-unit-outline.dll`. Edits are picked up within about a
second while the game runs; values that fail to parse or fall outside their range keep the default.

The same settings can be edited in-game from the core overlay panel **"Unit Outline"**: drag the
sliders, toggle the checkboxes, pick the reaction colors, then **Save** to write them back to the
INI (or **Revert** to discard). Panel edits apply live even before they are saved.

| Key | Default | Meaning |
|---|---|---|
| `Enable` | `1` | Master switch. `0` disables the outline. |
| `OutlineMouseover` | `1` | Outline the unit under the cursor. |
| `OutlineTarget` | `1` | Outline the current target. |
| `IncludeMount` | `1` | Also outline a selected unit's mount (its model-chain ancestors). |
| `HideOnPlayer` | `1` | Never outline your own character, even when it is the mouseover or the target. `0` outlines yourself like any other unit. |
| `Thickness` | `1.75` | Edge width in screen pixels, `0.5`-`6.0`. |
| `Intensity` | `4.0` | Edge brightness / accumulation weight, `0.5`-`4.0`. |
| `Opacity` | `1.0` | Alpha scale on the default outline look, `0.0`-`1.0`; lower fades it out. |
| `Threshold` | `0` | Edge cutoff, `0.0`-`0.5`; raise for a crisper, thinner line. At `0` nothing is clipped and `Softness` alone decides where the line ends. |
| `Softness` | `0.225` | Blur applied to the silhouette before the edge is detected, `0.0`-`0.5`; `0` turns it off. Softens the rim so pointy models (spikes, shoulders, weapon tips) do not get a hard aliased edge. |
| `MouseoverBrightness` | `1.0` | Extra brightness multiplier for the mouseover outline only, on top of `Intensity`, `0.0`-`4.0`; `1.0` matches a target outline. |
| `Occlusion` | `0` | `1` depth-tests the silhouette against the world so terrain and buildings hide the outline behind them. Off by default: this client's D3D9On12 path rejects the depth-tested stamp at multisampling x1 (at x2 the render-target/depth sample mismatch drops the test), so the outline is only reliably visible with it off. |
| `ColorHostile` | `255,0,0` | Hostile reaction color, `R,G,B` (0-255) or `#RRGGBB`. |
| `ColorNeutral` | `255,255,0` | Neutral reaction color. |
| `ColorFriendly` | `0,200,0` | Friendly reaction color (also used when the reaction is unknown). |
| `ColorFriendlyPlayer` | `40,120,200` | Friendly player color, reaction 4+ only: blue, the client's own friendly-player tint, so a player reads differently from an NPC. A player you can attack keeps `ColorHostile`/`ColorNeutral`. |
| `ColorQuestGiver` | `255,209,0` | Color for a quest giver that has an available or turn-in quest for you, and whose quests are not all far below your level. Such a unit is outlined always, without being pointed at, and at **full strength**: unlike the mouseover and target outlines it does not fade down to the settled opacity, so it stays plainly visible. Fed by the paired server module `mod-mana-quest-outline` over the addon channel; without that module installed the key has no visible effect. |

## Requirements

- `d3dcompiler_47.dll` must be available for the outline pixel shaders to compile. Without it the
  module stays off.

## License

GPL-3.0-or-later, see `LICENSE`.
