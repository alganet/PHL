--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sscanf() validates the whole format before it looks at the input
--FILE--
<?php
/* php reads the format TWICE: a validation pass raises every ValueError this
 * family has -- before a single byte of the subject is consulted -- and only
 * then does the scan run. So a bad specifier at the END refuses the call even
 * though the input would have satisfied everything before it. The `%n$`
 * positional spelling has four refusals of its own, and a format that simply
 * RAN OUT is reported with php's own truncated message: the offending
 * character is the terminator, and the text stops at the opening quote. */
function sscanf_err(string $subject, string $format, ...$vars): void {
    try {
        $r = sscanf($subject, $format, ...$vars);
        $out = str_replace("\n", '', var_export($r, true));
    } catch (\Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    printf("%-14s -> %s\n", var_export($format, true), $out);
}
echo "## bad conversion characters\n";
sscanf_err('x', '%z');
sscanf_err('0b11', '%b');
sscanf_err('x', '%G');
sscanf_err('5', '%-d');
sscanf_err('1 2', '%d %z');
sscanf_err('x', '%');
sscanf_err('x', '%*');
sscanf_err('x', '%l');
sscanf_err('x', 'abc%');

echo "## unclosed character sets\n";
sscanf_err('abc', '%[');
sscanf_err('abc', '%[]');
sscanf_err('abc', '%[^]');
sscanf_err('abc', '%[a-z');

echo "## the positional spelling\n";
sscanf_err('1 2', '%2$d %1$d');
sscanf_err('1 2', '%3$d %1$d');
sscanf_err('1 2', '%1$d %d');
sscanf_err('1 2', '%d %1$d');
sscanf_err('1 2', '%0$d');
sscanf_err('1 2', '%1$d %1$d');
sscanf_err('1 2', '%*d %1$d');
sscanf_err('1 2', '%1$d %*d');
sscanf_err('1', '%9$d');
sscanf_err('1', '%256$d');
--EXPECT--
## bad conversion characters
'%z'           -> ValueError: Bad scan conversion character "z"
'%b'           -> ValueError: Bad scan conversion character "b"
'%G'           -> ValueError: Bad scan conversion character "G"
'%-d'          -> ValueError: Bad scan conversion character "-"
'%d %z'        -> ValueError: Bad scan conversion character "z"
'%'            -> ValueError: Bad scan conversion character "
'%*'           -> ValueError: Bad scan conversion character "
'%l'           -> ValueError: Bad scan conversion character "
'abc%'         -> ValueError: Bad scan conversion character "
## unclosed character sets
'%['           -> ValueError: Unmatched [ in format string
'%[]'          -> ValueError: Unmatched [ in format string
'%[^]'         -> ValueError: Unmatched [ in format string
'%[a-z'        -> ValueError: Unmatched [ in format string
## the positional spelling
'%2$d %1$d'    -> array (  0 => 2,  1 => 1,)
'%3$d %1$d'    -> array (  0 => 2,  1 => NULL,  2 => 1,)
'%1$d %d'      -> ValueError: cannot mix "%" and "%n$" conversion specifiers
'%d %1$d'      -> ValueError: cannot mix "%" and "%n$" conversion specifiers
'%0$d'         -> ValueError: "%n$" argument index out of range
'%1$d %1$d'    -> ValueError: Variable is assigned by multiple "%n$" conversion specifiers
'%*d %1$d'     -> array (  0 => 2,)
'%1$d %*d'     -> array (  0 => 1,)
'%9$d'         -> array (  0 => NULL,  1 => NULL,  2 => NULL,  3 => NULL,  4 => NULL,  5 => NULL,  6 => NULL,  7 => NULL,  8 => 1,)
'%256$d'       -> ValueError: "%n$" argument index out of range
