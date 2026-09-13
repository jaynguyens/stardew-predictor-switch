# Switch 1.6.15 Year-1 typed-seed weather-search rules

This note defines the rules an exhaustive search should use when maximizing wet
days across Year 1 Spring and Summer. A **wet day** is Rain, Storm, or Green
Rain. Sun and Festival weather are dry for this metric.

## Confidence and source scope

The weather lifecycle and input-limit findings below come from a decompiled
Stardew Valley 1.6 source snapshot at commit
[`5225ef4`](https://github.com/Dannode36/StardewValleyDecompiled/tree/5225ef409e42a6159a82cf81200bf6eb315c9961),
checked against the current Stardew Valley Wiki. The Switch JKISS generator is
community reverse engineering rather than official Nintendo or ConcernedApe
documentation; its provenance is Oshawk's seed cracker and the upstream
predictor Switch PR, which reports live-game weather validation. The
single-precision input behavior is empirical: it is reproduced exactly by the
large-seed console observations below, but it isn't visible in the platform-
neutral decompiled C# source.

## Playable seed domain

The new-game Advanced Options textbox is numeric-only and has `textLimit = 9`.
The entered value is parsed as an unsigned integer. Therefore an exhaustive
search for seeds that a player can enter normally must cover:

```text
0 <= seed <= 999,999,999
```

That is exactly 1,000,000,000 candidates. Leading zeroes do not create distinct
numeric seeds. The browser predictor's URL parser accepts arbitrary-length
integer `id` values, so it can evaluate a 10-digit ID, but such an ID is outside
the vanilla seed textbox's normal input domain.

### Entered seed versus effective Game ID

Console observations show that the Switch new-game field behaves as if the
entered integer is converted through an IEEE-754 binary32 value. Every integer
through `16,777,216` (`2^24`) is exact. Above that point, some adjacent entries
round to the same effective Game ID, with the spacing increasing as the value
gets larger.

Examples confirmed against console-observed weather:

| Entered seed | Effective Game ID |
|---:|---:|
| `277770050` | `277770048` |
| `401102058` | `401102048` |
| `418923369` | `418923360` |
| `422049544` | `422049536` |

The web build therefore distinguishes `seed`, meaning the value typed into the
Switch new-game field, from `id`, meaning an exact internal Game ID. For
example, use `?seed=401102058`, not `?id=401102058`, to reproduce a manually
entered value.

The exhaustive scanner searches only stable entries where the entered seed is
already equal to its effective Game ID. This removes aliases without losing a
possible outcome, because every rounded effective ID is itself a valid integer
entry in the nine-digit domain.

Source: [`AdvancedGameOptions.cs`, lines 184-207](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley.Menus/AdvancedGameOptions.cs#L184-L207).
The [Advanced Game Options documentation](https://wiki.stardewvalley.net/Options#Advanced_Game_Options)
also documents the field's purpose, though not its digit limit.

## Year 1 Spring

| Day(s) | Rule | Wet? |
|---|---|---|
| 1, 2, 4, 5 | Forced Sun | No |
| 3 | Forced Rain | Yes |
| 13, 24 | Active festival weather | No |
| Remaining 21 days | Seeded 18.3% Rain roll | If the roll succeeds |

The perhaps surprising Spring 5 override follows from lifecycle timing: the
game advances its day counters before applying the `DaysPlayed + offset <= 4`
new-game override. This produces the player-visible sequence documented by the
[Weather page](https://wiki.stardewvalley.net/Weather): Spring 1, 2, 4, and 5
are sunny, while Spring 3 is rainy.

Source: [`Game1.getWeatherModificationsForDate`, lines 8323-8360](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley/Game1.cs#L8323-L8360)
and the [weather-data rules](https://wiki.stardewvalley.net/Modding:Weather_data#Weather_probability_by_type).

## Year 1 Summer

| Day(s) | Rule | Wet? |
|---|---|---|
| 1 | Forced Sun | No |
| 11, 28 | Active festival weather | No |
| 13, 26 | Forced Storm | Yes |
| One of 5, 6, 7, 14, 15, 16, 18, 23 | Green Rain | Yes |
| Other 22 days | Seeded summer precipitation roll | If the roll succeeds |

For target Summer day `D`, the precipitation threshold is:

```text
0.12 + 0.003 * (D - 1)
```

The source expression reads the current `Game1.dayOfMonth` while
`UpdateDailyWeather` is generating **tomorrow's** weather. That lifecycle is why
the target-day formula uses `D - 1`. For example, target Summer 2 uses 12.3%,
not 12.6%. A successful roll may become a storm, but either classification is
wet and therefore has the same search score.

Sources: [`GameStateQuery.cs`, lines 1625-1632](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley/GameStateQuery.cs#L1625-L1632)
and [`LocationWeather.UpdateDailyWeather`, lines 137-177](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley.Network/LocationWeather.cs#L137-L177).

Green Rain is chosen once per Summer from the exact candidate array above,
using a save-seeded random generator. Source:
[`Utility.isGreenRainDay`, lines 4680-4696](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley/Utility.cs#L4680-L4696).
The current [Weather page](https://wiki.stardewvalley.net/Weather#Green_Rain)
also includes Summer 15; any older list omitting 15 should not be used for the
scanner.

## Trout Derby and passive festivals

Summer 20 and 21 remain seed-driven and may be rainy. They must not be forced
sunny or removed from the optimization score.

The passive-festival check in `LocationWeather` passes a non-null location
context. `TryGetPassiveFestivalDataForDay` then skips entries without applicable
map replacements. Thus the general passive-festival sunny path does not imply
that every passive event forces clear weather; it applies to events whose map
replacement data affects the location context. Trout Derby is explicitly held
regardless of weather in the [event documentation](https://wiki.stardewvalley.net/Trout_Derby).

Sources: [`LocationWeather.cs`, line 167](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley.Network/LocationWeather.cs#L159-L177)
and [`Utility.TryGetPassiveFestivalDataForDay`, lines 5259-5295](https://github.com/Dannode36/StardewValleyDecompiled/blob/5225ef409e42a6159a82cf81200bf6eb315c9961/Stardew%20Valley/StardewValley/Utility.cs#L5259-L5295).

## Search-space accounting

Across the 56 Spring-and-Summer days there are:

- 9 forced-dry days;
- 4 guaranteed-wet days: Spring 3, Summer 13, Summer 26, and the Green Rain day;
- 43 seed-variable days: 21 in Spring and 22 in Summer.

The mathematical ceiling is therefore 47 wet days, although the probability of
all 43 variable rolls succeeding is extremely small. An implementation should
rank first by total wet days, then apply optional secondary perks only as
tiebreakers.

## Switch PRNG provenance

For Switch/Switch 2 prediction, use the JKISS implementation and the Stardew
1.6 seed-combining behavior, not the PC `System.Random` implementation.
[Oshawk's seed cracker](https://github.com/Oshawk/stardew-seed-cracker)
documents Switch and Stardew 1.6.15 support and contains the JKISS generator.
The upstream predictor's
[Switch/Switch 2 PR](https://github.com/MouseyPounds/stardew-predictor/pull/40)
states that the port came from Oshawk's implementation and records live-game
weather validation. These are the strongest available implementation sources,
but remain community reverse engineering.

## Scanner acceptance checklist

- Search the inclusive range `0..999_999_999` only.
- Convert typed entries through IEEE-754 binary32 before scoring, or equivalently
  search only entries which are unchanged by that conversion.
- Count Rain, Storm, and Green Rain as wet.
- Preserve Spring 5 as forced Sun.
- Use the target-day Summer formula with `D - 1`.
- Include Summer 15 among Green Rain candidates.
- Leave Trout Derby days 20 and 21 seed-driven.
- Exclude active-festival weather from wet counts.

## Observed calibration seeds

Before the exhaustive scan, the weather implementation was checked against the
following Reddit-observed Switch calendars:

| Entered seed | Effective ID | Observed Spring wet dates | Observed Summer wet dates |
|---:|---:|---|---|
| `8213213` (written as `008213213`) | `8213213` | 3, 7, 9, 10, 12, 17, 19, 20, 21, 25, 28 | Not reported |
| `8478309` | `8478309` | 3, 7, 8, 9, 10, 14, 22, 23, 25, 26, 27 | 6, 8, 12, 13, 25, 26, 27 |
| `24680` | `24680` | 3, 9, 10, 11, 12, 14, 15, 18, 26, 27 | 5, 10, 13, 18, 21, 26 |
| `77445` | `77445` | 3, 6, 14, 28 | 13, 16, 17, 26 |
| `2171145` | `2171145` | 3, 6, 14, 16, 18, 25, 26 | Not reported |
| `277770050` | `277770048` | 3, 6, 10, 19, 22 | Not reported |
| `418923369` | `418923360` | 3, 9, 10, 17, 28 | 6, 7, 9, 13, 14, 24, 26 |
| `422049544` | `422049536` | 3, 27 | 13, 14, 24, 26 |

The source posts sometimes omit Spring 3 from their written list because it is
forced rain; the normalized table includes it. The corrected local page and
scanner match every reported date. Sources: [Switch 2 world-seed
report](https://www.reddit.com/r/StardewValley/comments/1vlmh6z/switch_2_world_seeds/)
list](https://www.reddit.com/r/StardewValley/comments/1vfv9mc/list_of_stardew_switch_seeds_16_for_those_that/),
and [additional Switch 1.6 observations](https://www.reddit.com/r/StardewValley/comments/1vi6k4v/more_stardew_switch_seeds_community_center/).

An on-console report for entered seed `401102058` also found no random Spring
rain before day 12. Its effective ID `401102048` predicts Spring rain on days
3, 12, 18, and 22, reproducing that result through the reported date. This
observation was the regression which exposed the input-precision bug.

The executable regression is `tests/switch-rain-search.test.sh`. The scanner's
RNG implementation was also differentially checked against the repository's
checked-in `xxhash.min.js` and `jk-random.js` primitives.

## Exhaustive nine-digit results

`tools/search-switch-rain.cpp` was compiled with Clang `-O3` and run across the
full half-open entered-seed range `[0, 1,000,000,000)`. Separate scans of the
two half-ranges reproduced the same winners: two distinct effective IDs below
500 million and one above it.

The earlier exact-integer scan incorrectly reported 28 wet days because it
scored IDs which can't remain unchanged when typed into the Switch field. After
normalizing inputs and rescanning the full nine-digit domain, the maximum is
**27 wet days**. Three distinct effective IDs tie:

| Entered seed | Spring | Summer | Total |
|---:|---:|---:|---:|
| `96194896` | 14 | 13 | 27 |
| `656913728` | 13 | 14 | 27 |
| `4315107` | 11 | 16 | 27 |

The balanced recommendation is `96194896`. Its exact wet dates are:

- Spring: 3, 6, 8, 9, 11, 12, 14, 19, 20, 21, 22, 25, 26, 28;
- Summer: 6, 7, 8, 9, 12, 13, 15, 16, 17, 19, 24, 25, 26;
- Green Rain: Summer 16; deterministic storms: Summer 13 and 26.

This is only a weather score; cart stock, night events, and route timing are
outside the objective.

The corrected single-season maxima are:

- Spring: `681786432`, with 17 Spring + 9 Summer = 26 wet days;
- Summer: `64339760`, with 8 Spring + 18 Summer = 26 wet days. Three other
  distinct effective IDs also have 18 wet Summer days but lower combined totals.

All three winners are algorithmic predictions, not yet console-observed seeds.
In particular, don't reuse the superseded `401102058` or `604375215`
recommendations from the original scan. After input normalization,
`604375215` resolves to `604375232`, but one on-console report still disagrees
with that calendar; it remains a separate unresolved observation.
