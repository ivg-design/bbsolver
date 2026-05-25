# AE ScriptUI Test Harness

This folder contains the standalone After Effects reference harness for
`bbsolver`.

## Files

- `bbsolver-test-harness.jsx` — the ScriptUI panel entry point.
- `bbsolver-test-harness/` — required support folder included by the panel.

Install the root script and support folder side by side:

```text
ScriptUI Panels/
  bbsolver-test-harness.jsx
  bbsolver-test-harness/
```

macOS After Effects 2026 app-level location:

```text
/Applications/Adobe After Effects 2026/Scripts/ScriptUI Panels/
```

Windows After Effects app-level location:

```text
%ProgramFiles%\Adobe\Adobe After Effects 2026\Support Files\Scripts\ScriptUI Panels\
```

See [`../../docs/AE_SCRIPTUI_HARNESS.md`](../../docs/AE_SCRIPTUI_HARNESS.md)
for install commands, solver binary path resolution, supported controls, and
feature notes.
