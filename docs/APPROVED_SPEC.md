# Capicola for disting NT — Discovery Spec

## Destination
Produce a decision-complete, reviewable Spec for wrapping Capicola as a releasable disting NT project, preserving the upstream license, linking vendor code through Git submodules, and providing an NT-appropriate custom interface for Capicola’s supported processing capabilities.

## Problem and intended outcome
Capicola is an upstream real-time time stretcher, pitch shifter, transient detector, and envelope follower that the owner wants made usable on disting NT. The outcome is a releasable disting NT project that uses Capicola’s code and faithfully presents capabilities the upstream implementation actually supports. This is not a redesign or broader sample workstation. Development may start quickly and simply; the owner decides whether observed behavior is acceptable for public release.

## Users and stakeholders
- Primary users: ordinary disting NT owners; the release must not assume software-development expertise.
- Product owner and release maintainer: the current project owner.
- Upstream stakeholders: Capicola’s authors and copyright holders.
- Distribution dependencies: GitHub, NT Gallery, nt_helper, and the normal disting NT installation workflow.
- Support is best-effort, without a guaranteed response time.

## Shared language
- **Wrapper:** NT-specific integration around Capicola, without deliberately redesigning its processing behavior.
- **Upstream:** the original Capicola repository and implementation.
- **Vendor code:** external source incorporated as a dependency; vendor-code linkage uses Git submodules.
- **Supported platform baseline:** the exact disting NT firmware and SDK versions used and verified for a release.
- **Source mode:** one mutually exclusive audio origin: live stereo input or loaded-sample playback.
- **Normalization:** when only the left live input is connected, it feeds both processing channels.
- **Host behavior:** behavior owned by disting NT firmware or SDK, which the wrapper uses rather than replaces.
- **Release judgment:** the owner’s decision that the plugin works acceptably for publication, rather than a mandatory numeric test gate.

## Decisions
### D-001 — Faithful wrapper
The first release is a faithful disting NT wrapper around Capicola, not an expanded sampler or feature-inventing port.

### D-002 — Owner-controlled release
Public release occurs only when the owner judges the plugin to work acceptably. This Spec imposes no independent numeric stability, soak, CPU-headroom, or audio-test gate.

### D-003 — Approved single-screen UI
The owner approved “Capicola NT single-screen performance UI v2.” Normal operation uses one persistent performance screen, main and alternate pot functions, clear live/sample indication, temporary sample selection, envelope/transient feedback, three pressable pots, two pressable encoders, and no separate module buttons. Evidence may correct labels or ranges; material interaction or layout changes need renewed owner review.

### D-004 — Narrow compatibility promise
A release officially supports only the exact disting NT firmware and SDK versions used and verified for it.

### D-005 — Separate source modes
Live stereo input and loaded-sample playback are separate, mutually exclusive modes and are never mixed inside the plugin.

### D-006 — Minimal direct CV routing
Live mode uses stereo routing, with left-only mono normalized to both channels. Other modulation uses ordinary NT parameter mapping. Envelope and transient signals are optional CV outputs; each output selector defaults to 0/unconnected and includes 0 as its minimum.

### D-007 — Host-owned persistence
Sample references and related state use standard disting NT persistence. Preset restoration and missing-sample behavior follow verified host/SDK behavior. The wrapper does not silently substitute a sample or fall back to live input.

### D-008 — Ordinary-user distribution
The release targets ordinary disting NT owners and ships as a version-tagged GitHub release through NT Gallery to nt_helper, using the normal plugin installation workflow. Support is best-effort.

### D-009 — No invented quality gates
No fixed runtime, CPU percentage, audio suite, dropout count, or similar threshold is a publication gate. Mandatory platform and license requirements still apply.

### D-010 — Same-license wrapper release
The wrapper will be released under the exact same GPL-based license as upstream Capicola, clearly identified as a wrapper around that project. It will retain required upstream license text, copyright notices, and attribution and make corresponding source plus pinned submodule information available as required. No separate counsel or rights-holder approval is required unless a concrete license incompatibility is actually identified.

**Reason:** The owner wants compliance with the upstream license itself, not a speculative extra clearance process.

