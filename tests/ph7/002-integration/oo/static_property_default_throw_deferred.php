<?php
/*
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Fixture for static_property_default_throw_deferred.phpt: a class whose static
 * default cannot be evaluated. Including it must be silent in both engines --
 * php materializes the static table at the first ACCESS, never at declaration.
 */
class SdtIncluded { public static $s = SDT_UNDEF_INC; }
echo "included\n";
