--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Private constructor reachable from the declaring class when instantiating a subclass
--FILE--
<?php
abstract class PcifStatus {
    private function __construct() {}
    public static function unknown(): self { return new PcifUnknown(); }
    public static function known(): self { return new self(); }
    abstract public function label(): string;
}
final class PcifUnknown extends PcifStatus {
    public function label(): string { return 'unknown'; }
}
echo PcifStatus::unknown()->label(), "\n";
echo (PcifStatus::unknown() instanceof PcifUnknown) ? "isunknown\n" : "no\n";

// Global-scope instantiation of the private ctor is still rejected.
try {
    new PcifUnknown();
    echo "NOT REACHED\n";
} catch (\Error $e) {
    echo "rejected\n";
}
?>
--EXPECT--
unknown
isunknown
rejected
--CLEAN--
<?php
