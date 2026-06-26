--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override that widens the return type is rejected at compile time
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL-specific diagnostic wording'; ?>
--FILE--
<?php
class OvrP { public function f(): int { return 1; } }
class OvrC extends OvrP { public function f(): string { return "x"; } }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of OvrC::f() must be compatible with OvrP::f()%A
--CLEAN--
<?php
