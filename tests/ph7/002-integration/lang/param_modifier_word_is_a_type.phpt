--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A visibility word a namespace separator follows is a type, not a promotion
--DESCRIPTION--
`private\Q` is one qualified name to php's lexer, so the parameter is an ordinary typed one --
not a promoted property outside a constructor, which is what PHL used to call it. Same for
`public`, `protected` and `readonly`. The separator has to be GLUED to decide that: with a
space between them `private \Q` is a modifier and a type again, which is the shape every
promoted constructor property in the wild is written in.
--FILE--
<?php
namespace PmwiatZ\private { class Q { } }
namespace PmwiatZ\readonly { class Q { } }
namespace { class PmwiatHeld { } }
namespace PmwiatZ {
    function pmwiat(private\Q $q, readonly\Q $r): string {
        return \get_class($q) . '|' . \get_class($r);
    }
    class Promoted {
        public function __construct(private \PmwiatHeld $h, public readonly \PmwiatHeld $k) {}
        public function names(): string { return \get_class($this->h) . '|' . \get_class($this->k); }
    }
    echo pmwiat(new private\Q, new readonly\Q), "\n";
    echo (new Promoted(new \PmwiatHeld, new \PmwiatHeld))->names(), "\n";
}
?>
--EXPECT--
PmwiatZ\private\Q|PmwiatZ\readonly\Q
PmwiatHeld|PmwiatHeld
--CLEAN--
<?php
