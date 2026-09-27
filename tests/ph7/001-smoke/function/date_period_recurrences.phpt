--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DatePeriod's recurrence count has php's two range checks and its ceiling
--FILE--
<?php
/* PHL accepted every recurrence count there is -- `0` and `-1` included -- and
 * built a period php refuses to make, then iterated it. php checks the number
 * TWICE, in this order and with two different exception classes:
 *  - the number the caller wrote must be 1..2147483639, else a
 *    DateMalformedPeriodStringException;
 *  - that number PLUS the dates the options ask for (the start date unless
 *    EXCLUDE_START_DATE, the end date if INCLUDE_END_DATE) must still be under
 *    the ceiling, else a DateMalformedStringException whose sentence ends
 *    "(including options)" -- which is why 2147483639 alone is refused while the
 *    same count with EXCLUDE_START_DATE is built.
 * The ISO spelling has its own two rules: `R0` is a MISSING count rather than an
 * out-of-range one, and the scanner reads at most NINE digits of it, so
 * `R2147483639/...` is 214748363 recurrences there (PHL read them all). */
date_default_timezone_set('UTC');
$start = new DateTime('2020-01-01 00:00:00');
$iv = new DateInterval('P1D');

echo "--- the count, and the count plus its options\n";
foreach ([-2147483648, -1, 0, 1, 2, 2147483638, 2147483639, 2147483640, PHP_INT_MAX] as $n) {
    foreach ([0, 1, 2, 3] as $opt) {
        try {
            $p = new DatePeriod($start, $iv, $n, $opt);
            printf("n=%-20d opt=%d rec=%s incS=%s incE=%s\n", $n, $opt,
                var_export($p->getRecurrences(), true),
                var_export($p->include_start_date, true),
                var_export($p->include_end_date, true));
        } catch (Throwable $e) {
            printf("n=%-20d opt=%d %s: %s\n", $n, $opt, get_class($e), $e->getMessage());
        }
    }
}

echo "--- and how many dates a legal one yields\n";
foreach ([[1, 0], [2, 0], [2, 1], [2, 2], [2, 3], [3, 0]] as $case) {
    $p = new DatePeriod($start, $iv, $case[0], $case[1]);
    $seen = [];
    foreach ($p as $d) {
        $seen[] = $d->format('m-d');
    }
    printf("n=%d opt=%d -> %d [%s]\n", $case[0], $case[1], count($seen), implode(' ', $seen));
}

