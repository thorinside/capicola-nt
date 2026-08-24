# Analysis CV routing

Capicola exposes the four analysis signals confirmed by the pinned capability audit as optional NT CV outputs:

| Selector | Signal | Voltage |
| --- | --- | --- |
| `Input Transient output` | Kept transient from either input-channel detector | 0 V low, 5 V high |
| `Output Transient output` | Kept transient from the post-mix output detector | 0 V low, 5 V high |
| `Input Envelope output` | Maximum normalized envelope of the two input-channel detectors | 0–5 V |
| `Output Envelope output` | Normalized post-mix output envelope | 0–5 V |

All four selectors are ordinary NT CV-output parameters on the **Routing** page. Their minimum, first-load default, and host-reset default are `0` (`None`). At `0`, Capicola does not read, write, or claim a bus. Select any available NT bus to assign a signal, and select `0` again to disconnect it.

An assigned analysis signal replaces the selected bus with its voltage. Assigning multiple outputs, including Capicola's audio outputs, to the same bus is therefore not a mixing facility and should be avoided. These selectors do not add processing-control CV inputs; ordinary parameter modulation remains host-owned as described in [HOST_MODULATION.md](HOST_MODULATION.md).

The envelope normalization and detector gates mirror the audited upstream analysis path. Input analysis combines the two linked Capicola channel detectors; output analysis observes the final post-mix stereo sum. Source mode does not change the routing contract: the signals analyze whichever single source is active.
