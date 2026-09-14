# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5/5 lines (100.00%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include <libxml/parser.h>` |
|    - |    8 | `#include <libxml/tree.h>` |
|    - |    9 |  |
|    - |   10 | `/*` |
|    - |   11 | ` * ext/dom on libxml2: __dom_* native thunks + the DOM class prelude.` |
|    - |   12 | ` * Architecture notes live in vm_libxml.c (shared registries, error queue,` |
|    - |   13 | ` * lifetime model).` |
|    - |   14 | ` */` |
|    - |   15 |  |
|    - |   16 | `/*` |
|    - |   17 | ` * Install the DOM library.  Called from PH7_VmInit inside the` |
|    - |   18 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|    - |   19 | ` */` |
| 3884 |   20 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|    5 |   21 | `{` |
| 1942 |   22 | `	SXUNUSED(pVm);` |
| 3889 |   23 | `	return SXRET_OK;` |
|    5 |   24 | `}` |
|    - |   25 |  |
|    - |   26 | `#else` |
|    - |   27 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|    - |   28 | `typedef int vm_dom_unused;` |
|    - |   29 | `#endif /* PH7_ENABLE_LIBXML */` |
|    - |   30 |  |
