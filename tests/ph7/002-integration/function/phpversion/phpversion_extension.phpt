--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phpversion()'s $extension: a version for every loaded extension, php's FALSE for anything else
--FILE--
<?php
// phpversion()'s $extension was declared in the signature and answered NULL for
// everything: an UNKNOWN one where php answers FALSE -- so the documented
// `if (phpversion($e) === false)` check never fired and the version comparison
// that follows it ran against NULL -- a KNOWN one where php answers a version
// string, and even the explicit NULL that means "no extension at all".
//
// The assertions are about the SHAPE, since the version string and the set of
// extensions are each engine's own.
var_dump(phpversion() === PHP_VERSION);
var_dump(phpversion(null) === PHP_VERSION);

// Every extension the engine reports as loaded has a version, and it is the one
// php answers for its own bundled extensions: the engine's.
$pvx_bad = [];
foreach (get_loaded_extensions() as $pvx_e) {
    $pvx_v = phpversion($pvx_e);
    if (!is_string($pvx_v) || $pvx_v === '') { $pvx_bad[] = $pvx_e . '=' . var_export($pvx_v, true); }
    // ...and the lookup is case-insensitive, as extension_loaded()'s is.
    if (phpversion(strtoupper($pvx_e)) !== $pvx_v) { $pvx_bad[] = $pvx_e . ' case'; }
    if (phpversion(strtolower($pvx_e)) !== $pvx_v) { $pvx_bad[] = $pvx_e . ' case'; }
}
echo 'loaded extensions without a version: ', implode(',', $pvx_bad) ?: 'none', "\n";
var_dump(phpversion('Core') === PHP_VERSION, phpversion('core') === PHP_VERSION);

// A name the engine does not report is php's FALSE -- not NULL, and not a string.
foreach (['nosuchextension', '', 'PHL', '0', 'core '] as $pvx_e) {
    var_dump(phpversion($pvx_e));
}
// extension_loaded() and phpversion() answer from the same list.
$pvx_mismatch = [];
foreach (['Core', 'json', 'nosuchextension', 'spl', 'zzz'] as $pvx_e) {
    if (extension_loaded($pvx_e) !== is_string(phpversion($pvx_e))) { $pvx_mismatch[] = $pvx_e; }
}
echo 'disagreements with extension_loaded(): ', implode(',', $pvx_mismatch) ?: 'none', "\n";
?>
--EXPECT--
bool(true)
bool(true)
loaded extensions without a version: none
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
disagreements with extension_loaded(): none
