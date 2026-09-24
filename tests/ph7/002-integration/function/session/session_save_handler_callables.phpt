--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: session_set_save_handler()'s individual callbacks are refused (PHL half)
--DESCRIPTION--
php 8.4 DEPRECATES the six-to-nine callables form —
"Providing individual callbacks instead of an object implementing
SessionHandlerInterface is deprecated" — and PHL targets php's NON-deprecated
surface (§10), so the only handler it takes is the object. The refusal is php's
own message for a first argument that is not one, which is what each of those
callables is. php's half is the `_zend` twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
}
?>
--FILE--
<?php
$noop = function () { return true; };
try {
    var_dump(session_set_save_handler($noop, $noop, $noop, $noop, $noop, $noop));
} catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
echo session_module_name(), "\n";
?>
--EXPECT--
TypeError: session_set_save_handler(): Argument #1 ($open) must be of type SessionHandlerInterface, Closure given
files
--CLEAN--
<?php
unset($noop);
