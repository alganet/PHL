--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_constants() survives a constant whose expansion define()s: reading the table must not corrupt the walk over it
--FILE--
<?php
// Reporting a constant's VALUE means EXPANDING it, and a `const` whose
// initializer is a bytecode program runs user code — which may define() and grow
// the constant table while the hash walk is holding a fixed entry count. The
// walk then ran off the end of its bucket chain (a segfault). The table is
// snapshotted before anything is expanded now.
//
// WHEN the initializer runs differs between the engines (php evaluates it at the
// statement, PHL at first read), so this asserts only that the call
// survives and answers a sane table, not which names are in it at which point.
class GdcrDefiner
{
    public function __construct()
    {
        for ($gdcr_i = 0; $gdcr_i < 40; $gdcr_i++) {
            define('Gdcr_from_ctor_' . $gdcr_i, $gdcr_i);
        }
    }
}
const GDCR_OBJ = new GdcrDefiner();
$gdcr = get_defined_constants();
var_dump(is_array($gdcr), count($gdcr) > 100, isset($gdcr['GDCR_OBJ']));
$gdcr = get_defined_constants(true);
var_dump(is_array($gdcr), isset($gdcr['user']));
for ($gdcr_n = 0; $gdcr_n < 20; $gdcr_n++) {
    $gdcr = get_defined_constants();
}
echo "survived\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
survived
--CLEAN--
<?php
unset($gdcr, $gdcr_n, $gdcr_i);
