--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
listIdentifiers(): the group argument is not the bitmask it looks like
--FILE--
<?php
/* DateTimeZone::listIdentifiers() and its timezone_identifiers_list() twin.
 *
 * The group argument LOOKS like a plain bitmask and is not: two values are
 * compared EXACTLY, which is why `-1` is ALL and not ALL_WITH_BC even though it
 * has every bit set, and why `PER_COUNTRY|AFRICA` is Africa alone with its
 * country code ignored. Only the bare PER_COUNTRY demands one, and a null or
 * non-two-character code there is a ValueError -- while any two bytes are
 * accepted and simply match nothing, so `12` and the lower-case `br` are empty
 * arrays where `BR` is sixteen zones.
 *
 * The print order is case-INSENSITIVE. It cannot be seen in the 419 canonical
 * `Continent/City` names, which come out the same either way; only
 * ALL_WITH_BC's odd-cased backward links tell the two apart, and there `CET`
 * sits between `Canada/Yukon` and `Chile/Continental` rather than ahead of
 * every lower-case name. */
$P = DateTimeZone::PER_COUNTRY;
$consts = (new ReflectionClass('DateTimeZone'))->getConstants();
ksort($consts);
print_r($consts);

foreach ([[$P, null], [$P | 1, null], [-1, null], [0, null], [2047, null], [99999, null],
          [$P, 'BR'], [$P | 1, 'BR'], [$P | 4095, 'BR'], [1, 'BR'], [$P, ''], [$P, 'B'],
          [$P, 'b1'], [$P, '12'], [$P, 'US'], [1024, null], [128, null], [1 | 128, null]] as [$g, $cc]) {
    try {
        $r = $cc === null ? DateTimeZone::listIdentifiers($g) : DateTimeZone::listIdentifiers($g, $cc);
        printf("g=%-6d c=%-6s -> %3d [%s]\n", $g, var_export($cc, true), count($r),
            implode(',', array_slice($r, 0, 3)));
    } catch (Throwable $e) {
        printf("g=%-6d c=%-6s -> %s: %s\n", $g, var_export($cc, true), get_class($e), $e->getMessage());
    }
}

$all = DateTimeZone::listIdentifiers();
printf("ALL=%d first=%s last=%s md5=%s\n", count($all), $all[0], $all[count($all) - 1],
    md5(implode("\n", $all)));
foreach (['AFRICA','AMERICA','ANTARCTICA','ARCTIC','ASIA','ATLANTIC','AUSTRALIA',
          'EUROPE','INDIAN','PACIFIC','UTC'] as $g) {
    $l = DateTimeZone::listIdentifiers(constant("DateTimeZone::$g"));
    printf("%-11s %3d md5=%s\n", $g, count($l), md5(implode("\n", $l)));
}
foreach (['BR','US','GB','FR','JP','AU','RU','CN','AQ','ZZ'] as $k) {
    printf("%s=%d ", $k, count(DateTimeZone::listIdentifiers($P, $k)));
}
echo "\n";
var_dump(timezone_identifiers_list() === DateTimeZone::listIdentifiers());
var_dump(timezone_identifiers_list($P, 'BR') === DateTimeZone::listIdentifiers($P, 'BR'));
try { timezone_identifiers_list($P); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
/* Every identifier the list hands out is one the constructor takes -- which is
 * not true of a php built --with-system-tzdata, whose ALL_WITH_BC walk hands
 * back two files that are not zones at all. */
$bad = [];
foreach ($all as $z) {
    try { new DateTimeZone($z); } catch (Throwable $e) { $bad[] = $z; }
}
printf("unconstructible=%d\n", count($bad));
?>
--EXPECT--
Array
(
    [AFRICA] => 1
    [ALL] => 2047
    [ALL_WITH_BC] => 4095
    [AMERICA] => 2
    [ANTARCTICA] => 4
    [ARCTIC] => 8
    [ASIA] => 16
    [ATLANTIC] => 32
    [AUSTRALIA] => 64
    [EUROPE] => 128
    [INDIAN] => 256
    [PACIFIC] => 512
    [PER_COUNTRY] => 4096
    [UTC] => 1024
)
g=4096   c=NULL   -> ValueError: DateTimeZone::listIdentifiers(): Argument #2 ($countryCode) must be a two-letter ISO 3166-1 compatible country code when argument #1 ($timezoneGroup) is DateTimeZone::PER_COUNTRY
g=4097   c=NULL   ->  52 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=-1     c=NULL   -> 419 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=0      c=NULL   ->   0 []
g=2047   c=NULL   -> 419 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=99999  c=NULL   -> 387 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=4096   c='BR'   ->  16 [America/Araguaina,America/Bahia,America/Belem]
g=4097   c='BR'   ->  52 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=8191   c='BR'   -> 419 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=1      c='BR'   ->  52 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
g=4096   c=''     -> ValueError: DateTimeZone::listIdentifiers(): Argument #2 ($countryCode) must be a two-letter ISO 3166-1 compatible country code when argument #1 ($timezoneGroup) is DateTimeZone::PER_COUNTRY
g=4096   c='B'    -> ValueError: DateTimeZone::listIdentifiers(): Argument #2 ($countryCode) must be a two-letter ISO 3166-1 compatible country code when argument #1 ($timezoneGroup) is DateTimeZone::PER_COUNTRY
g=4096   c='b1'   ->   0 []
g=4096   c='12'   ->   0 []
g=4096   c='US'   ->  29 [America/Adak,America/Anchorage,America/Boise]
g=1024   c=NULL   ->   1 [UTC]
g=128    c=NULL   ->  58 [Europe/Amsterdam,Europe/Andorra,Europe/Astrakhan]
g=129    c=NULL   -> 110 [Africa/Abidjan,Africa/Accra,Africa/Addis_Ababa]
ALL=419 first=Africa/Abidjan last=UTC md5=cc6be5fd058803379ff1069fe5c1ab39
AFRICA       52 md5=bc3626f9102dce2f8e42a682f332d757
AMERICA     144 md5=77c184e2de71c26d5d1c09ccafda188b
ANTARCTICA   11 md5=d16d8205424bb8b7715a1a6ff6dd3145
ARCTIC        1 md5=9617dd9fd6c33e351243132da3cc6028
ASIA         82 md5=fd8421498021b98807e968f85b127fd7
ATLANTIC     10 md5=ec2c2bc2be32bba0147523dfc60b380e
AUSTRALIA    11 md5=e08a506c437635fc0659a073eadf0735
EUROPE       58 md5=0ffc52ebafe456a347afbc98562de5ed
INDIAN       11 md5=9c91a68b476e8e0f6f8a00a8e2ed1c82
PACIFIC      38 md5=e33484da6d956147b88bb358a05dd50f
UTC           1 md5=9234324ddf6b4176b57d803a925b7961
BR=16 US=29 GB=1 FR=1 JP=1 AU=12 RU=26 CN=2 AQ=10 ZZ=0 
bool(true)
bool(true)
ValueError: timezone_identifiers_list(): Argument #2 ($countryCode) must be a two-letter ISO 3166-1 compatible country code when argument #1 ($timezoneGroup) is DateTimeZone::PER_COUNTRY
unconstructible=0
