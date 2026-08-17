# Contributing to upstream OpenSpace — an evidence-based guide

An empirical analysis of how code actually enters `OpenSpace/OpenSpace`, and a playbook for
producing a PR that passes review.

**Corpus analysed** (2026-08-17, `upstream/master` @ `56e29b54b8`):

| Source | Volume |
| --- | --- |
| Commits on `master` | 12,617 (1,500–3,000 sampled in depth) |
| Pull requests | 200 merged (2025-02-28 → 2026-08-17), 100 closed, 6 open |
| Inline review comments | 1,131 across 51 PRs |
| Top-level review summaries | 49 |
| Written rules | `CONTRIBUTING.md`, `.editorconfig`, `.clang_tidy`, `Jenkinsfile`, docs.openspaceproject.com, codegen README |

Everything below is grounded in that data. Where the project's written rules and its actual
practice disagree, both are given — practice is what gets you merged.

---

## Part 1 — The written rules

These are short, and most contributors stop here. They are necessary but nowhere near
sufficient.

### 1.1 `.github/CONTRIBUTING.md`

The only formal contribution document. Its operative requirements:

- Contribute **through a fork + pull request**. A core developer reviews and integrates into
  `master`.
- **Prefer an accompanying issue** ("created by you or not") which the PR addresses.
- Use the phrase **`(closes #XXX)`** in the PR text.
- Branch naming for external PRs: **`pr/feature`**, where `feature` is a short descriptive
  name.
- The Code of Conduct is "in effect at all times".

> ⚠️ **The branch-naming rule is dead letter.** Across 200 merged PRs, *zero* used a `pr/`
> branch. Actual distribution of merged head branches: `feature/…` 97, `issue/…` 78, flat
> name 23, `docs/…` 2. See §2.3 for what to actually use.

### 1.2 Machine-enforced style — `.editorconfig`

Applies to every file; your editor should honour it automatically.

```ini
[*]
charset = utf-8
indent_style = space
indent_size = 4
insert_final_newline = true
trim_trailing_whitespace = true

[data/**.asset]
indent_size = 2

[**.glsl]
indent_size = 2
```

