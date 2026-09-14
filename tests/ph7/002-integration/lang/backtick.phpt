--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The backtick (`) operator is a hard parse error (php deprecates; PHL removes)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php deprecates the backtick operator (shell_exec() is the replacement) but still
// runs it; PHL targets php's non-deprecated surface, so it is a hard parse error.
echo `echo test`;
?>
--EXPECTF--
%AParse error:%Athe backtick (`) operator was removed, use shell_exec() instead%A
