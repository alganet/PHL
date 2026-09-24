--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: session_set_save_handler()'s individual callbacks deprecate and are accepted (php half)
--DESCRIPTION--
The php half of session_save_handler_callables.phpt: php 8.4 emits
"Providing individual callbacks instead of an object implementing
SessionHandlerInterface is deprecated" and installs the handler anyway. PHL has no
engine deprecation sites and refuses the spelling instead (§10).
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
// Nothing is printed until the end: the deprecation is raised INSIDE the call,
// and echoing it there would send the headers the same call then checks.
$log = [];
set_error_handler(function ($no, $msg) use (&$log) { $log[] = "[$no] $msg"; return true; });
$noop = function () { return true; };
try {
    $log[] = var_export(session_set_save_handler($noop, $noop, $noop, $noop, $noop, $noop), true);
} catch (Throwable $e) {
    $log[] = get_class($e) . ": " . $e->getMessage();
}
$log[] = session_module_name();
restore_error_handler();
echo implode("\n", $log), "\n";
?>
--EXPECT--
[8192] session_set_save_handler(): Providing individual callbacks instead of an object implementing SessionHandlerInterface is deprecated
true
user
--CLEAN--
<?php
unset($noop, $log);
