--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constant visibility (public, protected, private)
--FILE--
<?php
class ConstVisibility {
    const BARE = "bare";
    public const PUB = "public";
    protected const PROT = "protected";
    private const PRIV = "private";

    public function getAll() {
        echo self::BARE . "\n";
        echo self::PUB . "\n";
        echo self::PROT . "\n";
        echo self::PRIV . "\n";
    }
}

$obj = new ConstVisibility();
$obj->getAll();

echo ConstVisibility::BARE . "\n";
echo ConstVisibility::PUB . "\n";
?>
--EXPECT--
bare
public
protected
private
bare
public
--CLEAN--
<?php
unset($obj);
