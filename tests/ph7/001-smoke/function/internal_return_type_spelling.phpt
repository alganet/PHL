--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An internal function's return type is spelled as php's stub spells it
--DESCRIPTION--
Five rows of the return-type sweep are internal FUNCTIONS whose declared type
this engine wrote in its own words. Four of them answer php's literal `true`
type — `hash_update` and the three `stream_context_set_*` never report a
failure, and php's stub says so with the type rather than with `bool` — so
getReturnType() named a type php does not name and a program screening on the
name saw `bool`. The fifth is `jddayofweek`, whose union php writes
`string|int`: a union prints in the STUB's order, not in a canonical one, and
this engine had sorted it. The values are unchanged in every case, which is
what makes the narrower type honest.
--FILE--
<?php
function irtRow($f) {
    $r = new ReflectionFunction($f);
    $t = $r->getReturnType();
    printf("%-28s %-14s named=%s builtin=%s\n", $f,
        $r->hasReturnType() ? (string)$t : '-',
        var_export($t instanceof ReflectionNamedType, true),
        $t instanceof ReflectionNamedType ? var_export($t->isBuiltin(), true) : '-');
}
/* php's four `true` returns: the literal type, not `bool`. */
irtRow('hash_update');
irtRow('stream_context_set_option');
irtRow('stream_context_set_options');
irtRow('stream_context_set_params');
/* Their neighbours in the same two families, which really are `bool`/`int`. */
irtRow('hash_update_file');
irtRow('hash_update_stream');
irtRow('stream_context_get_options');
/* A union prints in the STUB's order, not a canonical one. */
irtRow('jddayofweek');

/* The value each of the four answers, which is what makes `true` honest. */
echo var_export(stream_context_set_option(stream_context_create(), 'http', 'method', 'POST'), true), "\n";
echo var_export(stream_context_set_options(stream_context_create(), ['http' => ['method' => 'POST']]), true), "\n";
echo var_export(stream_context_set_params(stream_context_create(), ['options' => []]), true), "\n";
echo var_export(hash_update(hash_init('md5'), 'x'), true), "\n";
/* And both arms of the union, in the order the type names them. */
echo var_export(jddayofweek(2440588, 1), true), " ", var_export(jddayofweek(2440588, 0), true), "\n";
--EXPECT--
hash_update                  true           named=true builtin=true
stream_context_set_option    true           named=true builtin=true
stream_context_set_options   true           named=true builtin=true
stream_context_set_params    true           named=true builtin=true
hash_update_file             bool           named=true builtin=true
hash_update_stream           int            named=true builtin=true
stream_context_get_options   array          named=true builtin=true
jddayofweek                  string|int     named=false builtin=-
true
true
true
true
'Thursday' 4
