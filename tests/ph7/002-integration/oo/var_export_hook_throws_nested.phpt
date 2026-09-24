--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a hook throwing inside a NESTED object prints nothing at all (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* var_export() builds its text and hands it to the output layer only once it is
 * whole, so a throw from a property hook anywhere inside it prints NOTHING --
 * including from an object nested in another object, which is the one place php
 * lets the partial buffer through (ending mid-entry, and not re-readable php).
 * The two direct shapes agree: see var_export_hook_throws.phpt. */
class VehnInner { public int $v { get => throw new LogicException('deep'); } }
class VehnOuter {
    public $a = 1;
    public VehnInner $i;
    public function __construct() { $this->i = new VehnInner; }
}
try { var_export(new VehnOuter); } catch (Throwable $e) { echo '|caught ', $e->getMessage(), "\n"; }
echo "AFTER\n";
?>
--EXPECT--
|caught deep
AFTER
--CLEAN--
<?php
