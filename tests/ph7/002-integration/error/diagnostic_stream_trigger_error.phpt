--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Userland trigger_error() notices/warnings route to the stderr log copy too
--DESCRIPTION--
E_USER_NOTICE / E_USER_WARNING raised via trigger_error() flow through the same
gated emitter as engine diagnostics: under the stock gates they land on STDERR in
`PHP Notice:` / `PHP Warning:` log shape, leaving program STDOUT clean.
--INI--
display_errors=0
log_errors=1
--FILE--
<?php
echo "start\n";
trigger_error("a notice", E_USER_NOTICE);
trigger_error("a warning", E_USER_WARNING);
echo "end\n";
?>
--EXPECT--
start
end
--EXPECT_STDERR--
PHP Notice:  a notice in %s on line %d
PHP Warning:  a warning in %s on line %d
