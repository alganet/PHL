--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fiber suspended inside a nested method call does not leak its self:: context to the resumer (php-exact; BYTECODE.md stage 4 aSelf parking)
--FILE--
<?php
class Worker {
    const NAME = "Worker";
    public function step() {
        echo "in ", self::NAME, "::step before\n";
        Fiber::suspend("w");
        echo "in ", self::NAME, "::step after\n";
        return "wdone";
    }
}
class Boss {
    const NAME = "Boss";
    public function run() {
        $f = new Fiber(function () {
            $w = new Worker();
            return $w->step();
        });
        $v = $f->start();
        echo self::NAME, " sees suspend=$v, my const=", self::NAME, "\n";
        $f->resume("go");
        echo self::NAME, " after resume, ret=", $f->getReturn(), "\n";
    }
}
(new Boss())->run();
?>
--EXPECT--
in Worker::step before
Boss sees suspend=w, my const=Boss
in Worker::step after
Boss after resume, ret=wdone
--CLEAN--
<?php
