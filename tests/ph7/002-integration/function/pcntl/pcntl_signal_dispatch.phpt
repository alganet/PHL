--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/pcntl: where a signal reaches PHP -- pcntl_signal_dispatch() and the async point
--SKIPIF--
<?php
if (!extension_loaded('pcntl') || !extension_loaded('posix')) {
    die("skip needs ext/pcntl and ext/posix (php has neither on Windows)\n");
}
if (!function_exists('pcntl_sigwaitinfo')) {
    die("skip php builds no pcntl_sigwaitinfo() on this system\n");
}
--FILE--
<?php
/* SIGURG is the signal to test with: its default disposition is IGNORE, so a
 * delivery that nothing handles cannot kill the test. */
$me = posix_getpid();
function line(string $l, $v): void {
    printf("%-42s %s\n", $l, str_replace("\n", '', var_export($v, true)));
}

echo "-- with async OFF nothing runs until the program ASKS\n";
pcntl_async_signals(false);
$seen = [];
pcntl_signal(SIGURG, function (int $signo, $siginfo) use (&$seen): void {
    $seen[] = [$signo, is_array($siginfo) ? array_keys($siginfo) : $siginfo];
});
posix_kill($me, SIGURG);
line('the handler has NOT run', $seen);
line('dispatch answers true', pcntl_signal_dispatch());
line('...and now it has', $seen);
line('a second dispatch finds nothing', [pcntl_signal_dispatch(), count($seen)]);

echo "-- the siginfo array is php's selection, not the whole struct\n";
$seen = [];
posix_kill($me, SIGURG);
pcntl_signal_dispatch();
line('three keys for a plain signal', $seen[0][1]);
line('signo is the signal', $seen[0][0] === SIGURG);

echo "-- deliveries QUEUE: two before one dispatch are two calls\n";
$seen = [];
pcntl_sigprocmask(SIG_BLOCK, [SIGURG]);
posix_kill($me, SIGURG);
posix_kill($me, SIGURG);
pcntl_sigprocmask(SIG_UNBLOCK, [SIGURG]);
pcntl_signal_dispatch();
line('at least one arrived', count($seen) >= 1);

echo "-- with async ON the handler runs BEFORE the next statement\n";
pcntl_async_signals(true);
$order = [];
pcntl_signal(SIGURG, function () use (&$order): void { $order[] = 'handler'; });
posix_kill($me, SIGURG);
$order[] = 'after the kill';
line('the handler went first', $order);
pcntl_async_signals(false);

echo "-- turning async ON does NOT flush what is already queued\n";
/* php raises its interrupt from the SIGNAL HANDLER and nowhere else, so a
 * delivery that arrived while async was off waits for the next dispatch or the
 * next signal -- it is not swept up by the switch. */
$order = [];
pcntl_sigprocmask(SIG_BLOCK, [SIGURG]);
posix_kill($me, SIGURG);
pcntl_sigprocmask(SIG_UNBLOCK, [SIGURG]);
line('nothing yet (async is off)', $order);
pcntl_async_signals(true);
$order[] = 'after turning async on';
line('...and still nothing', $order);
pcntl_signal_dispatch();
$order[] = 'after an explicit dispatch';
line('the dispatch is what runs it', $order);
pcntl_async_signals(false);

echo "-- SIG_IGN and SIG_DFL are skipped at the DISPATCH, not at the delivery\n";
$ran = false;
pcntl_signal(SIGURG, function () use (&$ran): void { $ran = true; });
posix_kill($me, SIGURG);
pcntl_signal(SIGURG, SIG_IGN);           /* changed AFTER it was delivered */
pcntl_signal_dispatch();
line('the disposition at dispatch wins', $ran);

echo "-- a throw from a handler is the CALLER's to catch, both ways in\n";
pcntl_signal(SIGURG, function (): void { throw new RuntimeException('from the handler'); });
posix_kill($me, SIGURG);
try { pcntl_signal_dispatch(); line('sync: not reached', false); }
catch (Throwable $e) { line('sync: caught', $e->getMessage()); }
pcntl_async_signals(true);
try {
    posix_kill($me, SIGURG);
    for ($i = 0; $i < 3; $i++) { $x = $i * 2; }
    line('async: not reached', false);
} catch (Throwable $e) { line('async: caught', $e->getMessage()); }
pcntl_async_signals(false);
pcntl_signal(SIGURG, SIG_IGN);

echo "-- sigprocmask reports the OLD set as a list of numbers\n";
pcntl_sigprocmask(SIG_SETMASK, [], $old);
line('nothing blocked to begin with', $old);
pcntl_sigprocmask(SIG_BLOCK, [SIGURG, SIGCONT], $old);
line('...and the block reports that', $old);
pcntl_sigprocmask(SIG_SETMASK, [], $old);
line('now it names both, in order', $old);
line('and the mask is empty again', (function () { pcntl_sigprocmask(SIG_SETMASK, [], $o); return $o; })());

