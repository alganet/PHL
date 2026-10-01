--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The timezone family's declared surface: which extension owns it, and each signature
--FILE--
<?php
/* get_extension_funcs() answers in php's own registration order, not
 * alphabetically, so the ORDER here is part of the expectation. */
foreach (get_extension_funcs('date') as $n) {
    if (str_starts_with($n, 'timezone')) {
        echo $n, "\n";
    }
}

echo "\n";
foreach (['timezone_location_get', 'timezone_version_get', 'timezone_transitions_get',
          'timezone_identifiers_list', 'timezone_abbreviations_list',
          'timezone_name_from_abbr'] as $n) {
    $r = new ReflectionFunction($n);
    $p = [];
    foreach ($r->getParameters() as $x) {
        $p[] = ($x->isOptional() ? '?' : '') . $x->getName();
    }
    printf("%-28s (%s) : %s\n", $n, implode(', ', $p), (string) $r->getReturnType());
}

echo "\n";
$m = new ReflectionMethod('DateTimeZone', 'getLocation');
printf("getLocation      %d param(s) : %s\n",
    $m->getNumberOfParameters(), (string) $m->getReturnType());
?>
--EXPECT--
timezone_open
timezone_name_get
timezone_name_from_abbr
timezone_offset_get
timezone_transitions_get
timezone_location_get
timezone_identifiers_list
timezone_abbreviations_list
timezone_version_get

timezone_location_get        (object) : array|false
timezone_version_get         () : string
timezone_transitions_get     (object, ?timestampBegin, ?timestampEnd) : array|false
timezone_identifiers_list    (?timezoneGroup, ?countryCode) : array
timezone_abbreviations_list  () : array
timezone_name_from_abbr      (abbr, ?utcOffset, ?isDST) : string|false

getLocation      0 param(s) : 
