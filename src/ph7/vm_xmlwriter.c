/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/xmlwriter.h>

/*
 * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class
 * prelude.  Architecture notes live in vm_libxml.c (shared registries,
 * error queue, lifetime model).
 */

/*
 * Install the XMLWriter library.  Called from PH7_VmInit inside the
 * bCompilingBuiltin window, after PH7_VmInstallLibxml.
 */
PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)
{
	SXUNUSED(pVm);
	return SXRET_OK;
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_xmlwriter_unused;
#endif /* PH7_ENABLE_LIBXML */