Consequences people get caught on:
- **C++ = 4 spaces. Assets and GLSL = 2 spaces.** Mixing these is an instant review comment.
- **Every file ends with a newline.** "Missing new line at end of file" appears verbatim in
  review threads (PR #3801 on a `.glsl`, PR #3784 on `ellipsoid.glsl`).
- No trailing whitespace, ever.

### 1.3 `.clang_tidy`

A large, deliberately curated check set: `bugprone-*`, `clang-analyzer-*`,
`cppcoreguidelines-*`, `google-*`, `llvm-*`, `misc-*`, `modernize-*`, `performance-*`,
`readability-*` — each with explicit opt-outs. Notable configuration that tells you what the
team cares about:

- `bugprone-assert-side-effect.AssertMacros = ghoul_assert,ghoul_precondition` — the project's
  assert macros are known to the linter; **no side effects inside them**.
- `performance-unnecessary-value-param.AllowedTypes` lists every `glm` vector/matrix/quat type
  — **pass small glm types by value**, don't `const&` them.
- `readability-inconsistent-declaration-parameter-name.Strict = True` — parameter names in the
  header and the `.cpp` **must match exactly**.
- Explicitly disabled: `readability-magic-numbers`, `cppcoreguidelines-avoid-magic-numbers`,
  `readability-identifier-length`, `modernize-use-auto`, `modernize-use-trailing-return-type`.
  (Magic numbers are still a *style-guide* rule — the linter just isn't the enforcer.)

### 1.4 CI — `Jenkinsfile`

There are **no GitHub Actions**. CI is Jenkins at `dev.openspaceproject.com`, running in
parallel:

| Lane | What it does |
| --- | --- |
| `tools` | `cppcheck --enable=all` over `include modules src tests`, with `support/cppcheck/suppressions.txt` |
| `linux_gcc_make` | build + `codegentest`, `SGCTTest`, `GhoulTest`, `OpenSpaceTest` |
| `linux_clang_make` | build + the same four test binaries |
| `windows_msvc` | build + tests |

Every module is force-enabled (`-DOPENSPACE_MODULE_<NAME>=ON` for every directory in
`modules/`), so **your change must compile with all modules on**, on three toolchains.

> Compiler-version reality: a maintainer noted "Our continuous integration machines are
> running on [GCC] 13, so we missed this one" (PR #3548). Newer-compiler breakage is a
> recurring, welcome class of external contribution — but it also means CI green ≠ builds
> everywhere.

Image regression tests run separately at `regression.openspaceproject.com` (see §5.4).

### 1.5 The C++ style guide

`docs.openspaceproject.com/latest/contribute/development/coding-style.html`. Full rule set,
condensed:

**Meta**
- *Rule 0 — Boy Scout rule*: fix violations you encounter while editing.
- *Rule 1*: violations are allowed **if they improve readability**. Readability wins.

**Layout**
- 90 columns max (rationale: two files side-by-side).
- 4-space indent; TABs converted to spaces.
- Class sections ordered `public` → `protected` → `private`; omit empty sections.
- Namespaces lowercase, max 2 levels deep (3rd only for `internal`). **Outermost namespace
  content is not indented; nested namespaces are.**
- Constructor init lists: one entry per line, leading `:` / `,`, `{` on its own line.
- Long calls: one argument per line, closing `);` on its own line. Exceptions: `std::format`
  args may stay compact; Qt `connect` groups object+signal.
- Operator chains split at logical barriers, aligned with the first operand.

**Naming**
- Functions are lowerCamelCase **verbs**. `normalize(v)` mutates; `v.normalized()` returns a
  copy.
- **No `get` prefix** when returning a value — `employee.name()`. `get` only when the value
  comes back through a reference parameter.
- Booleans (variables *and* methods) use `is` / `has` / `should` / `can`. **Never negated**
  (`hasError`, not `hasNoError`).
- Private members `_underscored` (plain data structs exempt).
- Counts prefixed `n` (`nPoints`); entity indices prefixed `i` (`iTable`).
- Name length ∝ scope length.
- Don't abbreviate dictionary words (`command`, not `cmd`). Acronyms are not uppercased in
  identifiers: `exportHtmlSource()`.
- `enum class` preferred; values are not SCREAMING_CASE; set the first value `= 0` explicitly.

**Includes** — grouped, alphabetical within group, blank line between groups:
1. own `.h` (in `.cpp`) / parent-class header (in `.h`)
2. project headers, grouped by top directory (`modules/`, `openspace/`, `ghoul/`)
3. external libraries
4. standard library

Relative paths only, never absolute. Don't repeat includes already present in your own/base
header — **except `Property` type headers, which are always included**. Prefer forward
declarations in headers.

**Language**
- Casts always explicit: `static_cast<float>(i)`.
- `auto` **sparingly** — only iterators, `std::chrono`, lambdas, structured bindings, or when
  the type is spelled on the same line.
- `nullptr`, never `0`/`NULL`.
- Implicit truth-test only for `bool` and pointers. Numbers compare explicitly:
  `if (nLines != 0)`.
- No complex or side-effecting conditionals; name the boolean first. `if (int c = f(); c > 0)`
  is allowed to narrow scope.
- Definitions live in `.cpp`/`.inl`, never inline in the class body. Templates go in `.inl`,
  included at the bottom of the header.
- Magic values (other than 0, 1, −1) become named constants.
- Prefer `<algorithm>`; prefer clarity over micro-optimisation outside hot loops.

**Doxygen**
- `//` for all comments, including multi-line. `/** */` only for Doxygen.
- Member variables and enum values: `///` on the **preceding** line. Everything else `/** */`.
- Order: description (ends with `.`) → blank → `\param`/`\return`/`\tparam` (no trailing
  period) → blank → `\throw`/`\pre`/`\post` → blank → `\see`.
- Use `\`, not `@`. Singular: `\return`, `\throw`.
- **No `\brief`** — the first sentence is the brief. No documentation for namespaces.

---

## Part 2 — What the commit history actually shows

### 2.1 The repository switched from merge commits to squash merges

Merge commits per year on `master`:

| Year | Commits | With `(#NNNN)` squash suffix | Merge commits |
| --- | --- | --- | --- |
| 2021 | 1024 | 64 | 139 |
| 2022 | 945 | 69 | 147 |
| 2023 | 837 | 100 | 79 |
| 2024 | 446 | 133 | 13 |
| 2025 | 483 | 133 | 5 |
| 2026 | 398 | 95 | **0** |

**Implication:** your PR will be **squash-merged**. GitHub will append `(#NNNN)` to your PR
*title*, and your PR title becomes the permanent commit subject. Individual commit messages
inside the branch are discarded.

> This is the single highest-leverage fact in this document. **Write the PR title as if it
> were the commit message**, because it becomes one.

Historically (pre-2024) the merge subject was
`Merge pull request #3634 from benpm/issue/3596-saturn-ring-shadow`; that form is gone.

### 2.2 Commit-message conventions (n = 1,500 non-merge commits)

| Property | Measurement | Rule |
| --- | --- | --- |
| Imperative verb start | 921 / 1500 (61%) + variants | **Use imperative mood** |
| Past tense (`Added`, `Fixed`) | 30 / 1500 (2%) | Avoid |
| Starts lowercase | 50 / 1500 (3%) | **Capitalise** the first word |
| Ends with a period | 7 / 1500 (0.5%) | **No trailing period** |
| Contains `(closes #N)` | 190 / 1500 (13%) | Use when an issue exists |
| Has a body | 375 / 1500 (25%) | Body optional |
| Contains backticks | 53 / 1500 | Used for identifiers |

Subject length is **not** the conventional 50/72:

```
min 3 | p25 35 | median 52 | p75 70 | p90 90 | max 213
443 / 2000 subjects exceed 72 characters
```

The project prefers a **complete, specific sentence** over a terse one. Two clauses joined by
`.` in a single subject line is normal.

Top opening verbs: `Add` (478), `Update` (391), `Fix` (353), `Remove` (158), `Make` (120),
`Move` (63), `Use` (53), `Change` (43), `Correctly` (39), `Prevent` (38).

Note `Correctly …` and `Prevent …` and `Don't …` — the house style describes the **corrected
behaviour**, not the bug:

```
Correctly unload lazily loaded images (closes #4073)
Do not crash when trying to load TLE file that does not exist (closes #3962)
Prevent division by 0 error in ScreenSpaceSkybrowser (closes #3696)
Don't update SceneGraph nodes unless they are fully initialized (closes #3498)
Make asset path comparison case-insensitive on Windows (closes #3855)
```

Multiple issues, and multi-clause subjects, are both idiomatic:

```
Add the ViaMD shaders to the deploy script (closes #3929, #4102)
Correctly resize the CEF browser when any window in a collection is resized (closes #3686). Warn about the behavior when showing 2D elements on multiple windows
```

**Template:**

```
<Imperative verb> <specific description of the corrected/added behaviour> (closes #NNNN)
```

### 2.3 Branch naming

Written rule: `pr/<feature>`. Observed on 200 merged PRs:

| Prefix | Count |
| --- | --- |
| `feature/<slug>` | 97 |
| `issue/<number>[-<slug>]` | 78 |
| flat name (`fix-gcc-builds`, `patch-1`) | 23 |
| `docs/<slug>` | 2 |
| `pr/<feature>` | **0** |

**Recommendation:** match the team, not the doc.
- Fixing a filed issue → `issue/3596-saturn-ring-shadow`
- New capability → `feature/hd-ring-shadow`
- Small external drive-by → a flat descriptive name is accepted (`fix-powerscaling-case-sensitive-includes`)

All four of `benpm`'s merged upstream PRs used the team convention rather than the
documented `pr/` one: `issue/3596-saturn-ring-shadow` (#3634), `feature/hd-ring-shadow`
(#3749), `issue/3776-ring-shadows-fisheye` (#3784), `feature/shadows` (#3801).

### 2.4 Commit size

Non-merge commits: **median 2 files changed**, p75 6, p90 29. Merged PRs: **median 5 files,
80 additions, 22 deletions**; p90 48 files / 1,089 additions.

External-contributor PRs are markedly smaller: **median 3 files changed**, p90 10.

---

## Part 3 — How PRs actually get reviewed

### 3.1 Who decides

Reviewers, by inline-comment volume across the sampled 51 PRs:

| Reviewer | Comments | Share |
| --- | --- | --- |
| `alexanderbock` | 573 | 51% |
| `WeirdRubberDuck` (Emma Broman) | 357 | 32% |
| `Roxeena` | 107 | 9% |
| `engbergandreas` | 29 | |
| `Copilot` (automated) | 25 | |
| others | ~40 | |

**Two people write 83% of all review feedback.** `alexanderbock` optimises for *code form*
(formatting, includes, naming, C++ idiom, `codegen` usage). `WeirdRubberDuck` optimises for
*user-facing behaviour* (documentation text, property semantics, does it actually work when I
run it, is it discoverable). `Roxeena` asks "why is this here?" about anything outside the
stated scope.

Write for these three reviewers specifically.

### 3.2 Review volume and outcomes

- Reviews per merged PR: **median 2**, p75 5, p90 11, max 96.
- Review states across 200 merged PRs: 726 `COMMENTED`, 150 `APPROVED`, 37 `CHANGES_REQUESTED`.
- **46 / 200 merged PRs (23%) had zero reviews.**
- 11 / 100 recently closed PRs were closed **without** merging.
- Labels are essentially unused (only 7 label applications across 200 PRs) — don't expect
  triage by label.

### 3.3 The two regimes

The data splits cleanly:

**Regime A — the small, obviously-correct fix (merges with no review at all).**
15 of 29 external PRs merged with zero reviews. Their shape: 1–4 files, a single well-scoped
defect, a body that explains *why* in 2–5 sentences with an external citation.

> #4134 `Fix powerscaling case sensitive includes` — "Some module shaders still used
> `powerScaling` instead of `powerscaling` in their includes, which broke shaders on Linux and
> other case sensitive filesystems."

> #3643 `webbrowser: Fix CEF deprecation notice and crash` — cites the upstream CEF issue,
> pastes the fatal log line, states that following the migration instructions fixes both.

**Regime B — the feature (10–96 review rounds, weeks of iteration).**
PRs #3652 (53 files), #3926 (43), #3801 (27) each drew multi-round `CHANGES_REQUESTED`.
Typical blocking objections were *not* about the algorithm — they were about form, docs, and
usability.

**Strategy: bias hard toward Regime A.** Split a feature into a sequence of small PRs; the
first one earns you credibility and the reviewers' shorthand.

### 3.4 What actually blocks a merge

Verbatim from `CHANGES_REQUESTED` summaries:

- **It doesn't work when I run it.** > "Nice, but unfortunately, there is a detail in the code
  that makes it not work." (#4130) — reviewers *run* your branch. Test the feature end-to-end
  from a clean profile before pushing.
- **Compiler warnings.** > "There are still a bunch of compiler warnings as well" followed by
  a paste of C4715/C4267 warnings (#3652). **Zero new warnings on MSVC.**
- **Unreadable structure.** > "This huge if-else statement is really difficult to
  read/overview. How about breaking out the texture loading for each case into individual
  functions" (#4056).
- **Code duplication.** > "There's quite a bit of code duplication … Would it make sense to
  break these parts out into a component" (#3652).
- **Missing/insufficient documentation.** The most common non-code blocker by far.
- **Style noise drowning the review.** > "Had a lot of code style comments, so starting with
  those as they distract a bit from commenting on the logic" (#3897, ×2). Style errors cost
  you an entire review round before anyone looks at your logic.
- **Discoverability/navigation gaps.** > "The renderables also need bounding spheres computed,
  so that the 'Jump to' and 'Fly to' commands can be used" (#3926).
- **Runtime spam.** > "Should these be debug-level messages or are they intended for the
  user?" (#3652).

---

## Part 4 — The unwritten rules (from 1,131 inline comments)

None of the following are in the style guide. All of them are enforced.

### 4.1 Diff hygiene — the cheapest points you can win

- **Never reorder unrelated code.** > "Whats the reason for switching the order of the local
  and global renderer here? It makes reading the diff more difficult than it need to be and
  would say that we should keep the old order" (#3749).
- **Never delete blank lines that aided readability.** > "I think having the empty lines here
  improved the readability" … "Same here" (#3749).
- **No stray files from other branches.** > "Are these artifacts from an outdated branch
  relative master? I am very confused why these changes are here." (#3801). Note that PR #3597
  accidentally carried a `modules/kameleon/ext/kameleon` submodule bump — check
  `git diff --stat upstream/master` for submodule pointer changes before opening.
- **No unrelated submodule bumps.** If your work needs a Ghoul/SGCT change, that is a
  *separate PR in that repository*, and reviewers will ask: > "Are you gonna have a separate PR
  in the Ghoul repository later?" (#3801).
- **No leftover debug/dead code.** > "Is this code meant to stay? Or should it be removed?"
  … "It doesn't look like these are used at the moment?" (#3801).
- **Copyright header year must be current.** The header reads `Copyright (c) 2014-2026` on
  today's master. Reviewers correct this literally: > "2025" (#3801, ×5 on new shader files),
  > ```suggestion * Copyright (c) 2014-2025 *``` (#3652).
- **Line-length violations are called out individually**, repeatedly, on the same PR: > "This
  line is longer than the line limit of 90 characters" … "Here again" … "And here" (#3801).

### 4.2 The line-break rule reviewers apply

Beyond the published rule, one clarification appears explicitly:

> "Fix indentation here. Also, as soon as we line break, all arguments should be line
> breaked" (#3897)

So this is wrong:

```cpp
std::for_each(std::execution::par, blockIndices.begin(), blockIndices.end(),
    [&](size_t blockIdx) {
```

and this is right:

```cpp
std::for_each(
    std::execution::par,
    blockIndices.begin(),
    blockIndices.end(),
    [&](size_t blockIdx) {
        // …
    }
);
```

`alexanderbock` frequently posts `suggestion` blocks whose only purpose is "Columnwidth
fixing" — reformatting a call into a `std::format` multi-line shape to fit 90 columns.

### 4.3 C++ idiom specifics

- **`explicit` on single-argument constructors.** > ```suggestion explicit TuioEar(int port = 3333);``` (#3813)
- **Unnamed parameters when unused** in overrides: `void render(const RenderData& data, RendererTasks&)`,
  `void update(const UpdateData&)` (#3897).
- **`const` wherever it survives.** > "If we delete the second assignment further below (which
  doesn't do anything), we can keep these variables `const`" (#3749).
- **Spell the floating-point type in glm literals.** > ```suggestion glm::dvec4(directionToSunWorldSpace, 0.0));```
  — "To signal immediately that it is meant to be a `double`" (#3749, applied 4×). Use `0.0`
  for double, `0.f` for float.
- **`std::format` in log macros**, never concatenation:
  ```cpp
  LERROR(std::format("Error executing 'filter': {}", lua_tostring(_state, -1)));
  ```
- **Don't `std::move` an rvalue reference parameter that's already `&&`.** > "No need to move
  since `decodedData` is already a `&&`" (#3926).
- **Throw rather than degrade silently.** > "Maybe this should throw a `ghoul::RuntimeError`
  instead? To make this class not work at all if the dependent node doesn't exist?" (#3926);
  > "Blanket-ignoring errors might be confusing for the user if they start messing with the
  shaders" (#3801).
- **Don't hand-roll what Ghoul provides.** > "Could we use a `ghoul::opengl::TextureUnit` for
  this? To make sure there is no clashes"; > "I remember vaguely that some of this code is
  already included in the `ghoul::opengl::Texture`, could we use that to avoid duplicated
  code?" (#3801).
- **Header guard form:** `__OPENSPACE_CORE___DYNAMICFILESEQUENCEDOWNLOADER___H__`, with the
  matching trailing comment on `#endif` (#3652). Module headers use the module token, e.g.
  `__OPENSPACE_MODULE_BASE___RENDERABLESWITCH___H__`.
- **Constants belong at the top of the `private:` section**, and can be
  `static constexpr inline` (#3926).
- **Naming semantics matter**, not just form: > "`_isStereo` … more aligned with the 'isUrl'
  setting, and 'use' makes it sound like it's a choice controllable by the user, to me. In this
  case, it isn't." (#4056)
- **Drop redundant prefixes.** > "I'd probably say to rename these two: `forwardTexture`,
  `backwardTexture` … The 'ring' is selfexplanatory from the call side, since it will be
  `_ringComponent->forwardTexture`" (#3749).
- **Don't abbreviate file names either.** > "Note for the filename, please do not abbreviate
  it. Instead just name it *debug_texture_fs.glsl*" (#3801, on `dbg_tex_fs.glsl`).

### 4.4 The `codegen` contract — where most form errors live

Every `Renderable`, `Translation`, `Rotation`, `Scale`, `Task`, and Lua binding goes through
`support/coding/codegen`. The canonical shape (from
`modules/base/rendering/renderableswitch.cpp`):

```cpp
#include <modules/base/rendering/renderableswitch.h>   // own header first

#include <openspace/documentation/documentation.h>     // project headers, grouped
#include <openspace/util/updatestructures.h>
#include <ghoul/misc/assert.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <optional>                                    // std last

namespace {
    using namespace openspace;

    constexpr Property::PropertyInfo DistanceThresholdInfo = {
        "DistanceThreshold",                            // identifier
        "Distance threshold",                           // GUI name
        "Threshold in meters for when the switch happens between the two renderables.",
        Property::Visibility::AdvancedUser
    };

    // Can be used to render one of two renderables depending on the distance between the
    // camera and the object's position.
    //
    // The two renderables are specified separately: `RenderableNear` and `RenderableFar`.
    struct [[codegen::Dictionary(RenderableSwitch)]] Parameters {
        // The renderable to show when the camera is closer to the object than the
        // threshold.
        std::optional<ghoul::Dictionary>
            renderableNear [[codegen::reference("core_renderable")]];

        // [[codegen::verbatim(DistanceThresholdInfo.description)]]
        std::optional<double> distanceThreshold [[codegen::greaterequal(0.0)]];
    };
} // namespace
#include "renderableswitch_codegen.cpp"

namespace openspace {

Documentation RenderableSwitch::Documentation() {
    return codegen::doc<Parameters>(
        "base_renderable_switch",
        Renderable::Documentation()
    );
}
```

Hard-won rules from the review threads:

1. **`PropertyInfo` is only for real properties.** > "This is not the documentation of any
   existing property, but just for one of the input parameters. In these cases, we do not
   create a `PropertyInfo`, but instead just add the documentation to the correct parameter"
   (#3897, said twice).
2. **Reuse the description via `[[codegen::verbatim(XInfo.description)]]`** rather than
   duplicating prose.
3. **`#include "…_codegen.cpp"` goes immediately after `} // namespace`**, on the very next
   line, with no blank line. > "Changed this in the other files as well as it was super
   brittle" (#3926).
4. **Property construction order in the constructor**, per property, in this exact sequence:
   ```cpp
   _stride = p.stride;
   _stride.onChange([this]() { _vectorFieldIsDirty = true; });
   addProperty(_stride);
   ```
   > "Each property should be added after the code that reads the parameter and adds the
   onchange." (#3897)
5. **Validate in the attribute, not in code**, where possible: `[[codegen::inrange(1, 65535)]]`,
   `[[codegen::greaterequal(0.0)]]`, `[[codegen::notempty()]]`, `[[codegen::color()]]`,
   `[[codegen::identifier()]]`, `[[codegen::datetime()]]`, `[[codegen::inlist(...)]]`,
   `[[codegen::reference("core_renderable")]]`, `[[codegen::key("Name")]]`,
   `[[codegen::private()]]`.
6. **The doc string passed to `codegen::doc<Parameters>` is a stable public ID** in the form
   `<module>_<classname_lowercase>` (e.g. `base_renderable_switch`,
   `solarbrowsing_renderablesolarimagery`). It appears in published documentation — don't
   churn it.
7. **Choose `Property::Visibility` deliberately**: `User`, `AdvancedUser`, `Developer`,
   `Hidden`. Reviewers challenge the choice: > "This seems like a developer visibility
   property? How often does one need to change this?" (#3801).
8. **Don't put min/max values in a property description.** > "we generally avoid specifying min
   and max values in the description" (#3897).

### 4.5 Property/renderable documentation prose

This is the single largest category of review feedback (68 of 1,131 comments explicitly about
comments/documentation; ~88 touch properties). Rules extracted from the suggestion blocks:

- The comment block above `struct Parameters` is **user-facing documentation** and is
  published. Write it for an asset author, not a C++ developer.
- Wrap it at 90 columns, `//` style, blank `//` line between paragraphs.
- **Backtick every identifier**: `` `Renderable` ``, `` `LoadingType` ``, `` `SourceFolder` ``,
  `` `RenderableNear` ``.
- Refer to types by their exact class name — > "I know its double, but technically
  `KameleonVolumeToFieldlinesTask` is the name of the task" (#3715).
- Structure "if X then Y is required" explicitly; the WSA docs PR (#3715) was praised
  specifically for documenting *which parameters are required when*.
- Reviewers will grammar-edit you. Expect suggestions on "that" vs "which", "Wether" →
  "Whether", "occured" → "occurred". Proof-read.
- Deprecations get a user-visible message:
  `"RenderablePlaneImageLocal is deprecated, use RenderablePlaneImage instead"` (#4056).

### 4.6 GLSL conventions (not documented anywhere — all from review)

- **2-space indent** (`.editorconfig`), 90-column limit still applies, newline at EOF.
- **Declaration order: `in`/`out` variables first, then `uniform`s.** > "For the GLSL coding
  style, we have the `in/out` variables at the top before the `uniform`s" (#3749).
- **Sample single-channel textures through `.a`, not `.r`** — > "We usually use the alpha
  channel to signal that we meant to access a 1-dimensional texture and not the red channel"
  (#3749). *(Caveat established in that same thread: verify it for your texture format —
  the contributor found `.a` did not work for that particular ring texture.)*
- Write float literals in a form the compiler can fuse: > "`frag.color.a * opacity + (1.0 - shadow) * 0.5`
  … More chance for the GLSL compiler to detect the fused multiply-add and optimize" (#3749).
- Keep `#ifdef` blocks consistent between declaration and use (#3749).
- **Comment the mathematics.** > "These calculations are hard to understand when you are not
  familiar with what is happening. Could we maybe add some comments that explain the formulas
  and where they are coming from?" (#3801).
- Don't duplicate a trivial shader — > "isn't this a generic shader? We could make one and
  reuse it in other places" (#3801).

### 4.7 Asset (`.asset`) conventions

- **2-space indent.**
- **Use string concatenation for generated names**, not interpolation:
  ```lua
  Identifier = "celestial-globe-" .. name,
  Name = "Celestial Globe - " .. name,
  ```
  > "Normally we use string concatination in other examples" (#3801, ×3).
- **Deinitialize in reverse order of initialization** — "to make sure we do not remove a parent
  before the child" (#3801).
- **Every asset needs an `asset.meta` block**:
  ```lua
  asset.meta = {
    Name = "Moon meta asset",
    Description = [[Trail, Model and Label of Moon.]],
    Author = "OpenSpace Team",
    URL = "https://www.openspaceproject.com",
    License = "MIT"
  }
  ```
- **Example assets are documentation.** > "Since this is an example, do we want to explain
  these parameters in a comment? … I'm trying to think what would be more understandable"
  (#3801). Name things descriptively (`ModelMarsLarge`, not `Model1`).
- **Test profiles don't get committed.** > "I would assume that we do not want to have this
  profile in the repository, since it is not really providing new content to the users. We
  could convert it to an image test instead?" — with the maintainer agreeing: "Having the
  profile for the PR is super useful though … But then removing it before merging it" (#3801).
  **Ship a test profile during review, delete it before merge, convert it to an `.ostest`.**

### 4.8 Scope discipline

`Roxeena`'s review of #3801 is the clearest lesson in the corpus: a large PR accumulated
changes from a sibling project branch (`project/moon-shot`), and every one of them drew a
"why is this here?" comment — `renderengine.h`, `scenegraphnode.h`, `gpulayergroup.cpp`,
`layer.cpp`, six `planetarytrail_*.glsl` files. The author's own reply — "Oops, I accidentally
let this one through the merge" — is exactly the outcome to avoid.

**Before opening a PR, read your own full diff file-by-file and delete anything you cannot
justify in one sentence tied to the PR title.**

Also flagged: **breaking changes carry an extra obligation.** > "This is a breaking change that
we need to make sure that none of our assets are using the old version and it needs to be in
the breaking changes list of the internal wiki" (#3715).

---

## Part 5 — Deliverable checklists

### 5.1 What a "new renderable" PR must contain

From `Add RenderableSwitch class (#3597)` — the complete, minimal file set:

```
modules/base/rendering/renderableswitch.h            # class, header guard, forward decls
modules/base/rendering/renderableswitch.cpp          # codegen Parameters + implementation
modules/base/CMakeLists.txt                          # add both files to the source lists
modules/base/basemodule.cpp                          # register in the factory
data/assets/examples/renderable/renderableswitch/switch.asset
data/assets/examples/renderable/renderableswitch/switch_near.asset
data/assets/examples/renderable/renderableswitch/switch_far.asset
```

and, mirroring the example path exactly:

```
visualtests/example/renderable/renderableswitch/switch.ostest
visualtests/example/renderable/renderableswitch/switch_near.ostest
visualtests/example/renderable/renderableswitch/switch_far.ostest
```

There are 248 example assets and 262 `.ostest` files upstream; the `visualtests/example/` tree
**mirrors `data/assets/examples/` exactly, with identical names**. Missing example assets is a
guaranteed review round.

### 5.2 Additional obligations by change type

| Change | Extra requirements |
| --- | --- |
| New renderable/translation/rotation/scale | Example asset(s) + matching `.ostest`; `Documentation()`; bounding sphere so "Fly to"/"Jump to" work |
| New Lua function | `[[codegen::luawrap]]`, documentation comment, appears in generated docs |
| New property | `PropertyInfo` with deliberate `Visibility`; description without min/max; `onChange` then `addProperty` |
| Breaking change | Verify no shipped asset uses the old form; add to the internal wiki's breaking-changes list; state it in the PR |
| New module | `include.cmake`, `<name>module.h/cpp`, registration; must build with all modules ON |
| Behaviour visible on screen | An `.ostest` under `visualtests/` |
| Data/asset update | `asset.meta` with Author/URL/License |
| Third-party dependency | Submodule under `ext/` or `modules/*/ext/`; `THIRD_PARTY_LICENSES.md` |

### 5.3 Pre-flight verification

```bash
# 1. Rebase on current upstream master, and check for accidental cargo
git fetch upstream && git rebase upstream/master
git diff --stat upstream/master          # every file justifiable? any submodule pointers?

# 2. Style sweep on your own diff
#    - 90 columns, 4-space C++ / 2-space asset+GLSL
#    - final newline in every new file, no trailing whitespace
#    - Copyright (c) 2014-2026 in every new file's header

# 3. Build clean, no new warnings (MSVC is the strictest reviewer-facing compiler)
cmake --preset windows-min && cmake --build --preset windows-min

# 4. Run the tests CI runs
./bin/OpenSpaceTest && ./bin/GhoulTest && ./bin/codegentest && ./bin/SGCTTest

# 5. Run the app from a clean profile and exercise the feature end-to-end.
#    Check the log for new warnings/errors and for chatty per-frame INFO messages.

# 6. clang-tidy the touched files against .clang_tidy
```

### 5.4 Image/visual tests

- `.ostest` files are JSON with two keys: `profile` and `commands` (ordered `{type, value}`
  instructions).
- Instruction types: `action`, `asset`, `deltatime`, `navigationstate`, `pause`, `property`,
  `recording`, `screenshot`, `script`, `time`, `wait`.
- **The last instruction must be exactly one `screenshot`.**
- Start by setting an explicit `time` for reproducibility; keep instruction count minimal;
  prefer `navigationstate` over `recording`; prefer dedicated instructions over `script`; add
  `wait` where data loads asynchronously.
- The server runs tests paused, with MRF caching on and UI/dashboard hidden — don't re-specify
  those.
- **Never move or rename an existing test** — third parties depend on the URLs.
- Create tests with the wizard: `support/testwizard`, `pip install -r requirements.txt`, then
  `python main.py` with OpenSpace already running on the target profile. Verify locally with
  the Runner from the `OpenSpace-VisualTesting` repository before committing.

---

## Part 6 — The PR playbook

### 6.1 Sequence

1. **Find or file an issue first.** `CONTRIBUTING.md` asks for it, and `(closes #NNNN)` is the
   dominant commit idiom. It also gets your design questioned before you write the code.
2. **Branch** `issue/<number>-<slug>` (or `feature/<slug>`) off current `upstream/master`.
3. **Keep it under ~10 files** if you possibly can (external-PR p90 = 10 files).
4. **Self-review the whole diff** before opening. Then run §5.3.
5. **Open the PR with a title that works as the final squashed commit message.**
6. **Attach a screenshot or short video** for anything visual — near-universal in this repo,
   and reviewers reciprocate with their own screen recordings.
7. **Respond to every comment.** Even "Okay, good to know, but I'm just going to leave it like
   this for now" is an accepted answer — silence is not.
8. **Expect to be asked to run it yourself**, and expect the reviewer to run it too.

### 6.2 PR title

It becomes the commit subject verbatim, plus `(#NNNN)`. Therefore:

- Imperative, capitalised, no trailing period.
- Describe the corrected behaviour, specifically. Length up to ~90 characters is normal.
- Append `(closes #NNNN)` when an issue exists.

```
✅ Correctly compute ring shadow UVs when the tile tree ascends (closes #4211)
✅ Prevent crash when a RenderableGlobe has no ring system (closes #3404)
❌ Fixed some bugs.
❌ ring shadow fix
❌ Feature/my-branch-name
```

(That last anti-pattern is real, and it is worth taking personally: `master`'s permanent
history contains `Feature/rename parallelpeer to astrocast (#4206)`,
`Feature/3D volume vectorfield (#3897)` — and `Issue/3596 saturn ring shadow (#3634)`, which
is `benpm`'s. GitHub pre-fills the PR title from the branch name; overwrite it every time.)

### 6.3 PR body template

Modelled on the bodies that merged with the least friction (median body length 330 characters;
23/200 were empty — don't be one of those):

```markdown
<One paragraph: what is wrong / missing, and the observable symptom.>

<One paragraph: what this change does, and why this approach. Link the upstream
issue/spec/documentation that justifies it.>

<Screenshot or video for anything visual.>

<Anything deliberately left out of scope, and why.>

closes #NNNN
```

A real, high-quality example (#3784, merged after one approving review):

> The shadows cast by Saturn onto its rings in fisheye projection mode are rendered
> incorrectly because the depth map used for shadow rendering is rendered during the normal
> render pass, so it is rendered 5 times and in different directions. This causes parts of the
> shadow to be missing. (See #3776 for more details)
>
> *[screenshot]*
>
> I've simply replaced shadow-mapping with a ray/ellipsoid intersection test. However, there
> are other use cases for shadow-mapping, like in project/moon-shot, so I've left most of the
> shadow-map rendering logic intact so that the project can be eventually updated and merged.
>
> fixes #3776

Note what it does: states the mechanism of the bug, shows the visual, explains the design
trade-off, and pre-empts "why did you leave that code in?".

### 6.4 Rules of thumb, ranked by return on effort

1. Make the PR small and single-purpose. This dominates everything else.
2. Get the formatting perfect before opening — 90 columns, includes, line breaks, copyright
   year, final newlines. Style comments consume a whole review round and delay substantive
   feedback.
3. Write the user-facing documentation as carefully as the code.
4. Run the branch yourself from a clean profile, with zero new compiler warnings and no new log
   spam.
5. Ship the example asset and the visual test with the feature, not afterwards.
6. Never carry unrelated changes, submodule bumps, or reformatting.
7. Answer every review comment.

---

## Part 7 — Gaps and caveats

- **`internal.openspaceproject.com` is inaccessible** to this analysis (HTTP 403
  unauthenticated; the page does not load in-browser without SSO). Four referenced pages remain
  unread and are likely to matter:
  - `Misc/workflows` — developer etiquette and basics
  - `Private/Gene/how-to-add-an-image-test`
  - `Private/Gene/testing-for-session-recording`
  - `Private/Gene/utah-data-server-config`
  The **breaking-changes list** lives on that wiki (per PR #3715) and is a hard requirement for
  any breaking change. If you can export those pages, they should be folded into §4 and §5.
- **`pds.nasa.gov` is a data source, not a code-review authority.** Ten files in the repo
  reference PDS/NASA Treks (Moon LRO NAC mosaics, MRO CTX/HiRISE, Viking Phobos, MESSENGER
  Mercury). Its relevance to a PR is provenance: data-bearing assets carry an `asset.meta`
  block naming Author/URL/License. There is no PDS-specific review rule.
- Docs live in a **separate repository**, `OpenSpace/OpenSpace-Docs` (Sphinx, `.markdownlint.json`,
  ReadTheDocs). Documentation-only contributions go there, not to the main repo. The main repo's
  `documentation/` directory is a submodule of `OpenSpace-Documentation-Dist`.
- The style guide does not cover GLSL, Lua, or asset files. §4.6/§4.7 reconstruct those
  conventions from review evidence only — treat them as strong priors, not citable rules.
- Label-based triage effectively does not exist (7 label applications across 200 PRs).
- All statistics are point-in-time (2026-08-17) over the most recent 200 merged PRs
  (2025-02-28 onward). Re-run the analysis before relying on the numbers a year from now.
