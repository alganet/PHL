--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A positional sprintf specifier does not move the sequential argument cursor
--FILE--
<?php
// php keeps two cursors: `%N$` reads argument N and leaves the sequential one
// (which only a number-less specifier advances) exactly where it was.
foreach (['%1$s|%s', '%s|%1$s|%s', '%2$s|%s|%s', '%s|%2$s|%s', '%3$s|%s',
          '%1$s%1$s%s', '%s%1$s%s'] as $sprintfCurFmt) {
    echo $sprintfCurFmt, ' => ', sprintf($sprintfCurFmt, 'a', 'b', 'c'), "\n";
}
echo vsprintf('%2$s|%s|%s', ['a', 'b', 'c']), "\n";

// The three numbers a specifier can carry are range-checked before the values
// are counted -- and each of them used to overflow instead.
foreach (['%0$s', '%2$s%0$s', '%2147483647$s', '%2147483648$s',
          '%2147483647d', '%2147483648d', '%99999999999999999999d',
          '%.2147483647f', '%.2147483648f',
          // ...and so is a custom-pad flag the format string ends on, which php
          // names in its own right rather than as the specifier it also lacks.
          "%'", "a%'", "%1\$'", "%'x"] as $sprintfCurBad) {
    try {
        $sprintfCurRet = sprintf($sprintfCurBad, 'a');
        echo $sprintfCurBad, ' => len ', strlen($sprintfCurRet), "\n";
    } catch (Throwable $sprintfCurE) {
        echo $sprintfCurBad, ' => ', get_class($sprintfCurE), ': ',
            $sprintfCurE->getMessage(), "\n";
    }
}
--EXPECT--
%1$s|%s => a|a
%s|%1$s|%s => a|a|b
%2$s|%s|%s => b|a|b
%s|%2$s|%s => a|b|b
%3$s|%s => c|a
%1$s%1$s%s => aaa
%s%1$s%s => aab
b|a|b
%0$s => ValueError: Argument number specifier must be greater than zero and less than 2147483647
%2$s%0$s => ValueError: Argument number specifier must be greater than zero and less than 2147483647
%2147483647$s => ValueError: Argument number specifier must be greater than zero and less than 2147483647
%2147483648$s => ValueError: Argument number specifier must be greater than zero and less than 2147483647
%2147483647d => ValueError: Width must be between 0 and 2147483647
%2147483648d => ValueError: Width must be between 0 and 2147483647
%99999999999999999999d => ValueError: Width must be between 0 and 2147483647
%.2147483647f => ValueError: Precision must be between 0 and 2147483647
%.2147483648f => ValueError: Precision must be between 0 and 2147483647
%' => ValueError: Missing padding character
a%' => ValueError: Missing padding character
%1$' => ValueError: Missing padding character
%'x => ValueError: Missing format specifier at end of string
--CLEAN--
<?php
