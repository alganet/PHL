--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A qualified name resolves its first segment through use-imports
--FILE--
<?php
namespace Qnir\Event {
    class Bus { public static function tag(): string { return 'bus'; } }
    function helper(): string { return 'help'; }
    const K = 42;
}
namespace Qnir\Event\Code {
    class Builder { public static function tag(): string { return 'builder'; } }
}
namespace Qnir\Other {
    class Bus { public static function tag(): string { return 'OTHER'; } }
}
namespace Qnir\Framework {
    use Qnir\Event;
    use Qnir\Other as Ot;

    // Leading segment 'Event' maps through `use Qnir\Event;` -> Qnir\Event\...
    echo Event\Bus::tag(), "\n";
    echo Event\Code\Builder::tag(), "\n";
    echo Event\helper(), "\n";
    echo Event\K, "\n";
    echo (new Event\Bus() instanceof Event\Bus) ? "isbus\n" : "no\n";
    // Aliased leading segment.
    echo Ot\Bus::tag(), "\n";
    // Absolute name is unaffected.
    echo \Qnir\Other\Bus::tag(), "\n";
}
?>
--EXPECT--
bus
builder
help
42
isbus
OTHER
OTHER
--CLEAN--
<?php
