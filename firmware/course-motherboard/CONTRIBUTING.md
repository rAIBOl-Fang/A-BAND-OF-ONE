# Contributing to EasyInput Maker

Thank you for helping improve EasyInput Maker. Issues, documentation improvements, tests, bug fixes, hardware-safe refactors, and non-commercial learning features are welcome.

For a beginner-friendly Chinese walkthrough covering ideas, bug reports, forks, branches, commits, pushes, and pull requests, read [`docs/contributing/how-to-contribute.md`](docs/contributing/how-to-contribute.md).

EasyInput Maker is a WaytoAGI community project. The original author is CY-CHENYUE, and the project-owned material is offered by 深圳物启万相人工智能有限公司 under the terms in `LICENSE`.

## Public contribution boundary

Everything submitted to this repository is public and remains in Git history. Do not submit:

- passwords, tokens, network credentials, device identifiers, personal information, private logs, or confidential material;
- code or assets copied from a private, commercial, or incompatible source;
- audio, images, fonts, models, or other media without documented redistribution rights;
- raw AI output that you have not read, understood, and tested.

For a suspected security vulnerability, follow `SECURITY.md` instead of opening a public issue.

## Contribution license

Unless a file or directory clearly states another license, project-owned material is provided under the PolyForm Noncommercial License 1.0.0 in `LICENSE`.

By submitting a contribution, you confirm that:

1. you created the contribution or otherwise have the right to submit it;
2. the contribution may be distributed under the license already applicable to the affected file or directory;
3. a contribution to project-owned material may be distributed under PolyForm Noncommercial 1.0.0;
4. you have identified all third-party material and preserved its notices;
5. the contribution contains no confidential information or undisclosed restrictions.

Submitting a contribution does not transfer your copyright and does not grant an additional commercial license. If a contribution modifies Apache-2.0, MIT, or other third-party material, that material remains governed by its existing license.

For clarity, a contribution to an Apache-2.0, MIT, or other independently licensed file receives the rights already granted by that existing license. “No additional commercial license” means that submitting a pull request does not grant rights beyond the license already applicable to the affected material.

Requests for commercial use may be submitted through the WaytoAGI community or the original author, CY-CHENYUE. Any authorization is effective only when confirmed in writing by the relevant copyright holder or its authorized representative.

## AI-assisted contributions

AI-assisted changes are allowed. The person submitting the pull request remains responsible for:

- understanding every submitted change;
- checking that the output was not copied from an incompatible source;
- verifying hardware safety and failure behavior;
- running the stated tests;
- disclosing meaningful AI assistance in the pull request.

“The AI generated it” is not evidence that a change is correct, safe, or licensed for redistribution.

## Before opening an issue

- Search existing issues first.
- Use a descriptive title and provide reproducible steps.
- State the board identity, firmware revision, build environment, and what you actually observed.
- Remove credentials, device identifiers, personal paths, and unrelated logs.
- Keep feature requests focused on the user need rather than prescribing an unreviewed protocol or security design.

## Pull request requirements

1. Keep one pull request focused on one problem.
2. Link the related issue when one exists.
3. Explain the user-visible effect and hardware resources involved.
4. Add or update automated tests where practical.
5. State exactly which checks were run and which were not run.
6. Distinguish static analysis, build success, device logs, and observed physical behavior.
7. Update public documentation when behavior, protocol, pin use, recovery, or configuration changes.
8. Complete every applicable item in the pull request template.

Changes involving BOOT, GPIO0, GPIO8, USB GPIO19/20, power sequencing, persistent configuration, networking, authentication, microphone, speaker, or third-party licensing require maintainer review.

## Review and merge

- A pull request is not accepted merely because it builds.
- Maintainers may request smaller changes, tests, provenance evidence, or real-device verification.
- The project may decline a contribution that is unsafe, insufficiently licensed, outside the non-commercial project scope, or too difficult to maintain.
- Once the public repository is available, changes to `main` will go through pull requests and required review checks.

Be respectful, discuss technical evidence rather than people, and assume that issue and pull request content will remain public.
