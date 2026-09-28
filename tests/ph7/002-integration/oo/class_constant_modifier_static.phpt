--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class constant takes no `static` modifier
--DESCRIPTION--
The modifier run is judged against the KIND of declaration it turned out to
modify, and php has a sentence per (modifier, kind) pair. A constant refuses
`static`, `readonly`, `abstract` and any `(set)` visibility.
--FILE--
<?php
class CcmC { static const K = 1; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot use the static modifier on a class constant %s
--CLEAN--
<?php
