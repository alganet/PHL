--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error with E_USER_ERROR: 8.4 deprecation, user fatal, halt (php parity)
--SKIPIF--
<?php if (function_exists('zend_version') echo 'skip: needs error message adjustment'; ?>
--FILE--
<?php
echo "before_error\n";
trigger_error("Fatal error", E_USER_ERROR);
echo "after_error\n";
?>
--EXPECTF--
before_error
%ADeprecated: Passing E_USER_ERROR to trigger_error() is deprecated since 8.4, throw an exception or call exit with a string message instead in %s on line %d%AFatal error: Fatal error in %s on line %d%A
--CLEAN--
<?php
