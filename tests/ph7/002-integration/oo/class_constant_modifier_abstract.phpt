--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant takes no `abstract` modifier
--DESCRIPTION--
PHL answered its own "Invalid property type or declaration near 'const'" here,
which describes the parser rather than the declaration.
--FILE--
<?php
class CcmaC { abstract const K = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the abstract modifier on a class constant %s
--CLEAN--
<?php
