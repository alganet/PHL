/*
 * The time instrument, as an LD_PRELOAD shared object.
 *
 * `perf` and `gdb -p` are both blocked on this box, and gprof's time column
 * lies about an -O3 build (its call COUNTS are still worth having). So: an
 * ITIMER_PROF timer, a SIGPROF handler, and a fixed table the handler folds
 * each backtrace into. It profiles the ordinary release binary -- nothing is
 * recompiled, so the thing measured is the thing that ships.
 *
 * Everything the handler touches is preallocated. The signal lands inside the
 * allocator constantly, so a handler that allocated would deadlock within
 * seconds, and one that touched stdio would corrupt the very output it is
 * writing. backtrace() is the one call in here that is not formally
 * async-signal-safe: its first invocation dlopen()s libgcc's unwinder. The
 * constructor makes that first call, so by the time a signal can arrive there
 * is nothing left to load.
 *
 * Addresses are recorded raw and resolved at exit, RELATIVE to the module they
 * fell in (dladdr gives both), because a PIE and its libraries all move.
 *
 * Build and run: build-aux/sample.sh <script.php> [args...]
 * Env: PHL_SAMPLE_US     tick in microseconds of CPU time (default 1000)
 *      PHL_SAMPLE_DEPTH  frames kept per sample, 1..8 (default 4)
 *      PHL_SAMPLE_OUT    raw output path (default phl-sample.out)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#define SAMPLER_MAXD  8
#define SAMPLER_SLOTS (1u << 18)   /* 262144 distinct stacks; ~19 MB */
/* backtrace() from inside the handler reports the handler itself and the
 * kernel's signal trampoline before the interrupted frame. */
#define SAMPLER_SKIP  2

typedef struct sampler_row sampler_row;
struct sampler_row {
	void *aPc[SAMPLER_MAXD];
	unsigned long nCount;
	int nDepth;
};

static sampler_row *aRow;
static unsigned long nSample, nDropped;
static int nDepth = 4;
static pid_t nPid;
static const char *zOut;

static unsigned int SamplerHash(void *const *aPc,int n)
{
	unsigned long long h = 1469598103934665603ULL;
	int i;
	for( i = 0 ; i < n ; ++i ){
		h ^= (unsigned long long)(unsigned long)aPc[i];
		h *= 1099511628211ULL;
	}
	h ^= h >> 31;
	return (unsigned int)h;
}
static void SamplerTick(int nSig)
{
	void *aRaw[SAMPLER_MAXD + SAMPLER_SKIP];
	sampler_row *pRow;
	unsigned int h;
	int n,d,i,k;

	(void)nSig;
	n = backtrace(aRaw,nDepth + SAMPLER_SKIP);
	d = n - SAMPLER_SKIP;
	if( d < 1 ){
		nDropped++;
		return;
	}
	h = SamplerHash(&aRaw[SAMPLER_SKIP],d) & (SAMPLER_SLOTS - 1);
	for( k = 0 ; k < 64 ; ++k ){
		pRow = &aRow[h];
		if( pRow->nCount == 0 ){
			for( i = 0 ; i < d ; ++i ){ pRow->aPc[i] = aRaw[SAMPLER_SKIP + i]; }
			pRow->nDepth = d;
			pRow->nCount = 1;
			nSample++;
			return;
		}
		if( pRow->nDepth == d ){
			for( i = 0 ; i < d ; ++i ){
				if( pRow->aPc[i] != aRaw[SAMPLER_SKIP + i] ){ break; }
			}
			if( i == d ){
				pRow->nCount++;
				nSample++;
				return;
			}
		}
		h = (h + 1) & (SAMPLER_SLOTS - 1);
	}
	/* Sixty-four probes deep: count it as lost rather than walk the table
	 * inside a signal handler. The dump reports how many. */
	nDropped++;
}
/* Modules are named once and referred to by index; a full path per frame would
 * be most of the output file. */
#define SAMPLER_MAXMOD 64
static const char *azMod[SAMPLER_MAXMOD];
static void *apModBase[SAMPLER_MAXMOD];
static int nMod;

