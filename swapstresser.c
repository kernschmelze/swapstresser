#include <stdlib.h>
#include <unistd.h>  
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

// Swapstresser by (c) Stefan Blachmann 2018
// to check whether an unixoid system configuration catches out-of-memory situations gracefully

int       cycleduration = 1000;
int       runcycles = 0;
int       allowallocfails = 0;

int       blocksize = 256;
int       blocksize_shm = 256;
int       blocksmax = 0;
int       blocksmax_shm = 0;
int       allocncycles = 1;
int       allocncycles_shm = 1;
int       freencycles = 0;
int       freencycles_shm = 0;
int       stressncycles = 0;
int       stressncycles_shm = 0;
int       stressfraction = 1;
int       stressfraction_shm = 1;
          /* counters */
int       blocksallocated = 0;
int       blocksallocated_shm = 0;

uint8_t **blocks;
int      *segments_shm;

void usage( void);
int  allocblock( void);
int  allocblock_shm( void);
void freeblock( void);
void freeblock_shm( void);
void stressblocks( void);
void stressblocks_shm( void);
void die( char*msg);

void usage( void)
{
  const char *name;

  if ((name = getprogname()) == NULL)
    name = "<programname>";
  fprintf(stderr, "%s: Unixoid out-of-memory situation evaluation tool\n", name);
  fprintf(stderr, "Usage: %s [-bmdafstrc]\n", name);
  fprintf(stderr, "  -d   cycle duration in milliseconds (default=1sec)\n");
  fprintf(stderr, "  -c   continue if allocation failed\n");
  fprintf(stderr, "  -r   if given, run this number of cycles, otherwise run infinitely\n");
  fprintf(stderr, "  The following options can be given in uppercase also.\n");
  fprintf(stderr, "  Uppercase uses SHM instead of malloc-based allocation.\n");
  fprintf(stderr, "  -b   block size in megabytes to malloc every time (default 1)\n");
  fprintf(stderr, "  -m   max block count to allocate\n");
  fprintf(stderr, "  -a   allocate new block every n cycles\n");
  fprintf(stderr, "  -f   free a block every n cycles\n");
  fprintf(stderr, "  -s   if specified, stress memory every nth cycle,\n");
  fprintf(stderr, "       by accessing each allocated 4KiB page of chosen blocks\n");
  fprintf(stderr, "  -t   fraction of contiguous blocks to stress (1 = all)\n");
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
      } 
    }
    ++blockpp;
  }
  // NOTREACHED
  die ("??? allocblock()");  // NOTREACHED
  return 1;     // dummy return to avoid warning
}

int allocblock_shm( void)
{
  int r, *seg_shm = segments_shm;
  
  // find a free slot
  for ( int n = 0; n < blocksmax_shm; ++n) {
    if (*seg_shm == 0) {
      // fill free slot
      r = shmget( IPC_PRIVATE, blocksize_shm << 12, S_IRUSR | S_IWUSR | IPC_CREAT);
      if (r == -1) 
        return 1;
      *seg_shm = r;
      ++blocksallocated_shm;
      return 0;
    }
    ++seg_shm;
  }
  // NOTREACHED
  die ("??? allocblock_shm()");
  return 1;     // dummy return to avoid warning
}

void freeblock( void)
{
  uint8_t **blockpp = blocks;
  int       nth = rand() % blocksallocated;
  
  // free nth allocated block
  for ( int n = 0; n < blocksmax; ++n) {
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
  }
  // NOTREACHED
  die ("??? freeblock()");
}

void freeblock_shm( void)
{
  int *seg_shm = segments_shm;
  int  nth = rand() % blocksallocated_shm;
  
  // free nth allocated segment
  for ( int n = 0; n < blocksmax_shm; ++n) {
    if (*seg_shm != 0) {
      if (!nth) {
        shmctl (*seg_shm, IPC_RMID, NULL);
        --blocksallocated_shm;
        return;
      } else
        --nth;
    }
    ++seg_shm;
  }
  // NOTREACHED
  die ("??? freeblock_shm()");
}

void stressblocks( void)
{
  uint8_t **blockpp;
  if (blocksallocated == 0) {
    /* skip if not yet allocated */  
    printf( "Stressing skipped, nothing allocated yet\n");
    return;
  }
  int startat = rand() % blocksallocated;
  int num = 1;
  int n, ind, page;
  
  // walk through num blocks beginning at block startat, if necessary wrap around
  printf( "\nGoing to stress %d blocks\n", num);
  ind = startat + num;
  for (n = 0; n < num; ++n) {
    if (ind >= blocksallocated)
      ind -= blocksallocated;
    // find the next allocated block
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
  } 
} 

