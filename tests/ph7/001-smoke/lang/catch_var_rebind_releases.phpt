--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rebinding the catch variable releases its prior value (runs the destructor)
--FILE--
<?php
class CatchRebindDtor {
    public $n;
    public function __construct($n) { $this->n = $n; }
    public function __destruct() { echo "dtor{$this->n}\n"; }
}
function catchRebindRun() {
    try {
        throw new Exception("a");
    } catch (Exception $e) {
        $e = new CatchRebindDtor(1);
        echo "c1\n";
    }
    // second catch rebinds $e in the same scope: the prior CatchRebindDtor(1)
    // must be released here (its destructor runs), not leaked.
    try {
        throw new Exception("b");
    } catch (Exception $e) {
        echo "c2\n";
    }
    echo "endfn\n";
}
catchRebindRun();
echo "done\n";
?>
--EXPECT--
c1
dtor1
c2
endfn
done
--CLEAN--
<?php
