#include <stdlib.h>
#include <unistd.h>  
#include <stdio.h>
#include <limits.h>

// this stupid program was written
//   to check whether an unixoid system configuration catches out-of-memory situations gracefully

uint8_t **blocks;
int       blocksize = 256;
int       blocksmax = 0;
int       cycleduration = 1000;
int       allocncycles = 1;
int       freencycles = 0;
int       stressncycles = 0;
int       stressfraction = 1;
int       runcycles = 0;
int       contiffail = 0;
          /* counters */
int       blocksallocated = 0;
int       allowallocfails = 0;
int       stresscycles = 0;

void usage( void);
int  allocblock( void);
void freeblock( void);
void stressblocks( void);
void die( char*msg);

void usage( void)
{
  const char *name;

  if ((name = getprogname()) == NULL)
    name = "<programname>";
  fprintf(stderr, "%s: Unixoid out-of-memory situation evaluation tool\n", name);
  fprintf(stderr, "Usage: %s [-bmdafstrc]\n", name);
  fprintf(stderr, "  -b   block size in megabytes to malloc every time (default 1)\n");
  fprintf(stderr, "  -m   max block count to allocate\n");
  fprintf(stderr, "  -d   cycle duration in milliseconds\n");
  fprintf(stderr, "  -a   allocate new block every n cycles\n");
  fprintf(stderr, "  -f   free a block every n cycles\n");
  fprintf(stderr, "  -s   if specified, stress memory every nth cycle,\n");
  fprintf(stderr, "       by accessing each allocated 4KiB page of chosen blocks\n");
  fprintf(stderr, "  -t   fraction of contiguous blocks to stress (1 = all)\n");
  fprintf(stderr, "  -r   if given, run this number of cycles, otherwise run infinitely\n");
  fprintf(stderr, "  -c   continue if allocation failed\n");
/*  fprintf(stderr, "  -S   instead of conventional memory, allocate SYSV shared memory\n"); */
}

int allocblock( void)
{
  uint8_t **blockpp = blocks;
  
  // find a free slot
  for ( int n = 0; n < blocksmax; ++n) {
    if (*blockpp == NULL) {
      // fill free slot
      *blockpp = malloc( blocksize << 12);
      if (*blockpp == NULL)
        return 1;
      else {
        ++blocksallocated;
        return 0;
    } }
    ++blockpp;
  }
  // if it ever reaches here, the programmer is a proven retard
  die ("??? allocblock()");
  return 1;     // dummy return to avoid warning
}

void freeblock( void)
{
  uint8_t **blockpp = blocks;
  int nth = rand() % blocksallocated;
  
  // free nth allocated block
  for ( int n = 0; n < blocksmax; ++n)
    if (*blockpp != NULL) {
      if (!nth) {
        free( *blockpp);
        *blockpp = NULL;
        --blocksallocated;
        return;
      } else
        --nth;
    }
    ++blockpp;
  // if it ever reaches here, the programmer is a proven retard
  die ("??? freeblock()");
}

void stressblocks( void)
{
  uint8_t **blockpp;
  int startat = rand() % blocksallocated;
  int num = rand() % (blocksallocated / stressfraction);
  int n, ind, page;
  
  // walk through num blocks beginning at block startat, if necessary wrap around
  printf( "\nGoing to stress %d blocks\n", num);
  ind = startat + num;
  for (n = 0; n < num; ++n) {
    if (ind >= blocksallocated)
      ind -= blocksallocated;
    // find the next allicated block
    blockpp = blocks + ind;
    while (*blockpp == NULL) {
      // fast forward for next non-empty block
      ++ind;
      if (ind >= blocksallocated)
        ind = 0;
      blockpp = blocks + ind;
    }
    printf( "Stressing block %d of %d\n", ind, blocksallocated);
    // now *blockpp points to our block, stress it  
    for ( page = 0; page < blocksize; ++page) {
      // touch every 4KiB page
      ++*(*blockpp + (page << 12));
    }
    ++ind;
} } 

void die( char*msg)
{
  printf( "%s\n", msg);
  exit( 1);
}

int main(int argc, char *argv[])
{
  char        invc = '\0';
  char       *inv = &invc;
  int         o, cycle = 1;
  
  if (argc == 1) {
    usage();
    exit( 0);
  }
  while ((o = getopt( argc, argv, "b:m:d:a:f:s:t:r:c")) != -1) {
    switch (o) {
      case 'b': blocksize = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "blocksize arg error");
                blocksize *= 256;
                break;
      case 'm': blocksmax = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "blocksmax arg error");
                break;
      case 'd': cycleduration = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "cycleduration arg error");
                break;
      case 'a': allocncycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "allocncycles arg error");
                break;
      case 'f': freencycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "freencycles arg error");
                break;
      case 's': stressncycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "stressncycles arg error");
                break;
      case 't': stressfraction = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "stressfraction arg error");
                break;
      case 'r': runcycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "runcycles arg error");
                break;
      case 'c': allowallocfails = 1;
                break;
      default : die( "unknown arg error");
                break;
  } }

  if ( (blocks = calloc( blocksmax, sizeof( uint8_t *))) == NULL )
    die( "Couldn't allocate index array");
  for ( ; !runcycles || runcycles > cycle; ++cycle) {
    printf( "Cycle #%d\n", cycle);
    if (!(cycle % allocncycles)) {
      printf( "Time to gobble up some memory...  ");
      if (blocksallocated < blocksmax) {
        int r = allocblock();
        if (r && !allowallocfails)
          die( "Allocation failed!");
        if (r)
          printf( "Allocated %d of %d blocks FAILED!\n", blocksallocated, blocksmax);
        else
          printf( "Allocated %d of %d blocks\n", blocksallocated, blocksmax);
      } else {
        printf( "but got already all %d blocks allowed.\n", blocksmax);
    } }
    if (stressncycles) {
      if (!(cycle % stressncycles)) {
        printf( "Time to stress some memory...  ");
        stressblocks();
        printf( "Stressed!\n");
    } }
    if (blocksallocated && freencycles) {
      if (!(cycle % freencycles)) {
        printf( "Time to free some memory...  ");
        freeblock();
        printf( "Freed one of %d allocated blocks\n", blocksallocated);
    } }
    usleep( 1000 * cycleduration);
  }
  return 0;
}
       