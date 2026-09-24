--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_declared_traits lists traits, and get_declared_classes does not
--FILE--
<?php
trait GdtSmokeTrait { public function gdtHello() { return "hi"; } }
class GdtSmokeClass { use GdtSmokeTrait; }
$traits = get_declared_traits();
$classes = get_declared_classes();
var_dump(is_array($traits));
var_dump(in_array('GdtSmokeTrait', $traits, true));
var_dump(in_array('GdtSmokeTrait', $classes, true));
var_dump(in_array('GdtSmokeClass', $classes, true));
var_dump(in_array('GdtSmokeClass', $traits, true));
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
bool(false)
--CLEAN--
<?php
unset($traits, $classes);
