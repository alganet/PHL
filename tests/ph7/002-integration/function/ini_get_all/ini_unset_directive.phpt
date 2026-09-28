--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A directive php declares with no value reports NULL, not the empty string
--DESCRIPTION--
php has THREE states for a directive's value and this engine had two: set,
written, and never given one at all. The third is what `error_log` and
`sqlite3.extension_dir` are in, and every surface that shows the RAW value --
`ini_get_all()` in both its shapes, and `ReflectionExtension::getINIEntries()`
-- answers NULL for them. The empty string is a different thing entirely: it is
a value, and a script can write it.

`ini_get()` cannot tell the two apart, because it answers the empty string
either way; `get_cfg_var()` can, and answers false -- it reads the php.ini FILE
rather than the live directive, so a name the file never mentioned stays false
even after a write.

The write itself shows the rule the two halves come from. php saves the
ORIGINAL value at the first modification and reports that as `global_value`, so
a directive with a default keeps its own across a write -- and one that was
never given a value has no original to save, so its global starts reading the
written value instead. Only `ini_restore()` puts the NULL back.
--FILE--
<?php
$show = function ($label, $fn) {
    try { $out = json_encode($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo str_pad($label, 32), ' => ', $out, "\n";
};
$row = fn ($n) => ini_get_all(null, true)[$n] ?? 'absent';

/* php declares these two with no value at all, and NULL is what every surface
   that shows the raw value answers for them. */
foreach (['error_log', 'sqlite3.extension_dir'] as $name) {
    $show("$name row", fn () => $row($name));
    $show("$name flat", fn () => array_key_exists($name, ini_get_all(null, false))
                                 ? ini_get_all(null, false)[$name] : 'absent');
    $show("$name ini_get", fn () => ini_get($name));
    $show("$name get_cfg_var", fn () => get_cfg_var($name));
}

/* A write leaves a local value -- and, because there was no original to keep,
   the global reads it too. Only ini_restore() puts the NULL back. */
$show('write it', fn () => ini_set('error_log', '/tmp/phl-ini-null.log'));
$show('row after the write', fn () => $row('error_log'));
$show('ini_get after the write', fn () => ini_get('error_log'));
$show('get_cfg_var is still false', fn () => get_cfg_var('error_log'));
$show('restore', fn () => ini_restore('error_log'));
$show('row after the restore', fn () => $row('error_log'));

/* The EMPTY string is a value, and writing it is not the same as being unset. */
$show('write the empty string', fn () => ini_set('error_log', ''));
$show('row after that', fn () => $row('error_log'));
$show('restore again', fn () => ini_restore('error_log'));
$show('row after that', fn () => $row('error_log'));

/* A directive that HAS a default keeps its global across a write, which is the
   other half of the same rule. */
$show('a directive with a default', fn () => $row('precision'));
$show('write it', fn () => ini_set('precision', '9'));
$show('its global is the ORIGINAL', fn () => $row('precision'));
$show('restore', fn () => ini_restore('precision'));
$show('row after the restore', fn () => $row('precision'));
--EXPECT--
error_log row                    => {"global_value":null,"local_value":null,"access":7}
error_log flat                   => null
error_log ini_get                => ""
error_log get_cfg_var            => false
sqlite3.extension_dir row        => {"global_value":null,"local_value":null,"access":4}
sqlite3.extension_dir flat       => null
sqlite3.extension_dir ini_get    => ""
sqlite3.extension_dir get_cfg_var => false
write it                         => ""
row after the write              => {"global_value":"\/tmp\/phl-ini-null.log","local_value":"\/tmp\/phl-ini-null.log","access":7}
ini_get after the write          => "\/tmp\/phl-ini-null.log"
get_cfg_var is still false       => false
restore                          => null
row after the restore            => {"global_value":null,"local_value":null,"access":7}
write the empty string           => ""
row after that                   => {"global_value":"","local_value":"","access":7}
restore again                    => null
row after that                   => {"global_value":null,"local_value":null,"access":7}
a directive with a default       => {"global_value":"14","local_value":"14","access":7}
write it                         => "14"
its global is the ORIGINAL       => {"global_value":"14","local_value":"9","access":7}
restore                          => null
row after the restore            => {"global_value":"14","local_value":"14","access":7}
