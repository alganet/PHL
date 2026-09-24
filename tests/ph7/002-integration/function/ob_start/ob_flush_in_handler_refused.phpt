--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_flush() from inside an output handler is refused (PHL member: this engine's "Error" label and no trace -- the standing §6 error-format class; the REFUSAL is php's)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
--FILE--
<?php
// Every ob call that would MUTATE the stack is refused from inside a handler --
// the handler IS an operation on that stack. Only ob_start() used to be caught
// here, and ob_flush() from a handler recursed until the native nesting cap.
ob_start(function ($obm_b, $obm_p) { ob_flush(); return "<$obm_b>"; });
echo "x";
ob_end_flush();
echo "NOT REACHED\n";
?>
--EXPECTF--
PHP Error:  ob_flush(): Cannot use output buffering in output buffering display handlers in %s on line 5
