--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe does not swallow exceptions thrown by the called method
--FILE--
<?php
class NsfExcBoom {
    public function go() { throw new \Exception("bang"); }
}
$nsfExc_a = new NsfExcBoom();
try {
    $nsfExc_a?->go();
    echo "no-throw\n";
} catch (\Exception $e) {
    echo "caught:", $e->getMessage(), "\n";
}
echo "done\n";
?>
--EXPECT--
caught:bang
done
--CLEAN--
<?php
unset($nsfExc_a);
