--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A declaration asks the autoloader for its parent, then its traits, then its interfaces
--FILE--
<?php
// php links a declaration in one order and asks for each dependency as it links it:
// the parent, then the traits, then the interfaces -- so a trait written INSIDE the
// body is asked for ahead of an interface written in the header.
// The smoke corpus runs in ONE interpreter, so this loader answers for its own
// names alone and leaves every other lookup to the ones already registered.
spl_autoload_register(function ($n) {
    if (strncmp($n, 'Alo', 3) !== 0) { return; }
    echo "$n ";
    if (str_starts_with($n, 'AloT')) { eval("trait $n {}"); }
    elseif (str_starts_with($n, 'AloI')) { eval("interface $n {}"); }
    else { eval("class $n {}"); }
});

echo "1: "; class Alo1 extends AloBase1 implements AloI1 { use AloT1; } echo "\n";
echo "2: "; class Alo2 implements AloI2, AloI3 { use AloT2, AloT3; } echo "\n";
echo "3: "; class Alo3 extends AloBase3 { use AloT4; } echo "\n";
echo "4: "; interface Alo4 extends AloI4, AloI5 {} echo "\n";
echo "5: "; class Alo5 implements AloI6 { use AloT5; use AloT6; } echo "\n";
echo "6: "; enum Alo6: int implements AloI7 { use AloT7; case X = 1; } echo "\n";
echo "7: "; abstract class Alo7 extends AloBase7 implements AloI8 { use AloT8; } echo "\n";
?>
--EXPECT--
1: AloBase1 AloT1 AloI1 
2: AloT2 AloT3 AloI2 AloI3 
3: AloBase3 AloT4 
4: AloI4 AloI5 
5: AloT5 AloT6 AloI6 
6: AloT7 AloI7 
7: AloBase7 AloT8 AloI8 
