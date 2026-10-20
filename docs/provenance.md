# Provenance

The port uses the existing RTL8188E source and preserves its history and
per-file license notices. Stock analysis target: Tilk 9082XS module
v1.2.0_0046.20160328, kernel3.14.29, SHA256:
`b285a592d7cacad018ca4bd3451da967f225f2db5eb109c55957289e38d050cc`.

Recovered firmware: .data+0x49b0,15382 bytes, SHA256
`f26c7e38bd498dee77fb302983a7df7d743e68ffc867624a18237eefc21d409c`.
No redistribution permission has been established. Neither the stock module
nor its firmware bytes are published. The local extractor validates both
digests and generates an ignored header. Obtain the stock artifact from your
own device or an authorized source; other layouts are rejected.

`s9082_data.h` retains recovered register initialization/power records and
contract evidence identifiers. These are hardware configuration projections,
not original vendor source. Retained full analysis dossiers and raw runtime
logs remain in the investigator's private workspace; this repository publishes
curated conclusions without credentials, network identities or key dumps.

Extraction dependency: pyelftools (install into a Python virtual environment).
Generated firmware is local-only and must not be added to Git or release ZIPs.
