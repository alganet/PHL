--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `new` inside a `static function` constant-expression body is a call-time new, not a constant-expression `new`: the new-expression scan must skip the closure body (GenStateInitHasNewExpr regression guard)
--FILE--
<?php
// The closure body is regular runtime code; its `new` runs when the closure is
// invoked. GenStateInitHasNewExpr must skip the whole `static function(){...}`
// construct so the "New expressions are not supported in this context" reject
// does not fire for it. `static function` is the only closure form both engines
// accept in a constant expression, so this is cross-engine (the earlier PHL-only
// vehicle used an arrow-fn property default, which PHP -- and now PHL -- reject).
const GF = static function () { return new stdClass(); };
class C {
    const CF = static function () { return new stdClass(); };
}
$g = GF;
echo get_class($g()), ' ', get_class((C::CF)()), "\n";
?>
--EXPECT--
stdClass stdClass
--CLEAN--
<?php
