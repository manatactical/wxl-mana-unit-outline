# wxl-unit-outline

Outline features from World of Warcraft retail for WarcraftXL: a reaction-colored silhouette
outline on the unit under the cursor and on the current target.

The module is a render script. It subscribes to the core's render events, stamps the selected
units' silhouettes into an off-screen mask, and edge-detects that mask back into the frame at the
world -> UI boundary, so the outline sits under the interface like the retail glow.

## Features

- Reaction-colored outline: red for hostile, yellow for neutral, green for friendly.
- Outlines both the mouseover unit and the current target.
- **Mounted units outline their mount too.** The rider hangs off the mount in the model attachment
  chain, so the whole ancestor chain of a selected unit is outlined with the same color.
- Alpha-tested batches (wings, hair cards, cloaks) are cut out correctly.
- The local player is punched out of the mask, so a target hidden behind the player is not outlined
  on top of them.
- Fully configurable look through `wxl-unit-outline.ini` (see below), reloaded live.

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
| `Thickness` | `2.5` | Edge width in screen pixels, `0.5`-`6.0`. |
| `Intensity` | `1.6` | Edge brightness / accumulation weight, `0.5`-`4.0`. |
| `Opacity` | `1.0` | Alpha scale on the default outline look, `0.0`-`1.0`; lower fades it out. |
| `Threshold` | `0.02` | Edge cutoff, `0.0`-`0.5`; raise for a crisper, thinner line. |
| `MouseoverBrightness` | `1.0` | Extra brightness multiplier for the mouseover outline only, on top of `Intensity`, `0.0`-`4.0`; `1.0` matches a target outline. |
| `ColorHostile` | `255,0,0` | Hostile reaction color, `R,G,B` (0-255) or `#RRGGBB`. |
| `ColorNeutral` | `255,255,0` | Neutral reaction color. |
| `ColorFriendly` | `0,255,0` | Friendly reaction color (also used when the reaction is unknown). |

## Requirements

- `d3dcompiler_47.dll` must be available for the outline pixel shaders to compile. Without it the
  module stays off.

## License

GPL-3.0-or-later, see `LICENSE`.