void stressblocks_shm( void)
{
  int *seg_shm;
  uint8_t *memp;
  if (blocksallocated_shm == 0) {
    /* skip if not yet allocated */  
    printf( "SHM stressing skipped, nothing allocated yet\n");
    return;
  }
  int startat = rand() % blocksallocated_shm;
  int num = rand() % (blocksallocated_shm / stressfraction_shm);
  int n, ind, page;
  
  // walk through num blocks beginning at block startat, if necessary wrap around
  printf( "\nGoing to stress %d segments\n", num);
  ind = startat + num;
  for (n = 0; n < num; ++n) {
    if (ind >= blocksallocated_shm)
      ind -= blocksallocated_shm;
    // find the next allocated block
    seg_shm = segments_shm + ind;
    while (*seg_shm == 0) {
      // fast forward for next non-empty block
      ++ind;
      if (ind >= blocksallocated_shm)
        ind = 0;
      seg_shm = segments_shm + ind;
    }
    printf( "Stressing segment %d of %d\n", ind, blocksallocated_shm);
    // attach segment
    memp = (uint8_t *) shmat (*seg_shm, NULL, 0);
    if (((int) memp) == -1)
      die( "shmat() failed!");
    // now *blockpp points to our block, stress it  
    for ( page = 0; page < blocksize; ++page) {
      // touch every 4KiB page
      ++*(memp + (page << 12));
    }
    int r = shmdt( memp);
    if (r)
      die( "shmdt() failed!");
    ++ind;
  }
} 

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
  while ((o = getopt( argc, argv, "d:r:cb:m:a:f:s:t:B:M:A:F:S:T:")) != -1) {
    switch (o) {
      case 'd': cycleduration = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "cycleduration arg error");
                break;
      case 'r': runcycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "runcycles arg error");
                break;
      case 'c': allowallocfails = 1;
                break;
      case 'B': blocksize_shm = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "blocksize_shm arg error");
                blocksize_shm *= 256;
                break;
      case 'b': blocksize = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "blocksize arg error");
                blocksize *= 256;
                break;
      case 'm': blocksmax = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "blocksmax arg error");
                break;
      case 'M': blocksmax_shm = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "blocksmax_shm arg error");
                break;
      case 'a': allocncycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "allocncycles arg error");
                break;
      case 'A': allocncycles_shm = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "allocncycles_shm arg error");
                break;
      case 'f': freencycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "freencycles arg error");
                break;
      case 'F': freencycles_shm = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "freencycle_shms arg error");
                break;
      case 's': stressncycles = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "stressncycles arg error");
                break;
      case 'S': stressncycles_shm = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "stressncycles_shm arg error");
                break;
      case 't': stressfraction = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "stressfraction arg error");
                break;
      case 'T': stressfraction_shm = strtol( optarg, &inv, 10);
                if (*inv != '\0')
                  die( "stressfraction_shm arg error");
                break;
      default : die( "unknown arg error");
                break;
  } }


  // init the lookup arrays
  if (blocksmax) {
    if ( (blocks = calloc( blocksmax, sizeof( uint8_t *))) == NULL )
      die( "Couldn't allocate index array");
  }
  if (blocksmax_shm) {
    if ( (segments_shm = calloc( blocksmax_shm, sizeof( int))) == NULL )
      die( "Couldn't allocate shm segments array");
  }
  
  for ( ; !runcycles || runcycles > cycle; ++cycle) {
    printf( "Cycle #%d\n", cycle);
    if (blocksmax) {
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
    } } }
    if (blocksmax_shm) {
      if (!(cycle % allocncycles_shm)) {
        printf( "Time to gobble up some SHM memory...  ");
        if (blocksallocated_shm < blocksmax_shm) {
          int r = allocblock_shm();
          if (r && !allowallocfails)
            die( "Allocation failed!");
          if (r)
            printf( "Allocated %d of %d blocks FAILED!\n", blocksallocated_shm, blocksmax_shm);
          else
            printf( "Allocated %d of %d blocks\n", blocksallocated_shm, blocksmax_shm);
        } else {
          printf( "but got already all %d blocks allowed.\n", blocksmax_shm);
      } } 
      if (stressncycles_shm) {
        if (!(cycle % stressncycles_shm)) {
          printf( "Time to stress some SHM memory...  ");
          stressblocks_shm();
          printf( "Stressed!\n");
      } }
      if (blocksallocated_shm && freencycles_shm) {
        if (!(cycle % freencycles_shm)) {
          printf( "Time to free some SHM memory...  ");
          freeblock_shm();
          printf( "Freed one of %d allocated blocks\n", blocksallocated_shm);
    } } }
    usleep( 1000 * cycleduration);
  }
  return 0;
}
       
