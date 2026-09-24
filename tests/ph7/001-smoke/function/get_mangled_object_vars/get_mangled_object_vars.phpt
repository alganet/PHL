--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_mangled_object_vars keeps the visibility mangling on the keys
--FILE--
<?php
class GmovBase { public $pub = 1; protected $pro = 2; private $pri = 3; }
class GmovChild extends GmovBase { private $own = 4; }
$vars = get_mangled_object_vars(new GmovChild);
foreach ($vars as $k => $v) {
    echo str_replace("\0", '@', $k), " => ", $v, "\n";
}
?>
--EXPECT--
pub => 1
@*@pro => 2
@GmovBase@pri => 3
@GmovChild@own => 4
--CLEAN--
<?php
unset($vars);