## Scope and boundaries
### In scope
- Capicola integration into a disting NT project.
- Separate live stereo and loaded-sample modes.
- Left-to-right mono normalization.
- Optional default-disconnected envelope/transient CV outputs.
- Ordinary NT parameter-to-CV mapping.
- Standard NT sample selection and persistence.
- Git submodules for vendor code.
- Same-license release with required notices, attribution, corresponding source, and submodule information.
- Approved custom UI, build, packaging, documentation, compatibility declaration, and owner-directed release preparation.
- Tagged GitHub releases distributed through NT Gallery to nt_helper.

### Out of scope
- Capabilities absent from Capicola.
- A broader sample workstation or core-processing redesign.
- Simultaneous live/sample mixing.
- Dedicated processing-control CV inputs in version 1.
- Custom preset-opening or missing-file recovery policy.
- Routine three-screen navigation or separate hardware buttons.
- Implementation before Spec approval.
- Compatibility promises for untested versions.
- Guaranteed support times.
- Mandatory soak, numeric CPU, or compulsory audio-test gates.
- A speculative legal-opinion or rights-holder-clearance gate when no concrete incompatibility has been identified.

## Functional requirements
- FR-001: Integrate Capicola as the processing engine behind a disting NT wrapper.
- FR-002: Expose only audited upstream capabilities feasible on disting NT.
- FR-003: Provide the approved single-screen interface for source selection/loading, performance control, and feedback.
- FR-004: Link vendor code through Git submodules.
- FR-005: Release under the same applicable upstream GPL-based license and preserve all required license text, notices, attribution, corresponding-source availability, and submodule provenance.
- FR-006: Follow verified NT SDK/API, build, packaging, and distribution requirements for the supported baseline.
- FR-007: Normal operation uses one persistent performance screen.
- FR-008: Use alternate pot functions and both pressable encoders as needed, with no separate hardware buttons.
- FR-009: Release metadata and documentation name the supported platform baseline.
- FR-010: Process live stereo input.
- FR-011: Load and process a supported sample.
- FR-012: Source modes are mutually exclusive.
- FR-013: Connected left and right inputs feed corresponding channels.
- FR-014: With only left connected, left feeds both channels.
- FR-015: Non-audio modulation uses NT parameters and host parameter-to-CV mapping.
- FR-016: Confirmed envelope/transient signals are optional CV outputs, subject to upstream audit and NT limits.
- FR-017: Each CV output selector defaults to 0/unconnected and has 0 as its minimum.
- FR-018: Material changes from the approved v2 UI require owner-reviewed reprototyping.
- FR-019: Store and restore sample state only through standard facilities supported by the verified NT SDK.
- FR-020: Invalid or unavailable sample behavior follows the supported NT baseline; do not substitute a sample or switch to live input.
- FR-021: Package public versions as tagged GitHub releases compatible with NT Gallery and nt_helper.
- FR-022: Provide ordinary-user installation, operation, compatibility, known-limit, and troubleshooting documentation.

Exact sample formats and limits, audited labels/ranges, final CV output names/count, and host error presentation must be verified against the selected upstream commit and supported NT baseline during delivery; they are not additional product-owner decisions.

## User experience
- Display: 256×64 pixels, 16 grayscale levels.
- Controls: three pressable pots and two pressable encoders; no separate buttons.
- The approved v2 prototype is the visual and interaction baseline.
- One persistent screen keeps immediate controls, source state, and activity visible.
- Alternate functions have clear visible feedback.
- Sample selection may temporarily replace the performance view.
- Source changes replace rather than blend sources.
- CV outputs appear disconnected by default.
- Preset restoration and sample errors retain standard NT behavior.

## Data, privacy, and safety
Operation is local, with no network service, account, analytics, or personal-data collection. Sample references persist only through supported NT facilities. The wrapper does not silently replace an unavailable sample or unexpectedly switch to live input. CV outputs default to disconnected. Malformed-resource and host recovery behavior follows the supported NT baseline.

## Integrations and dependencies
- Upstream: `https://github.com/heavylight-industries/capicola`.
- disting NT SDK and runtime/build interfaces.
- Vendor dependencies linked through pinned Git submodules.
- Exact firmware, SDK, upstream, and dependency revisions are pinned during delivery.
- NT host parameter-to-CV mapping, sample selection, persistence, and resource handling.
- GitHub tags/releases, NT Gallery, and nt_helper.

