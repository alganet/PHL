--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: string
--FILE--
<?php
class TpUser {
    private string $name;
    public function __construct(string $name) { $this->name = $name; }
    public function getName(): string { return $this->name; }
}
$u = new TpUser("ada");
echo $u->getName(), "\n";
?>
--EXPECT--
ada
--CLEAN--
<?php
unset($u);
