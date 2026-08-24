# Capicola for disting NT

A planned disting NT wrapper around [Capicola](https://github.com/heavylight-industries/capicola).

## Delivery status

Discovery and required review approval are complete. Implementation is governed by the immutable [approved Spec](docs/APPROVED_SPEC.md) and its [implementation-gate record](docs/IMPLEMENTATION_GATE.md). Work must not expand beyond that first-release boundary without a reviewed Spec revision.

The pinned upstream/platform revisions and traceable first-release processing surface are recorded in the [capability audit](docs/CAPABILITY_AUDIT.md). No plugin implementation is present yet.

## Audit verification

```sh
git submodule update --init --recursive
make verify
```
