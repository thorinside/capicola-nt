# Implementation gate

## Status

**Approved — implementation may proceed only within the approved Spec.**

The project owner and administrator completed discovery and approved the controlling first-release Spec before implementation began. The current approved revision is stored at [`APPROVED_SPEC.md`](APPROVED_SPEC.md). Its distribution wording was later narrowed at the owner's direction without changing the implemented product scope.

- Approved Spec SHA-256: `39dd5357c55bd5baee57b13d5c2105bba85cc1bcd7130eb79b750a33338e6253`
- Portal project: `ced2f617-f7eb-4820-ae21-f46d1f6d373c`
- Approval evidence: the administrator-approved Spec revision supplied for the owner-authorized delivery sequence
- Pre-implementation baseline: commit `c8a96b7` (`Initialize project`), containing only the project README

## Boundary

The approved Spec is the source of truth for the first release. Delivery work must remain inside its scope, decisions, functional requirements, and acceptance criteria. Technical facts explicitly identified there as delivery-time verification items may be resolved against pinned upstream and supported-platform revisions; they do not authorize new product scope.

A material interaction or layout change to the approved “Capicola NT single-screen performance UI v2” requires renewed owner review. Public release remains owner-controlled. No implementation ticket may silently expand Capicola into a broader sampler, mix live and sample sources, invent processing capabilities, or replace mandatory license and supported-baseline checks.

If a proposed change conflicts with or exceeds the approved Spec, stop implementation and obtain a reviewed Spec revision before proceeding.

## Integrity check

From the repository root:

```sh
shasum -a 256 docs/APPROVED_SPEC.md
```

The result must match the approved SHA-256 above so the current owner-approved revision remains reproducible exactly.
