--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: bool
--FILE--
<?php
class TpFlag {
    public bool $active = false;
}
$f = new TpFlag();
echo $f->active ? "yes" : "no", "\n";
$f->active = true;
echo $f->active ? "yes" : "no", "\n";
?>
--EXPECT--
no
yes
--CLEAN--
<?php
unset($f);
