# Contributing

This document outlines the process for making contributions to the LCEVC decoder repository. The objective is to provide a clear guideline that increases development efficiency.

---

### Before Contributing

Before we can incorporate your contributions into the LCEVC decoder codebase, you must sign the LCEVC Contributor License Agreement (CLA). This agreement ensures that while you retain ownership of your contributions, you grant us the necessary rights to use, modify, and distribute them as part of the project.

The CLA protects both you and V-Nova by clarifying the following:
- You retain ownership of the copyright for your contributions.
- You grant us the rights necessary to use, distribute, and modify your code.
- You will notify us if you are aware of any third-party patents that your code might infringe.

Although you are not required to sign the CLA immediately, we cannot merge your contribution into the project until it has been signed. If you are planning a larger contribution, it is a good idea to contact us first to discuss your ideas. This helps us guide you and ensures a smoother contribution process.

Before submitting a PR please reach out to legal@v-nova.com to sign the [CLA](https://gist.github.com/v-nova-ci/f66ee698bdfda7b1ec9649cc806826a4).

---

### Requesting Features

To request a new feature, use the GitHub issue tracker and select the "Feature Request" issue type in https://github.com/v-novaltd/LCEVCdec/issues/new?template=feature_request.yml. Please write your feature requests as a **User Story** and include the following information:

- A clear description of the motivation: why the feature is required and who the user/audience is.
- A clear description of the feature: what it needs to do.
- Information on how the feature can be tested, including any relevant test data.
- Any external dependencies.

We will make every effort to schedule time to develop feature requests as part of the decoder roadmap, but please note that this process may take time.

---

### Reporting Defects

To report a defect, use this project’s GitHub issue tracker and select the "Defect Report" issue type in https://github.com/v-novaltd/LCEVCdec/issues/new?template=bug_report.yml. You will be able to track the progress of the defect resolution via the GitHub issue tracker.

Regardless of the nature of your defect report (whether it's a comment or a failure of a complex scenario), please follow these guidelines:

- **Summary:** Provide a high-level description of the defect. For example:
 - *"Playing an MP4 crashes the Android Playground App"*

- **Description:** Include clear and concise details of the defect, along with simple steps to reproduce it. For example:
 - *When selecting an MP4 input file in the attached Android App and hitting play, the application crashes.*
 - *Observed on build date: <10/05/2019, Git Hash: abc123xyz>. Version information is important!*
 - *Android version xxx / API Build.*
 - *Issue only occurs on a Samsung Galaxy S9, not on five other test devices.*
 - *Several different MP4 files were tried, all causing the issue.*

- **Supporting Files:** Provide any relevant debug logs, test content, simple/minimal test apps or configuration files to help reproduce the issue.

The more detailed your bug report, the faster it can be resolved.

---

### Code Contribution

This section outlines the high-level process for contributing features or defect resolutions to the LCEVC decoder:

- Source code and documentation changes should be submitted as **Pull Requests** against V-Nova repositories and will be reviewed according to V-Nova’s [coding standards](https://github.com/v-novaltd/coding-standard).
- Ensure your change is associated with a GitHub Issue, and state in the Issue if you intend to or have already developed this feature or defect fix.
- **Engage with V-Nova engineers** about any proposed solutions to defects or feature requests before starting development work. This helps ensure that your contributions align with the project’s goals.

---

### Branching Guidelines

- All work must be performed on a **separate branch** based on **main**, using a public fork.
- Name your branch clearly for easy identification. The format should be `{GitHub Issue number}/{feature_name}` For example:
 - `123/test_improvements`. Use snake case for the feature description with no more than 5 words.

- Ideally, the branch should contain only changes related to the corresponding GitHub Issue. If you need to make additional changes unrelated to the issue, raise a new GitHub Issue and Pull Request.
- Keep branches up to date before raising a Pull Request against main.

---

### Preparing and Submitting Code Changes

When you’ve completed your development, submit your changes via a **Pull Request** following these steps:

- Ensure code hygiene by following "best practices" as outlined in the [coding standard](https://github.com/v-novaltd/coding-standard)
 - Style guide
 - Best Practice
 - Glossary
 - Clang tidy scripts
It is your responsibility to ensure that all changes are correctly tested.
 - Merge Requests will be held up if there is no test component associated with it, please implement appropriate tests for functional changes made.
 - Test suites are present in the repository (i.e *src/func\_tests*, etc...), it is expected that tests will be modified appropriately to cater for the changes made.
 - The full test suite needs to be executed with no failures.

- Please squash as many commits together as makes sense, avoid loading the history with single line commits whilst retaining the core logical progression of changes so that it can still be understood as to how you arrived at a particular solution to aid future maintenance.
- [Raise a Pull Request](https://github.com/v-novaltd/LCEVCdec/pulls) with a **clear & concise** description of the changes and why you arrived at this solution.
- A **Pull Request** submission will notify V-Nova engineers to start the process of reviewing and verifying the proposed source code changes.

The github CI action will run automatically on a PR to validate your changes against the project's tests, once it is passing V-Nova engineers will perform a code review and may run further internal tests for large changes. If you are challenged on your implementation, do not take offence if there's a lot of activity here, it probably means you're touching something critical.
 - You are responsible for understanding and resolving any feedback issues provided, if in doubt ask questions.

- Once all discussion points are completed, the PR can be merged, the preference is to use the GitHub interface to perform this.
- Once all discussion points are completed, V-Nova engineers will merge your PR.

---

### Commit Messages
Commit messages are an important tool to communicate what you have changed and why you performed the change. They also provide a deeper level of context for the changes that may help someone in the future.
Each commit message should have a short "one-line" sentence explanation at the beginning. If the commit affects only one component, the component name should be prepended and separated from the rest of the sentence with a colon, where multiple components are touched then it may be omitted. Below is an example:
`component_name: short one-line summary`

A clear & concise longer description of what changed, including why the change was made, this may be separated over several paragraphs.
Bullet points, indented with one space before and one space after the bullet, indented to have the text aligned.

- Bullet points are separated from surrounding text with blank lines.
- It is good to use *emphasise*, \_underline\_ or `code` as in Markdown, if necessary, but they shouldn't be overused.
- Multi-line code examples are indented with four spaces, as in Markdown syntax
- Finally list tags with one tag per-line, separated from the commit message body with a blank line before.

Here’s an example of a longer commit message:

```markdown
parallel_decode: Fix conformance issues in tiling mode, memory leak and optimise tests
- Fixes residual mismatch in clear blocks at the end of a tile
- Fix memory leak related to tiling mode
- Add slimmed down parallel-decode func_tests - removed a lot from the serial test set that are related to the upscaler which don't apply to bugs that could come from the parallel-decode loop
```

- There is an exception to the above rule, common sense should be applied, typically single line of code changes that are nominal compilation fixes can contain just a brief description, as these commits should eventually be squashed.
