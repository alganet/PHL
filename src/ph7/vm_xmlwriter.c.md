# src/ph7/vm_xmlwriter.c

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
|    - |    7 | `#include <libxml/xmlwriter.h>` |
|    - |    8 |  |
|    - |    9 | `/*` |
|    - |   10 | ` * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class` |
|    - |   11 | ` * prelude.  Architecture notes live in vm_libxml.c (shared registries,` |
|    - |   12 | ` * error queue, lifetime model).` |
|    - |   13 | ` */` |
|    - |   14 |  |
|    - |   15 | `/*` |
|    - |   16 | ` * Install the XMLWriter library.  Called from PH7_VmInit inside the` |
|    - |   17 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|    - |   18 | ` */` |
| 3884 |   19 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)` |
|    5 |   20 | `{` |
| 1942 |   21 | `	SXUNUSED(pVm);` |
| 3889 |   22 | `	return SXRET_OK;` |
|    5 |   23 | `}` |
|    - |   24 |  |
|    - |   25 | `#else` |
|    - |   26 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|    - |   27 | `typedef int vm_xmlwriter_unused;` |
|    - |   28 | `#endif /* PH7_ENABLE_LIBXML */` |
|    - |   29 |  |
