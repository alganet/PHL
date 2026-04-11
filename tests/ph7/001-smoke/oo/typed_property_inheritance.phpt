--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: subclass accesses inherited typed property
--FILE--
<?php
class TpAnimal {
    public string $name = "";
    public int $legs = 4;
}
class TpDog extends TpAnimal {
    public string $breed = "mutt";
}
$d = new TpDog();
$d->name = "Rex";
$d->breed = "labrador";
echo $d->name, " (", $d->breed, ") has ", $d->legs, " legs\n";
?>
--EXPECT--
Rex (labrador) has 4 legs
--CLEAN--
<?php
unset($d);
