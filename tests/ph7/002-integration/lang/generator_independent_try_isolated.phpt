--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two independent generators each suspended inside their own try stay isolated
--FILE--
<?php
function gen($id) {
    try { yield "$id-a"; yield "$id-b"; }
    catch (\Throwable $e) { echo "$id GEN WRONGLY CAUGHT\n"; }
}
$g1 = gen(1); $g2 = gen(2);
echo $g1->current(), " ", $g2->current(), "\n";
try { throw new Exception("x"); } catch (Exception $e) { echo "caught\n"; }
$g1->next(); $g2->next();
echo $g1->current(), " ", $g2->current(), "\n";
echo "done\n";
?>
--EXPECT--
1-a 2-a
caught
1-b 2-b
done
--CLEAN--
<?php
