--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named return/param type hints resolve against the current namespace
--FILE--
<?php
namespace RtnqOther {
    class Widget { public string $w = 'other-widget'; }
}
namespace {
    class RtnqBase { public string $g = 'global'; }
    class RtnqWidget { public string $g = 'global-widget'; }
}
namespace RtnqApp {
    use RtnqOther\Widget as AliasedWidget;

    class RtnqBase { public string $n = 'ns'; }

    class Service {
        // Bare name must resolve to RtnqApp\RtnqBase, not the global \RtnqBase.
        public function ret(): RtnqBase { return new RtnqBase(); }
        public function param(RtnqBase $b): string { return $b->n; }
        // Absolute name keeps the global class.
        public function abs(\RtnqBase $b): string { return $b->g; }
        // `use` alias resolves to the imported FQN.
        public function alias(AliasedWidget $w): string { return $w->w; }
    }

    $s = new Service();
    echo \get_class($s->ret()), "\n";
    echo $s->param(new RtnqBase()), "\n";
    echo $s->abs(new \RtnqBase()), "\n";
    echo $s->alias(new \RtnqOther\Widget()), "\n";
}
?>
--EXPECT--
RtnqApp\RtnqBase
ns
global
other-widget
--CLEAN--
<?php
