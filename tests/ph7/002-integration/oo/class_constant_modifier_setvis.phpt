--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant takes no asymmetric `(set)` visibility
--DESCRIPTION--
php names the modifier as written, `private(set)` and all.
--FILE--
<?php
class CcmsC { public private(set) const K = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the private(set) modifier on a class constant %s
--CLEAN--
<?php
