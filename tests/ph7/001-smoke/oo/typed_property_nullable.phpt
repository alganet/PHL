--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: nullable
--FILE--
<?php
class TpProfile {
    public ?string $nickname = null;
    public ?int $age = null;
}
$p = new TpProfile();
echo is_null($p->nickname) ? "null" : $p->nickname, "\n";
echo is_null($p->age) ? "null" : $p->age, "\n";
$p->nickname = "ada";
$p->age = 36;
echo $p->nickname, "\n", $p->age, "\n";
$p->nickname = null;
echo is_null($p->nickname) ? "null" : $p->nickname, "\n";
?>
--EXPECT--
null
null
ada
36
null
--CLEAN--
<?php
unset($p);
