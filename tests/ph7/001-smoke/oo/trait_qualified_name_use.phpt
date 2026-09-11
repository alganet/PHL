--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class may use a namespaced/fully-qualified trait name
--FILE--
<?php
namespace Qtnu\Space {
    trait Greeter { public function hi(): string { return 'hi from Greeter'; } }
}
namespace {
    class QtnuUser { use \Qtnu\Space\Greeter; }
    class QtnuUser2 { use Qtnu\Space\Greeter; }
    echo (new QtnuUser())->hi(), "\n";
    echo (new QtnuUser2())->hi(), "\n";
}
?>
--EXPECT--
hi from Greeter
hi from Greeter
