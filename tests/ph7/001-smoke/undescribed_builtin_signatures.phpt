--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin with no signature row is unscreened and un-arity-checked
--DESCRIPTION--
aBuiltinSig[] is the single source of truth for a builtin's arity, its argument
types and what Reflection prints. Twenty-eight builtins had no row at all — the
mb_* family, the stream_*/proc_* families, token_get_all/token_name, chroot,
shell_exec, set_include_path, memory_reset_peak_usage — so nothing described
them to the shared screen and nothing bounded their arity: mb_strlen([1])
answered int(5), the length of the word "Array" the argument stringified to;
mb_substr('abcdef', 'x') answered the whole string, having read 'x' as offset 0;
token_name('nope') answered 'UNKNOWN'; and token_get_all([1]) tokenised the
string "Array". Five more had a row whose RETURN type disagreed with php's.
--FILE--
<?php
function ubsShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* An array reached these and stringified to "Array". */
ubsShow('mb_strlen', fn() => mb_strlen([1]));
ubsShow('mb_strwidth', fn() => mb_strwidth([1]));
ubsShow('mb_str_split', fn() => mb_str_split([1]));
ubsShow('token_get_all', fn() => token_get_all([1]));
ubsShow('shell_exec', fn() => shell_exec([1]));
ubsShow('set_include_path', fn() => set_include_path([1]));

/* A non-numeric string reached an int parameter and became 0. */
ubsShow('mb_substr offset', fn() => mb_substr('abcdef', 'x'));
ubsShow('mb_strpos offset', fn() => mb_strpos('abcdef', 'c', 'x'));
ubsShow('token_name', fn() => token_name('nope'));
ubsShow('mb_convert_case mode', fn() => mb_convert_case('abc', 'x'));

/* Arity is the row's too. */
ubsShow('mb_strlen no args', fn() => mb_strlen());
ubsShow('mb_strlen surplus', fn() => mb_strlen('abc', 'UTF-8', 'extra'));
ubsShow('token_name surplus', fn() => token_name(311, 'extra'));
ubsShow('memory_reset_peak_usage surplus', fn() => memory_reset_peak_usage('extra'));
ubsShow('stream_get_wrappers surplus', fn() => stream_get_wrappers('extra'));

/* Ordinary calls are untouched. */
ubsShow('mb_strlen ok', fn() => mb_strlen('héllo', 'UTF-8'));
ubsShow('mb_substr ok', fn() => mb_substr('abcdef', 2, 3));
ubsShow('mb_strpos ok', fn() => mb_strpos('abcdef', 'c'));
ubsShow('mb_str_split ok', fn() => mb_str_split('abc'));
ubsShow('mb_convert_case ok', fn() => mb_convert_case('ab cd', MB_CASE_TITLE));
ubsShow('token_name ok', fn() => token_name(T_ECHO));
ubsShow('mb_internal_encoding ok', fn() => mb_internal_encoding());

/* Reflection prints php's declaration for every one of them. */
foreach (['chroot', 'mb_strlen', 'mb_substr', 'proc_terminate', 'stream_get_contents',
          'token_name', 'hrtime', 'fsockopen'] as $ubsFn) {
    $ubsRef = new ReflectionFunction($ubsFn);
    $ubsOut = [];
    foreach ($ubsRef->getParameters() as $ubsP) {
        $ubsOut[] = ($ubsP->hasType() ? (string) $ubsP->getType() . ' ' : '')
            . ($ubsP->isPassedByReference() ? '&' : '') . '$' . $ubsP->getName();
    }
    echo $ubsFn, ' :: ', implode(', ', $ubsOut), ' : ',
        ($ubsRef->hasReturnType() ? (string) $ubsRef->getReturnType() : '-'), "\n";
}
--EXPECT--
mb_strlen => TypeError: mb_strlen(): Argument #1 ($string) must be of type string, array given
mb_strwidth => TypeError: mb_strwidth(): Argument #1 ($string) must be of type string, array given
mb_str_split => TypeError: mb_str_split(): Argument #1 ($string) must be of type string, array given
token_get_all => TypeError: token_get_all(): Argument #1 ($code) must be of type string, array given
shell_exec => TypeError: shell_exec(): Argument #1 ($command) must be of type string, array given
set_include_path => TypeError: set_include_path(): Argument #1 ($include_path) must be of type string, array given
mb_substr offset => TypeError: mb_substr(): Argument #2 ($start) must be of type int, string given
mb_strpos offset => TypeError: mb_strpos(): Argument #3 ($offset) must be of type int, string given
token_name => TypeError: token_name(): Argument #1 ($id) must be of type int, string given
mb_convert_case mode => TypeError: mb_convert_case(): Argument #2 ($mode) must be of type int, string given
mb_strlen no args => ArgumentCountError: mb_strlen() expects at least 1 argument, 0 given
mb_strlen surplus => ArgumentCountError: mb_strlen() expects at most 2 arguments, 3 given
token_name surplus => ArgumentCountError: token_name() expects exactly 1 argument, 2 given
memory_reset_peak_usage surplus => ArgumentCountError: memory_reset_peak_usage() expects exactly 0 arguments, 1 given
stream_get_wrappers surplus => ArgumentCountError: stream_get_wrappers() expects exactly 0 arguments, 1 given
mb_strlen ok => 5
mb_substr ok => 'cde'
mb_strpos ok => 2
mb_str_split ok => array (  0 => 'a',  1 => 'b',  2 => 'c',)
mb_convert_case ok => 'Ab Cd'
token_name ok => 'T_ECHO'
mb_internal_encoding ok => 'UTF-8'
chroot :: string $directory : bool
mb_strlen :: string $string, ?string $encoding : int
mb_substr :: string $string, int $start, ?int $length, ?string $encoding : string
proc_terminate :: $process, int $signal : bool
stream_get_contents :: $stream, ?int $length, int $offset : string|false
token_name :: int $id : string
hrtime :: bool $as_number : array|int|float|false
fsockopen :: string $hostname, int $port, &$error_code, &$error_message, ?float $timeout : -