static int SamplerModule(void *pPc,unsigned long *pOfft)
{
	Dl_info sInfo;
	int i;
	if( dladdr(pPc,&sInfo) == 0 || sInfo.dli_fname == 0 ){
		*pOfft = (unsigned long)pPc;
		return -1;
	}
	for( i = 0 ; i < nMod ; ++i ){
		if( apModBase[i] == sInfo.dli_fbase ){
			*pOfft = (unsigned long)((char *)pPc - (char *)sInfo.dli_fbase);
			return i;
		}
	}
	if( nMod == SAMPLER_MAXMOD ){
		*pOfft = (unsigned long)pPc;
		return -1;
	}
	azMod[nMod] = sInfo.dli_fname;
	apModBase[nMod] = sInfo.dli_fbase;
	*pOfft = (unsigned long)((char *)pPc - (char *)sInfo.dli_fbase);
	return nMod++;
}
static void SamplerDump(void) __attribute__((destructor));
static void SamplerDump(void)
{
	FILE *pOut;
	unsigned int i;
	int j;
	if( aRow == 0 || getpid() != nPid ){
		/* A forked child inherits neither the timer nor the right to write
		 * over its parent's output. */
		return;
	}
	setitimer(ITIMER_PROF,0,0);
	pOut = fopen(zOut,"w");
	if( pOut == 0 ){
		return;
	}
	/* Name every module first: the STACK lines below refer to them by index. */
	for( i = 0 ; i < SAMPLER_SLOTS ; ++i ){
		for( j = 0 ; j < aRow[i].nDepth ; ++j ){
			unsigned long offt;
			(void)SamplerModule(aRow[i].aPc[j],&offt);
		}
	}
	fprintf(pOut,"# phl sampler  samples %lu  dropped %lu  depth %d\n",
		nSample,nDropped,nDepth);
	for( j = 0 ; j < nMod ; ++j ){
		fprintf(pOut,"MODULE %d %s\n",j,azMod[j]);
	}
	for( i = 0 ; i < SAMPLER_SLOTS ; ++i ){
		if( aRow[i].nCount == 0 ){
			continue;
		}
		fprintf(pOut,"STACK %lu",aRow[i].nCount);
		for( j = 0 ; j < aRow[i].nDepth ; ++j ){
			unsigned long offt;
			int m = SamplerModule(aRow[i].aPc[j],&offt);
			fprintf(pOut," %d:%lx",m,offt);
		}
		fprintf(pOut,"\n");
	}
	fclose(pOut);
	fprintf(stderr,"sampler: %lu samples (%lu dropped) -> %s\n",nSample,nDropped,zOut);
}
static void SamplerStart(void) __attribute__((constructor));
static void SamplerStart(void)
{
	struct sigaction sa;
	struct itimerval it;
	void *aWarm[SAMPLER_MAXD];
	const char *z;
	long nUs = 1000;
	void *pMap;

	nPid = getpid();
	zOut = getenv("PHL_SAMPLE_OUT");
	if( zOut == 0 ){ zOut = "phl-sample.out"; }
	z = getenv("PHL_SAMPLE_US");
	if( z ){
		nUs = strtol(z,0,0);
		if( nUs < 100 ){ nUs = 100; }
	}
	z = getenv("PHL_SAMPLE_DEPTH");
	if( z ){
		nDepth = (int)strtol(z,0,0);
		if( nDepth < 1 ){ nDepth = 1; }
		if( nDepth > SAMPLER_MAXD ){ nDepth = SAMPLER_MAXD; }
	}
	pMap = mmap(0,(size_t)SAMPLER_SLOTS * sizeof(sampler_row),
		PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
	if( pMap == MAP_FAILED ){
		fprintf(stderr,"sampler: cannot map its table -- disabled\n");
		return;
	}
	aRow = (sampler_row *)pMap;
	/* Force libgcc's unwinder to load NOW. The first backtrace() dlopen()s it,
	 * and a dlopen from inside a signal handler that landed in malloc is a
	 * deadlock that takes minutes to reproduce and hours to believe. */
	(void)backtrace(aWarm,SAMPLER_MAXD);
	memset(&sa,0,sizeof(sa));
	sa.sa_handler = SamplerTick;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	if( sigaction(SIGPROF,&sa,0) != 0 ){
		fprintf(stderr,"sampler: cannot install SIGPROF -- disabled\n");
		aRow = 0;
		return;
	}
	it.it_interval.tv_sec  = nUs / 1000000;
	it.it_interval.tv_usec = nUs % 1000000;
	it.it_value = it.it_interval;
	setitimer(ITIMER_PROF,&it,0);
}