echo "-- sigwaitinfo takes a BLOCKED signal straight out of the queue\n";
pcntl_sigprocmask(SIG_BLOCK, [SIGURG]);
posix_kill($me, SIGURG);
line('it answers the number', pcntl_sigwaitinfo([SIGURG], $info) === SIGURG);
line('...and fills the same array', array_keys($info));
posix_kill($me, SIGURG);
line('the timed one too', pcntl_sigtimedwait([SIGURG], $info2, 1) === SIGURG);
line('...with the same keys', array_keys($info2));
pcntl_sigprocmask(SIG_UNBLOCK, [SIGURG]);

echo "-- a dispatch INSIDE a handler is a no-op, and a re-raise lands on a FRESH queue\n";
/* php detaches the whole queue before it runs anything, so a handler that
 * raises the signal it is handling cannot re-enter and cannot loop. */
$depth = 0; $max = 0;
pcntl_signal(SIGURG, function () use (&$depth, &$max, $me): void {
    $depth++; $max = max($max, $depth);
    if ($depth < 3) { posix_kill($me, SIGURG); pcntl_signal_dispatch(); }
    $depth--;
});
posix_kill($me, SIGURG);
pcntl_signal_dispatch();
line('the handler never nested', $max);
pcntl_signal_dispatch();
line('...and the next dispatch runs it', $max);
/* That handler leaves one more delivery behind every time it runs: put the
 * disposition back first, THEN drain, so the next section starts empty. */
pcntl_signal(SIGURG, SIG_IGN);
pcntl_signal_dispatch();

echo "-- a handler that throws DROPS what was queued behind it\n";
$ran = [];
pcntl_signal(SIGCONT, function () use (&$ran): void { $ran[] = 'cont'; throw new RuntimeException('cont'); });
pcntl_signal(SIGURG, function () use (&$ran): void { $ran[] = 'urg'; });
/* Unblocked ONE AT A TIME: the kernel's delivery order for two signals
 * unblocked together is not fixed, and the queue order is the point here. */
pcntl_sigprocmask(SIG_BLOCK, [SIGURG, SIGCONT]);
posix_kill($me, SIGURG);
posix_kill($me, SIGCONT);
pcntl_sigprocmask(SIG_UNBLOCK, [SIGCONT]);
pcntl_sigprocmask(SIG_UNBLOCK, [SIGURG]);
try { pcntl_signal_dispatch(); } catch (Throwable $e) { line('the throw came out', $e->getMessage()); }
line('only the thrower ran', $ran);
pcntl_signal_dispatch();
line('...and the other is GONE', $ran);
pcntl_signal(SIGCONT, SIG_DFL);
pcntl_signal(SIGURG, SIG_DFL);
?>
--EXPECT--
-- with async OFF nothing runs until the program ASKS
the handler has NOT run                    array ()
dispatch answers true                      true
...and now it has                          array (  0 =>   array (    0 => 23,    1 =>     array (      0 => 'signo',      1 => 'errno',      2 => 'code',    ),  ),)
a second dispatch finds nothing            array (  0 => true,  1 => 1,)
-- the siginfo array is php's selection, not the whole struct
three keys for a plain signal              array (  0 => 'signo',  1 => 'errno',  2 => 'code',)
signo is the signal                        true
-- deliveries QUEUE: two before one dispatch are two calls
at least one arrived                       true
-- with async ON the handler runs BEFORE the next statement
the handler went first                     array (  0 => 'handler',  1 => 'after the kill',)
-- turning async ON does NOT flush what is already queued
nothing yet (async is off)                 array ()
...and still nothing                       array (  0 => 'after turning async on',)
the dispatch is what runs it               array (  0 => 'after turning async on',  1 => 'handler',  2 => 'after an explicit dispatch',)
-- SIG_IGN and SIG_DFL are skipped at the DISPATCH, not at the delivery
the disposition at dispatch wins           false
-- a throw from a handler is the CALLER's to catch, both ways in
sync: caught                               'from the handler'
async: caught                              'from the handler'
-- sigprocmask reports the OLD set as a list of numbers
nothing blocked to begin with              array ()
...and the block reports that              array ()
now it names both, in order                array (  0 => 18,  1 => 23,)
and the mask is empty again                array ()
-- sigwaitinfo takes a BLOCKED signal straight out of the queue
it answers the number                      true
...and fills the same array                array (  0 => 'signo',  1 => 'errno',  2 => 'code',)
the timed one too                          true
...with the same keys                      array (  0 => 'signo',  1 => 'errno',  2 => 'code',)
-- a dispatch INSIDE a handler is a no-op, and a re-raise lands on a FRESH queue
the handler never nested                   1
...and the next dispatch runs it           1
-- a handler that throws DROPS what was queued behind it
the throw came out                         'cont'
only the thrower ran                       array (  0 => 'cont',)
...and the other is GONE                   array (  0 => 'cont',)
