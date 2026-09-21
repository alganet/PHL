--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `static function` closure is the one form accepted in a constant expression (global const, class const, nested in an array); its body is regular runtime code
--FILE--
<?php
const GLOB = static function () { return 'g'; };
class C {
    const CLS = static function () { return 'c'; };
    const ARR = [static function () { return 'a'; }];
    // Body is runtime code: a non-static closure NESTED inside is fine.
    const NEST = static function () { $inner = function () { return 'n'; }; return $inner(); };
}
$g = GLOB;
echo $g(), (C::CLS)(), (C::ARR[0])(), (C::NEST)(), "\n";
?>
--EXPECT--
gcan
--CLEAN--
<?php
