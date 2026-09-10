--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
new self / new static / new parent are not namespace-qualified
--FILE--
<?php
namespace Nsspc\App;

class Base {
    public function who(): string { return 'base'; }
    public static function make(): static { return new static(); }
    private static ?self $inst = null;
    public static function singleton(): self { return self::$inst ??= new self(); }
}

class Child extends Base {
    public function who(): string { return 'child'; }
    // No return type hint here: this test is about `new parent()`, and a `: Base`
    // hint would tangle with the separate return-type namespace-qualification issue.
    public function makeParent() { return new parent(); }
}

echo (new Child())->who(), "\n";          // child
echo Child::make()->who(), "\n";          // child (new static)
echo (new Child())->makeParent()->who(), "\n"; // base (new parent)
echo Base::singleton()->who(), "\n";      // base (new self)
echo \get_class(Base::singleton()), "\n"; // Nsspc\App\Base
?>
--EXPECT--
child
child
base
base
Nsspc\App\Base
--CLEAN--
<?php
