--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A promoted parameter's modifier run refuses a modifier written twice
--DESCRIPTION--
The promotion-modifier run reads each modifier at most once, exactly as a class
body's does, and php words the duplicate identically wherever it was written.
--FILE--
<?php
class PpdC { public function __construct(public public int $p) {} }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Multiple access type modifiers are not allowed %s
--CLEAN--
<?php