echo "--- the ISO spelling refuses and truncates differently\n";
foreach (['R0/2020-01-01T00:00:00Z/P1D', 'R00/2020-01-01T00:00:00Z/P1D',
          'R1/2020-01-01T00:00:00Z/P1D', 'R000000000/2020-01-01T00:00:00Z/P1D',
          'R0000000001/2020-01-01T00:00:00Z/P1D', 'R123456789/2020-01-01T00:00:00Z/P1D',
          'R1234567890/2020-01-01T00:00:00Z/P1D', 'R2147483639/2020-01-01T00:00:00Z/P1D'] as $spec) {
    foreach (['new', 'factory'] as $door) {
        try {
            $p = $door === 'new' ? new DatePeriod($spec) : DatePeriod::createFromISO8601String($spec);
            printf("%-46s %-8s rec=%s start=%s\n", $spec, $door,
                var_export($p->getRecurrences(), true), get_class($p->getStartDate()));
        } catch (Throwable $e) {
            printf("%-46s %-8s %s: %s\n", $spec, $door, get_class($e), $e->getMessage());
        }
    }
}
?>
--EXPECT--
--- the count, and the count plus its options
n=-2147483648          opt=0 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-2147483648          opt=1 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-2147483648          opt=2 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-2147483648          opt=3 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-1                   opt=0 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-1                   opt=1 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-1                   opt=2 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=-1                   opt=3 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=0                    opt=0 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=0                    opt=1 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=0                    opt=2 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=0                    opt=3 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=1                    opt=0 rec=1 incS=true incE=false
n=1                    opt=1 rec=1 incS=false incE=false
n=1                    opt=2 rec=1 incS=true incE=true
n=1                    opt=3 rec=1 incS=false incE=true
n=2                    opt=0 rec=2 incS=true incE=false
n=2                    opt=1 rec=2 incS=false incE=false
n=2                    opt=2 rec=2 incS=true incE=true
n=2                    opt=3 rec=2 incS=false incE=true
n=2147483638           opt=0 rec=2147483638 incS=true incE=false
n=2147483638           opt=1 rec=2147483638 incS=false incE=false
n=2147483638           opt=2 DateMalformedStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640 (including options)
n=2147483638           opt=3 rec=2147483638 incS=false incE=true
n=2147483639           opt=0 DateMalformedStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640 (including options)
n=2147483639           opt=1 rec=2147483639 incS=false incE=false
n=2147483639           opt=2 DateMalformedStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640 (including options)
n=2147483639           opt=3 DateMalformedStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640 (including options)
n=2147483640           opt=0 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=2147483640           opt=1 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=2147483640           opt=2 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=2147483640           opt=3 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=9223372036854775807  opt=0 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=9223372036854775807  opt=1 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=9223372036854775807  opt=2 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
n=9223372036854775807  opt=3 DateMalformedPeriodStringException: DatePeriod::__construct(): Recurrence count must be greater or equal to 1 and lower than 2147483640
--- and how many dates a legal one yields
n=1 opt=0 -> 2 [01-01 01-02]
n=2 opt=0 -> 3 [01-01 01-02 01-03]
n=2 opt=1 -> 2 [01-02 01-03]
n=2 opt=2 -> 4 [01-01 01-02 01-03 01-04]
n=2 opt=3 -> 3 [01-02 01-03 01-04]
n=3 opt=0 -> 4 [01-01 01-02 01-03 01-04]
--- the ISO spelling refuses and truncates differently
R0/2020-01-01T00:00:00Z/P1D                    new      DateMalformedPeriodStringException: DatePeriod::__construct(): ISO interval must contain an end date or a recurrence count, "R0/2020-01-01T00:00:00Z/P1D" given
R0/2020-01-01T00:00:00Z/P1D                    factory  DateMalformedPeriodStringException: DatePeriod::createFromISO8601String(): ISO interval must contain an end date or a recurrence count, "R0/2020-01-01T00:00:00Z/P1D" given
R00/2020-01-01T00:00:00Z/P1D                   new      DateMalformedPeriodStringException: DatePeriod::__construct(): ISO interval must contain an end date or a recurrence count, "R00/2020-01-01T00:00:00Z/P1D" given
R00/2020-01-01T00:00:00Z/P1D                   factory  DateMalformedPeriodStringException: DatePeriod::createFromISO8601String(): ISO interval must contain an end date or a recurrence count, "R00/2020-01-01T00:00:00Z/P1D" given
R1/2020-01-01T00:00:00Z/P1D                    new      rec=1 start=DateTime
R1/2020-01-01T00:00:00Z/P1D                    factory  rec=1 start=DateTimeImmutable
R000000000/2020-01-01T00:00:00Z/P1D            new      DateMalformedPeriodStringException: DatePeriod::__construct(): ISO interval must contain an end date or a recurrence count, "R000000000/2020-01-01T00:00:00Z/P1D" given
R000000000/2020-01-01T00:00:00Z/P1D            factory  DateMalformedPeriodStringException: DatePeriod::createFromISO8601String(): ISO interval must contain an end date or a recurrence count, "R000000000/2020-01-01T00:00:00Z/P1D" given
R0000000001/2020-01-01T00:00:00Z/P1D           new      DateMalformedPeriodStringException: DatePeriod::__construct(): ISO interval must contain an end date or a recurrence count, "R0000000001/2020-01-01T00:00:00Z/P1D" given
R0000000001/2020-01-01T00:00:00Z/P1D           factory  DateMalformedPeriodStringException: DatePeriod::createFromISO8601String(): ISO interval must contain an end date or a recurrence count, "R0000000001/2020-01-01T00:00:00Z/P1D" given
R123456789/2020-01-01T00:00:00Z/P1D            new      rec=123456789 start=DateTime
R123456789/2020-01-01T00:00:00Z/P1D            factory  rec=123456789 start=DateTimeImmutable
R1234567890/2020-01-01T00:00:00Z/P1D           new      rec=123456789 start=DateTime
R1234567890/2020-01-01T00:00:00Z/P1D           factory  rec=123456789 start=DateTimeImmutable
R2147483639/2020-01-01T00:00:00Z/P1D           new      rec=214748363 start=DateTime
R2147483639/2020-01-01T00:00:00Z/P1D           factory  rec=214748363 start=DateTimeImmutable
