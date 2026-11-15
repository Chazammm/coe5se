# Sandbox source contribution review

Prepared against actual official dev `df09497f04a39ff8fb7b0979dedde7dfed120c64` (3.4.4), rechecked before publication. Source review branches only, on the user's fork. Nothing was pushed upstream, merged or submitted as a PR. No Extender binary accompanies the mod.

The fork's existing `dev` and `main` are deliberately untouched. For an independent review diff, compare the branch with the exact official base SHA above, rather than the fork's older `dev`. The eventual upstream PR base is official `dev`; dependent branches use the explicitly listed stack base while reviewing.

| Order | Review branch | Intended review base | Code head |
|---|---|---|---|
| 1. ranged | [review/sandbox-ranged-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-ranged-20261115) | `dev` | `245307837306510e51bf56427add9c1ab003dc89` |
| 2. origin | [review/sandbox-content-origin-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-content-origin-20261115) | `dev` | `3adfd71d72a1477329482d01a0136c7504a4d5ad` |
| 3. core | [review/sandbox-field-core-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-field-core-20261115) | `dev` | `b67156dd852435ac0d3a4144a116519f1596c2bd` |
| 4. presentation | [review/sandbox-field-presentation-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-field-presentation-20261115) | `review/sandbox-field-core-20261115` | `c2c9c3a6ab2ac72d560f0bcc938b3cbf247e0dfe` |
| 5. configuration | [review/sandbox-field-configuration-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-field-configuration-20261115) | `review/sandbox-field-presentation-20261115` | `fcc206fc2293af25d1878166392d476ef9aa9669` |

Complete combination: [review/sandbox-complete-20261115](https://github.com/Chazammm/coe5se/tree/review/sandbox-complete-20261115). The documents in this folder are integration review material, not extra changes mixed into every individual PR. Drafts follow the current upstream template. The stacked bases are explicit; after dependencies merge, actual upstream submissions must target dev.

- [Capability matrix and dependency graph](CAPABILITY-MATRIX.md)
- [Complete imported function inventory](API-INVENTORY.csv)
- [Build, native and visible test results](TEST-RESULTS.md)
- Drafts: [Ranged](PR-RANGED.md), [provenance](PR-ORIGIN.md), [field core](PR-CORE.md), [presentation](PR-PRESENTATION.md), [configuration](PR-CONFIGURATION.md)

## Outstanding personal sign-off decision

Upstream requires a Signed-off-by line on every commit. This is acceptance of contribution terms, not a cryptographic signature. The user has not explicitly accepted those terms, so no sign-off was invented from configured Git identity. Branches are unsigned review material, not compliant PR submissions.

Signing off confirms authorship/right to contribute, grants musical_pianist and successors a perpetual, worldwide, non-exclusive, royalty-free, irrevocable license to use/copy/change/sublicense/distribute the contribution under CoE5SE's license or other terms, and acknowledges public permanent contribution/sign-off history. The contributor retains copyright. Read the actual [pinned CONTRIBUTING.md](../../CONTRIBUTING.md) before deciding. This task did not rewrite published history or force-push anything. If signing later, use new signed review refs rather than overwriting others' work without authorization.

All current project/GitHub communication is maintained in English. Historical Sandbox reports have English consolidations with original source-era reports preserved in Git history. Normal game/profile controls and all original dirty experimental work are preserved; see the test report.
