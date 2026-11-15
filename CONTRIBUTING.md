# Contributing to Chenyi Office（胡辰逸定制版）

Thanks for your interest. The project is in its foundation stage, so the development workflow is still being defined. This document describes how to participate now and what the workflow will look like once source integration lands.

The first supported platform is Windows 10/11 x64; changes targeting other platforms are out of scope until version 1.0.

## Ways to contribute

- **Report a bug** — use the [bug report form](https://github.com/sitong-zhang/office-software/issues/new?template=bug_report.yml). Include the version, operating system, and exact steps to reproduce.
- **Request a feature** — use the [feature request form](https://github.com/sitong-zhang/office-software/issues/new?template=feature_request.yml). Explain the problem before proposing the solution.
- **Ask a question or share an idea** — start a thread in [Discussions](https://github.com/sitong-zhang/office-software/discussions).
- **Improve documentation** — open a pull request with the change and a short rationale.
- **Contribute code** — see the workflow below. Build instructions will be published with the first source integration.

## Development workflow

1. Fork the repository and create a topic branch from `main`:

   ```bash
   git checkout -b feature/short-description
   ```

2. Keep changes focused. One pull request should address one problem.
3. Follow the coding style of the surrounding code. Third-party source keeps its original style and license headers.
4. Add or update documentation when behavior changes.
5. Open a pull request and describe what changed, why, and how it was tested.

## Commit messages

Use the conventional prefix style:

```text
feat: add first-run setup screen
fix: correct PDF export page range
docs: clarify release checklist
build: pin toolchain version
```

Keep the subject under 72 characters. Explain the reason in the body when it is not obvious.

## Licensing of contributions

This project is licensed under the Mozilla Public License 2.0. By submitting a pull request you confirm that:

- you have the right to contribute the code;
- new files are licensed under MPL 2.0 and carry the appropriate header;
- you did not copy code from a source that is incompatible with MPL 2.0.

There is no separate contributor license agreement. Do not remove or alter third-party copyright notices or license headers.

## Branding rules

Use Chenyi Office naming consistently across user-facing surfaces. See [docs/branding.md](docs/branding.md) for the rules that apply to product naming, icons, splash screens, and documentation.

## Code of conduct

All participation is covered by the [Code of Conduct](CODE_OF_CONDUCT.md). Report unacceptable behavior according to the process described there.
