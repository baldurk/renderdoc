# Contributing to RenderDoc

This document is split up and organised into several sections to aid reading and linking. For small changes like one-line fixes or minor tweaks then you can just read the [quick start section](#quick-start) below.

Don't worry about reading all of these documents end-to-end and getting everything perfect the first time. The point of this information isn't to be restrictive about rules and reject contributions, but to give people guidance and help about how to contribute. I'm happy to help out with any changes needed to get your PR ready to merge, until you get the hang of things. If you're unfamiliar with git and need help making any changes, feel free to ask as well!

If you're a regular contributor or if you have a larger amount of code to change, please do read through these as it will make life easier for everyone if you to follow along with these guidelines from the start.

## Code of Conduct

I want to ensure that anyone can contribute to RenderDoc with only the next bug to worry about. For that reason the project has adopted the [contributor covenent](CODE_OF_CONDUCT.md) as a code of conduct to be enforced for anyone taking part in RenderDoc development. This includes any comments on issues or any public discussion e.g. in the #renderdoc IRC channel or discord server.

If you have any queries or concerns in this regard you can get in touch with me [directly over email](mailto:baldurk@baldurk.org).

## Copyright / Contributor License Agreement

Any code you submit will become part of the repository and be distributed under the [RenderDoc license](../LICENSE.md). By submitting code to the project you agree that the code is your own work and that you have the ability to give it to the project.

You also agree by submitting your code that you grant all transferrable rights to the code to the project maintainer, including for example re-licensing the code, modifying the code, distributing in source or binary forms. Specifically this includes a requirement that you assign copyright to the project maintainer (Baldur Karlsson). For this reason, do not modify any copyright statements in files in any PRs.

## Contributing information

1. [Dependencies](CONTRIBUTING/Dependencies.md)
2. [Compiling](CONTRIBUTING/Compiling.md)
3. [Preparing commits](CONTRIBUTING/Preparing-Commits.md)
4. [Developing a change](CONTRIBUTING/Developing-Change.md)
5. [Testing](CONTRIBUTING/Testing.md)
6. [Code Explanation](CONTRIBUTING/Code-Explanation.md)

## Quick Start

The two things you'll need to bear in mind for a small change are the [commit message](CONTRIBUTING/Preparing-Commits.md#commit-messages) and [code formatting](CONTRIBUTING/Preparing-Commits.md#code-formatting).

Commit messages should have a first line with a **maximum of 72 characters**, then a gap, then if you need it a longer explanation in any format you want. The reason for this is that limiting the first line to 72 characters means that `git log` and github's history always displays the full message without it being truncated.

For more information, check the section about [commit messages](CONTRIBUTING/Preparing-Commits.md#commit-messages).

Code should be formatted using **clang-format 3.8**. The reason we fix a specific version of clang-format is that unfortunately different versions can format code in different ways using the same config file, so this would cause problems with automatic verification of code formatting.

For more information, check the section about [code formatting](CONTRIBUTING/Preparing-Commits.md#code-formatting).

