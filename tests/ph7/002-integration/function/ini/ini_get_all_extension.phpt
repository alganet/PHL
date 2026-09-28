--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ini_get_all($extension) is the MODULE REGISTRY's key: folded, exact, and `core` means all
--FILE--
<?php
// The $extension filter used to know five names spelled this engine's own way
// (Core, session, date, standard, bcmath) and refuse everything else, and it
// picked directives by their NAME prefix. php looks the name up in the module
// registry, whose key is the extension name FOLDED -- so the match is
// case-SENSITIVE against that key. Only names both engines carry are pinned.
foreach (['core', 'Core', 'standard', 'STANDARD', 'date', 'DATE', 'json', 'JSON',
          'session', 'bcmath', 'spl', 'SPL', 'reflection', 'tokenizer',
          'igaenosuchextension', ''] as $igaeName) {
    $igae = @ini_get_all($igaeName);
    printf("%-20s %s\n", $igaeName === '' ? '(empty)' : $igaeName,
        $igae === false ? 'false' : 'array');
}

// An extension that owns no directive is still FOUND and answers the empty
// array; only a name the registry has no module for is the warning and false.
$igae = ini_get_all('json');
var_dump($igae === [], count(ini_get_all('session')) > 0);

// php's own exception: `core` is every directive, whatever module registered it.
var_dump(count(ini_get_all('core')) === count(ini_get_all()));

// A directive belongs where the partition puts it, not where its name points --
// so `standard` does not pick a directive up just for being prefixed, while
// `core` carries it under the all-rule above.
$igaeSession = ini_get_all('session');
$igaeCore = ini_get_all('core');
var_dump(isset($igaeSession['session.name']), isset($igaeCore['session.name']));
var_dump(isset(ini_get_all('standard')['session.name']));
var_dump(isset(ini_get_all('date')['date.timezone']));

// The refusal names the extension the caller asked for.
$igaeWarn = [];
set_error_handler(function ($n, $s) use (&$igaeWarn) { $igaeWarn[] = $s; return true; });
ini_get_all('igaeNoSuchExtension');
restore_error_handler();
echo $igaeWarn[0], "\n";
?>
--EXPECT--
core                 array
Core                 false
standard             array
STANDARD             false
date                 array
DATE                 false
json                 array
JSON                 false
session              array
bcmath               array
spl                  array
SPL                  false
reflection           array
tokenizer            array
igaenosuchextension  false
(empty)              false
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
ini_get_all(): Extension "igaeNoSuchExtension" cannot be found
--CLEAN--
<?php