## Quality and operational requirements
- Release is controlled by the owner’s judgment that the plugin works acceptably.
- Builds and packaging satisfy the selected NT baseline and distribution route.
- Documentation identifies the baseline, installation, operation, known limits, and troubleshooting.
- Actual upstream-license and mandatory distributor requirements are binding; speculative additional clearance is not.
- Checks may be performed at the owner’s discretion, without a mandatory soak, CPU margin, exhaustive audio suite, or formal pass/fail matrix.
- Support is best-effort.

## Risks and assumptions
- Exact CPU, memory, SDK feasibility, controls, outputs, and source limits require verification during delivery.
- The same-license release assumes the wrapper and distributed dependency combination is compatible with upstream’s actual license terms; any concrete conflict discovered during implementation must be surfaced rather than hidden.
- Some upstream capabilities may need constrained ranges.
- Source changes must avoid stale audio, overlap, or unsafe gain jumps in ordinary use, subject to owner judgment.
- NT output limits may constrain simultaneous analysis outputs.
- NT Gallery or nt_helper rules may change and must be checked for each release.

## Acceptance criteria
- **AC-001:** The first-release feature set contains no user-facing audio-processing or sampler capability that is absent from the audited upstream Capicola implementation.
- **AC-002:** Each user-facing processing capability in the release is traceable to an audited upstream Capicola capability and is documented with any NT-specific limit.
- **AC-003:** The repository links vendor code through Git submodules rather than copied, untracked vendor source.
- **AC-004:** The release includes all license text, copyright notices, attribution, source availability, and other obligations identified by the completed license audit.
- **AC-005:** A user can select/load a supported sample and access the confirmed Capicola processing controls through the approved NT interface.
- **AC-006:** No implementation work begins until this Spec has completed discovery and received the required review approval.
- **AC-007:** The owner has confirmed that the plugin works acceptably for public release; this confirmation does not require a fixed soak duration, numeric CPU threshold, or mandatory audio-test matrix.
- **AC-008:** Release review confirms compliance with mandatory supported-baseline build/package requirements and records any known limitations the owner chooses to publish.
- **AC-009:** After source selection/loading, a user can operate the normal performance workflow from one persistent screen without separate hardware buttons or three routine pages.
- **AC-010:** When a pressable pot has an alternate function, the interface gives visible feedback identifying the active function and value.
- **AC-011:** Release documentation identifies the exact supported disting NT firmware and SDK versions, and no additional version is described as supported without recorded test evidence.
- **AC-012:** On the supported baseline, live stereo input can be selected and processed using documented routing.
- **AC-013:** On the supported baseline, a supported sample can be loaded and processed through Capicola.
- **AC-014:** Switching source modes results in exactly one active source, with no simultaneous live/sample mix.
- **AC-015:** With both live inputs connected, left and right feed corresponding channels without unintended summing or swapping.
- **AC-016:** With only left connected, it feeds both channels; connecting right restores independent stereo.
- **AC-017:** On first load and reset, every optional CV output selector is 0/unconnected and no physical CV output is claimed.
- **AC-018:** A user can assign each supported envelope/transient signal to an available CV output and disconnect it by selecting 0.
- **AC-019:** A supported processing parameter can be modulated through host parameter-to-CV mapping without a plugin-specific direct CV input.
- **AC-020:** The delivered performance interface matches the owner-approved “Capicola NT single-screen performance UI v2” information hierarchy and interaction model, except for evidence-based corrections to labels or ranges; any material layout or interaction change has a separately approved prototype.
- **AC-021:** Saving and restoring a preset with a valid sample reference behaves according to the documented disting NT host behavior on the supported platform baseline.
- **AC-022:** When a saved sample reference is missing, moved, unsupported, or unreadable, observed behavior matches the documented disting NT host behavior; Capicola neither substitutes another sample nor automatically falls back to live input.
- **AC-023:** A typical disting NT owner can install a version-tagged release through the documented GitHub, NT Gallery, and nt_helper plugin flow without compiling the project.
- **AC-024:** The public release includes documentation for installation, normal operation, the supported firmware/SDK baseline, known limits, troubleshooting, and reproducible issue reporting.
- **AC-025:** Public release uses the same applicable GPL-based license as upstream Capicola, identifies the project as a wrapper, retains required upstream notices and attribution, and provides the corresponding source and pinned submodule information required by that license.

## Open questions
None requiring a product-owner decision. Exact technical facts listed above are delivery-time verification items against pinned source and platform revisions.