--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strip_tags(): php's state machine (quotes, nesting, comments, php + xml + doctype) and the array allow-list
--FILE--
<?php
/* strip_tags() was "find a '<', drop through to the next '>'". php runs a state
 * machine, and everything it tracks beside the two brackets was a difference:
 * a '<' followed by whitespace is TEXT, a quoted attribute may hold a '>', a
 * nested '<' raises a depth the matching '>' lowers, a NUL is dropped rather
 * than ending the scan, and comments / php tags / doctypes each have their own
 * exit rule. An unterminated tag eats the rest of the input, where the old scan
 * put the tail back as text. */
function st($s, $allow = null)
{
    $r = $allow === null ? strip_tags($s) : strip_tags($s, $allow);
    printf("%-30s %-12s => %s\n", var_export($s, true),
        $allow === null ? '-' : json_encode($allow), var_export($r, true));
}
st('<b>x</b>');
st('a < b');            // '<' before a space is text
st('a <b');             // an unterminated tag eats the tail
st('<<>>');
st('a<b<c>d');          // the nested '<' raises the depth
st('<b class="a>b">y</b>');
st("<b href='>'>t");
st('a<!-- <b> -->c');
st('<?php echo "<b>"; ?>x');
st('<?xml version="1.0"?>t');
st('<!DOCTYPE html>d');
st("a\0b");
st("<a\0b>c");
st('</b>ok');
st('<b >x</b >');
// the allow list, in both spellings, with php's normalisation
st('<b>x</b><i>y</i>', '<b>');
st('<b>x</b><i>y</i>', ['b']);
st('<b>x</b><i>y</i>', ['b', 'i']);
st('<b>x</b><i>y</i>', ['B']);
st('<b>x</b><i>y</i>', []);
st('<b class="c">x</b>', ['b']);
st('<br/>x', ['br']);
st('<bo>x</bo>', ['b']);   // a prefix of an allowed name is not allowed
?>
--EXPECT--
'<b>x</b>'                     -            => 'x'
'a < b'                        -            => 'a < b'
'a <b'                         -            => 'a '
'<<>>'                         -            => ''
'a<b<c>d'                      -            => 'a'
'<b class="a>b">y</b>'         -            => 'y'
'<b href=\'>\'>t'              -            => 't'
'a<!-- <b> -->c'               -            => 'ac'
'<?php echo "<b>"; ?>x'        -            => 'x'
'<?xml version="1.0"?>t'       -            => 't'
'<!DOCTYPE html>d'             -            => 'd'
'a' . "\0" . 'b'               -            => 'ab'
'<a' . "\0" . 'b>c'            -            => 'c'
'</b>ok'                       -            => 'ok'
'<b >x</b >'                   -            => 'x'
'<b>x</b><i>y</i>'             "<b>"        => '<b>x</b>y'
'<b>x</b><i>y</i>'             ["b"]        => '<b>x</b>y'
'<b>x</b><i>y</i>'             ["b","i"]    => '<b>x</b><i>y</i>'
'<b>x</b><i>y</i>'             ["B"]        => '<b>x</b>y'
'<b>x</b><i>y</i>'             []           => 'xy'
'<b class="c">x</b>'           ["b"]        => '<b class="c">x</b>'
'<br/>x'                       ["br"]       => '<br/>x'
'<bo>x</bo>'                   ["b"]        => 'x'
--CLEAN--
<?php
