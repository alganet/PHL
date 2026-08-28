--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: DateTime parse failures throw php's exact DateMalformedStringException messages
--FILE--
<?php
$inputs = [
    'total nonsense',
    'next xyz',
    '@abc',
    '2024-13-45 99:99:99',
    '2024-91-05',
    '2024-01-45',
    '2024-01-15 25:00:00',
    '2024-01-15 19:99:00',
    '2024-01-15 19:00:99',
    '25:30',
    '10:99',
    '10:30:99',
];
foreach ($inputs as $s) {
    try {
        new DateTime($s, new DateTimeZone('UTC'));
        echo "OK($s)\n";
    } catch (DateMalformedStringException $e) {
        echo $e->getMessage(), "\n";
    }
}

// modify() keeps its method prefix; the constructor does not
try {
    (new DateTime('@0'))->modify('bogus!');
} catch (DateMalformedStringException $e) {
    echo $e->getMessage(), "\n";
}

// month/day zero normalize instead of failing
echo (new DateTime('2024-00-15', new DateTimeZone('UTC')))->format('Y-m-d'), "\n";
echo (new DateTime('2024-01-00', new DateTimeZone('UTC')))->format('Y-m-d'), "\n";

// unknown zone name (PHL has no tz database; php agrees on truly-bad ids)
try {
    new DateTimeZone('Xyz/Abc');
} catch (DateInvalidTimeZoneException $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Failed to parse time string (total nonsense) at position 0 (t): The timezone could not be found in the database
Failed to parse time string (next xyz) at position 0 (n): The timezone could not be found in the database
Failed to parse time string (@abc) at position 0 (@): Unexpected character
Failed to parse time string (2024-13-45 99:99:99) at position 6 (3): Unexpected character
Failed to parse time string (2024-91-05) at position 6 (1): Unexpected character
Failed to parse time string (2024-01-45) at position 9 (5): Unexpected character
Failed to parse time string (2024-01-15 25:00:00) at position 11 (2): Unexpected character
Failed to parse time string (2024-01-15 19:99:00) at position 15 (9): Double time specification
Failed to parse time string (2024-01-15 19:00:99) at position 18 (9): Unexpected character
Failed to parse time string (25:30) at position 0 (2): Unexpected character
Failed to parse time string (10:99) at position 4 (9): Unexpected character
Failed to parse time string (10:30:99) at position 7 (9): Unexpected character
DateTime::modify(): Failed to parse time string (bogus!) at position 0 (b): The timezone could not be found in the database
2023-12-15
2023-12-31
DateTimeZone::__construct(): Unknown or bad timezone (Xyz/Abc)
--CLEAN--
<?php
unset($inputs, $s);
