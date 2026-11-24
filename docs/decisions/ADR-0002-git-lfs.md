---
adr: ADR-0002
title: Version control is Git with Git LFS and file locking
status: accepted
date: 2026-11-24
relates_to: [ASSET-001, ASSET-006, Q-01]
---

## Context
Unreal saves maps and assets as binary files (`.uasset`, `.umap`) that cannot be merged, and Blender files are the same.
The repository has been plain Git on GitHub since the start, with `.gitattributes` already sending binaries through LFS.
The Unreal project is about to begin (PH0), so the tool for those files has to be settled (Q-01).

## Options considered
| Option | Pros | Cons |
|---|---|---|
| Git + Git LFS, binaries lockable | Already in use (GitHub, pull requests, the work-branch rule); Git LFS 3.8 is installed; `git lfs lock` stops two people editing one binary | LFS storage and bandwidth on GitHub have quotas; locking must be remembered |
| Perforce | The usual tool in large Unreal studios; locking is built in | A server to run, a second tool to learn, the history and pull-request flow would move |

## Decision
Git with Git LFS. Unreal and Blender binaries are stored in LFS and marked `lockable` (`.gitattributes`); lock a binary
with `git lfs lock <file>` before editing it, as CLAUDE.md asks, and unlock it after the pull request merges. The owner
chose this in chat on 2026-11-24 ("Yes, Git LFS").

## Consequences
- Nothing changes in how work lands: work branch, pull request, merge into `main`.
- Watch the GitHub LFS quota as art grows; revisit (Perforce, or another LFS host) when a second artist joins, as the
  default in Q-01 said.
- Code-first assets (the city built from `World/` at load time) keep the number of binaries small.
