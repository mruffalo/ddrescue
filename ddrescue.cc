/*  ddrescue - A data recovery tool
    Copyright (C) 2004 Antonio Diaz Diaz.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
/*
    Return values: 0 for a normal exit, 1 for environmental problems
    (file not found, invalid flags, I/O errors, etc), 2 to indicate a
    corrupt or invalid input file, 3 for an internal consistency error
    (eg, bug) which caused ddrescue to panic.
*/

#define _FILE_OFFSET_BITS 64

#include <queue>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>


// Date of this version: 2004-09-28

const char * const program_version = "0.6";
const char * const program_year    = "2004";

bool volatile interrupted = false;		// user pressed Ctrl-C
void sighandler( int ) throw() { interrupted = true; }

struct Block
  {
  off_t ipos, opos;
  size_t size;
  Block() : ipos( 0 ), opos( 0 ), size( 0 ) {}
  Block( off_t i, off_t o, size_t s ) : ipos( i ), opos( o ), size( s ) {}
  };


void more_info( const char * program_name ) throw()
  {
  std::fprintf( stderr, "Try `%s --help' for more information.\n", program_name );
  }


void show_version() throw()
  {
  std::printf( "ddrescue version %s\n", program_version );
  std::printf( "Copyright (C) %s Antonio Diaz Diaz.\n", program_year );
  std::printf( "This program is free software; you may redistribute it under the terms of\n" );
  std::printf( "the GNU General Public License.  This program has absolutely no warranty.\n" );
  }


void show_error( const char * msg, bool show_errno = false ) throw()
  {
  std::fprintf( stderr, "ddrescue: %s", msg );
  if( show_errno ) std::fprintf( stderr, ": %s", strerror( errno ) );
  std::fprintf( stderr, "\n" );
  }


void show_help( const char * program_name, int cluster, int hardbs ) throw()
  {
  std::printf( "ddrescue copies data from one file or block device to another,\n" );
  std::printf( "trying hard to rescue data in case of read errors.\n" );
  std::printf( "Usage: %s [options] infile outfile [badblocks_file]\n", program_name );
  std::printf( "Options:\n" );
  std::printf( "  -h, --help                   display this help and exit\n" );
  std::printf( "  -V, --version                output version information and exit\n" );
  std::printf( "  -b, --block-size=<bytes>     hardware block size of input device [%d]\n", hardbs );
  std::printf( "  -c, --cluster-size=<blocks>  hardware blocks to copy at a time [%d]\n", cluster );
  std::printf( "  -e, --max-errors=<n>         exit if more error areas found (-1=inf) [-1]\n" );
  std::printf( "  -i, --input-position=<pos>   start position in input file [0]\n" );
  std::printf( "  -o, --output-position=<pos>  start position in output file [ipos]\n" );
  std::printf( "  -q, --quiet                  quiet operation\n" );
  std::printf( "  -r, --max-retries=<n>        exit after given retries (-1=infinity) [0]\n" );
  std::printf( "  -s, --max-size=<bytes>       maximum size of data to be copied\n" );
  std::printf( "  -t, --truncate               truncate output file\n" );
  std::printf( "  -v, --verbose                verbose operation\n" );
  std::printf( "Numbers may be followed by a multiplier: b = blocks, c = 1, k = 2^10 = 1024,\n" );
  std::printf( "M = 2^20, G = 2^30, T = 2^40, P = 2^50, E = 2^60, Z = 2^70 or Y = 2^80.\n" );
  std::printf( "\nIf badblocks_file given, write to it the list of remaining bad blocks on exit.\n" );
  std::printf( "If badblocks_file exists, try to copy only the blocks listed in it.\n" );
  }


const char *format_num( long long n, long long max = 999999 ) throw()
  {
  static const char * const units[8] =
    { "ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  static char buf[16];
  const char *su = "";
  for( int i = 0; i < 8 && std::llabs( n ) > std::llabs( max ); ++i )
    { n /= 1024; su = units[i]; }
  std::snprintf( buf, sizeof( buf ), "%Ld %s", n, su );
  return buf;
  }


void show_status( off_t ipos, off_t opos, off_t recsize, off_t errsize,
                  size_t errors, bool force = false ) throw()
  {
  static const char* const up = "\x1b[A";
  static off_t a_rate = 0, c_rate = 0, last_size = 0;
  static time_t t1 = 0, t2 = 0;
  if( t1 == 0 )
    { t1 = t2 = std::time( 0 ); std::printf( "\n\n\n" ); force = true; }

  time_t t3 = std::time( 0 );
  if( t3 > t2 || force )
    {
    if( t3 > t2 )
      {
      a_rate = recsize / ( t3 - t1 );
      c_rate = ( recsize - last_size ) / ( t3 - t2 );
      t2 = t3; last_size = recsize;
      }
    std::printf( "\r%s%s%s", up, up, up );
    std::printf( "rescued: %10sB,", format_num( recsize ) );
    std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
    std::printf( "  current rate: %9sB/s\n", format_num( c_rate, 99999 ) );
    std::printf( "   ipos: %10sB,   errors: %7u,  ",
                 format_num( ipos ), errors );
    std::printf( "  average rate: %9sB/s\n", format_num( a_rate, 99999 ) );
    std::printf( "   opos: %10sB\n", format_num( opos ) );
    }
  }


size_t readblock( int fd, char *buf, size_t size, off_t pos ) throw()
  {
  int rest = size;
  lseek( fd, pos, SEEK_SET );
  while( rest > 0 )
    {
    errno = 0;
    int n = read( fd, buf + size - rest, rest );
    if( n > 0 ) rest -= n;
    else if( n == 0 ) break;
    else if( errno != EINTR && errno != EAGAIN ) break;
    }
  return ( rest > 0 ) ? size - rest : size;
  }


size_t writeblock( int fd, char *buf, size_t size, off_t pos ) throw()
  {
  int rest = size;
  lseek( fd, pos, SEEK_SET );
  while( rest > 0 )
    {
    errno = 0;
    int n = write( fd, buf + size - rest, rest );
    if( n > 0 ) rest -= n;
    else if( n == 0 ) break;
    else if( errno != EINTR && errno != EAGAIN ) break;
    }
  return ( rest > 0 ) ? size - rest : size;
  }


// Returns true if returned block is part of a bigger one at front()
bool split_block( Block & block, std::queue< Block > & badclusters,
                  size_t hardbs ) throw()
  {
  block = badclusters.front();
  size_t size = ( block.ipos + block.size ) % hardbs;
  if( size == 0 ) size = hardbs;
  if( size >= block.size ) { badclusters.pop(); return false; }

  size = block.size - size;
  badclusters.front().size = size;
  block.ipos += size; block.opos += size; block.size -= size;
  return true;
  }


int copybb( std::queue< Block > & badclusters, std::queue< Block > & badblocks,
            size_t hardbs, int ides, int odes, int max_retries,
            int verbosity, off_t recsize, off_t errsize ) throw();
int copyfile( std::queue< Block > & badclusters, std::queue< Block > & badblocks,
              off_t ipos, off_t opos, off_t max_size, size_t cluster,
              size_t hardbs, int ides, int odes, int max_errors,
              int max_retries, int verbosity ) throw()
  {
  const off_t end = ipos + max_size;
  off_t recsize = 0, errsize = 0;
  const size_t softbs = cluster * hardbs;
  char buf[softbs];

  // 1) Read the non-damaged part of the disk, skipping the damaged areas.
  while( !max_size || ipos < end )
    {
    size_t size = softbs;
    if( max_size && ipos + (off_t)size > end ) size = end - ipos;

    size_t rd;
    if( size > hardbs )
      {
      rd = readblock( ides, buf, hardbs, ipos );
      if( rd == hardbs )
        rd += readblock( ides, buf + rd, size - rd, ipos + rd );
      }
    else rd = readblock( ides, buf, size, ipos );
    recsize += rd;

    if( rd > 0 && writeblock( odes, buf, rd, opos ) != rd )
      { show_error( "write error", true ); return 1; }
    if( rd < size )
      {
      if( rd == 0 && !errno ) break;		// EOF
      else					// Read error
        {
        badclusters.push( Block( ipos + rd, opos + rd, size - rd ) );
        errsize += ( size - rd );
        if( max_errors >= 0 && (int)badclusters.size() > max_errors )
          { show_error( "too many errors in input file" ); return 2; }
        }
      }
    ipos += size; opos += size;
    if( verbosity >= 0 )
      show_status( ipos, opos, recsize, errsize, badclusters.size() );
    }
  if( verbosity >= 0 )
    show_status( ipos, opos, recsize, errsize, badclusters.size(), true );

  if( badclusters.size() == 0 ) return 0;
  return copybb( badclusters, badblocks, hardbs, ides, odes, max_retries,
                 verbosity, recsize, errsize );
  }


int copybb( std::queue< Block > & badclusters, std::queue< Block > & badblocks,
            size_t hardbs, int ides, int odes, int max_retries,
            int verbosity, off_t recsize, off_t errsize ) throw()
  {
  const char * const ccmsg = "Press Ctrl-C to interrupt";
  Block block;
  char buf[hardbs];
  bool split = false;

  // 2) Try to read the damaged areas, splitting them into smaller pieces
  //    and reading the non-damaged pieces, until the hardware block size
  //    is reached, or until interrupted by the user.
  interrupted = false;
  signal( SIGINT, sighandler );
  while( !interrupted && badclusters.size() )
    {
    if( split_block( block, badclusters, hardbs ) ) split = true;
    else { badblocks.push( block ); continue; }
    size_t rd = readblock( ides, buf, block.size, block.ipos );
    recsize += rd;
    errsize -= rd;

    if( rd > 0 && writeblock( odes, buf, rd, block.opos ) != rd )
      { show_error( "write error", true ); return 1; }
    if( rd < block.size && ( rd > 0 || errno ) )	// read_error
      {
      badblocks.push( Block( block.ipos+rd, block.opos+rd, block.size-rd ) );
      badclusters.push( badclusters.front() ); badclusters.pop();
      }
    else errsize -= ( block.size - rd );
    if( verbosity >= 0 )
      {
      show_status( block.ipos, block.opos, recsize, errsize,
                   badclusters.size() + badblocks.size() );
      std::printf( "\rSplitting error areas. %s\r", ccmsg );
      std::fflush( stdout );
      }
    }

  // 3) Try to read the damaged hardware blocks until the specified number
  //    of retries is reached, or until interrupted by the user.
  int retries = 0, counter = badblocks.size();
  if( !split && max_retries == 0 ) max_retries = 1;
  while( !interrupted && badblocks.size() && retries != max_retries )
    {
    block = badblocks.front(); badblocks.pop();
    size_t rd = readblock( ides, buf, block.size, block.ipos );
    recsize += rd;
    errsize -= rd;

    if( rd > 0 && writeblock( odes, buf, rd, block.opos ) != rd )
      { show_error( "write error", true ); return 1; }
    if( rd < block.size && ( rd > 0 || errno ) )	// read_error
      badblocks.push( Block( block.ipos+rd, block.opos+rd, block.size-rd ) );
    else errsize -= ( block.size - rd );
    if( verbosity >= 0 )
      {
      show_status( block.ipos, block.opos, recsize, errsize, badblocks.size() );
      if( !split && max_retries == 1 )
        std::printf( "\rCopying bad blocks. %s    \r", ccmsg );
      else std::printf( "\rRetry %d  %s              \r", retries + 1, ccmsg );
      std::fflush( stdout );
      }
    if( max_retries > 0 && --counter <= 0 )
      { ++retries; counter = badblocks.size(); }
    }

  if( verbosity >= 0 )
    {
    show_status( block.ipos, block.opos, recsize, errsize,
                 badclusters.size() + badblocks.size(), true );
    std::printf( "\r\n" );
    if( interrupted ) std::printf( "Interrupted by user\n" );
    std::fflush( stdout );
    }
  return 0;
  }


long long getnum( const char* const ptr, size_t bs,
                  long long min = LONG_LONG_MIN + 1,
                  long long max = LONG_LONG_MAX ) throw()
  {
  errno = 0;
  char *tail;
  long long result = std::strtoll( ptr, &tail, 0 );
  if( tail == ptr )
    { show_error( "bad or missing numerical argument" ); std::exit(1); }

  if( !errno )
    {
    int n = 0;
    switch( *tail )
      {
      case 'c':
      case ' ':
      case  0 : break;
      case 'b': if( bs && LONG_LONG_MAX / bs < std::llabs( result ) ) errno = ERANGE;
                else result *= bs;
                break;
      case 'Y': ++n;
      case 'Z': ++n;
      case 'E': ++n;
      case 'P': ++n;
      case 'T': ++n;
      case 'G': ++n;
      case 'M': ++n;
      case 'k': for( ++n; n > 0 && !errno; --n )
                  { if( LONG_LONG_MAX / 1024 < std::llabs( result ) ) errno = ERANGE;
                  else result *= 1024; }
                break;
      default: show_error( "bad suffix in numerical argument" ); std::exit(1);
      }
    }
  if( !errno && ( result < min || result > max ) ) errno = ERANGE;
  if( errno )
    { show_error( "numerical argument out of limits" ); std::exit(1); }
  return result;
  }


bool check_identical( const char * name1, const char * name2 ) throw()
  {
  if( std::strcmp( name1, name2 ) == 0 ) return true;
  struct stat stat1, stat2;
  if( stat( name1, &stat1) || stat( name2, &stat2) ) return false;
  return ( stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev );
  }


bool read_badblocks_file( const char *bbname, std::queue< Block > & badclusters,
                          off_t & errsize, int max_errors ) throw()
  {
  if( !bbname ) return false;
  FILE *f = std::fopen( bbname, "r" );
  if( !f ) return false;
  errsize = 0;
  while( max_errors < 0 || (int)badclusters.size() <= max_errors )
    {
    long long ipos, opos;
    int size;
    int n = std::fscanf( f, "%Li %Li %i\n", &ipos, &opos, &size );
    if( n < 0 ) break;	// EOF
    if( n != 3 || ipos < 0 || opos < 0 || size <= 0 )
      {
      char buf[80];
      std::snprintf( buf, sizeof( buf ), "error in file %s, line %d\n",
                     bbname, badclusters.size() + 1 );
      show_error( buf ); std::exit(1);
      }
    badclusters.push( Block( ipos, opos, size ) );
    errsize += size;
    }
  std::fclose( f );
  if( max_errors >= 0 && (int)badclusters.size() > max_errors )
    { show_error( "too many blocks in badblocks file" ); std::exit(1); }
  return ( badclusters.size() > 0 );
  }


int write_badblocks_file( const char *bbname, std::queue< Block > & badclusters,
                          std::queue< Block > & badblocks ) throw()
  {
  if( !bbname ) return 0;
  if( badclusters.size() + badblocks.size() == 0 )
    {
    if( std::remove( bbname ) != 0 && errno != ENOENT )
      {
      char buf[80];
      std::snprintf( buf, sizeof( buf ), "error deleting file %s", bbname );
      show_error( buf, true ); return 1;
      }
    return 0;
    }
  FILE *f = std::fopen( bbname, "w" );
  if( !f )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error opening file %s for writing", bbname );
    show_error( buf, true ); return 1;
    }
  while( badclusters.size() )
    {
    Block block = badclusters.front(); badclusters.pop();
    long long ipos = block.ipos, opos = block.opos;
    std::fprintf( f, "0x%LX  0x%LX  %u\n", ipos, opos, block.size );
    }
  while( badblocks.size() )
    {
    Block block = badblocks.front(); badblocks.pop();
    long long ipos = block.ipos, opos = block.opos;
    std::fprintf( f, "0x%LX  0x%LX  %u\n", ipos, opos, block.size );
    }
  if( std::fclose( f ) )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error writing badblocks list to file %s", bbname );
    show_error( buf, true ); return 1;
    }
  return 0;
  }


int main( int argc, char* argv[] ) throw()
  {
  off_t ipos = -1, opos = -1, max_size = -1;
  size_t cluster = 128, hardbs = 512;
  int max_errors = -1, max_retries = 0, o_trunc = 0, verbosity = 0;

  while( true )
    {
    static struct option const long_options[] =
      {
      {"block-size", required_argument, 0, 'b'},
      {"cluster-size", required_argument, 0, 'c'},
      {"max-errors", required_argument, 0, 'e'},
      {"help", no_argument, 0, 'h'},
      {"input-position", required_argument, 0, 'i'},
      {"output-position", required_argument, 0, 'o'},
      {"quiet", no_argument, 0, 'q'},
      {"max-retries", required_argument, 0, 'r'},
      {"max-size", required_argument, 0, 's'},
      {"truncate", no_argument, 0, 't'},
      {"verbose", no_argument, 0, 'v'},
      {"version", no_argument, 0, 'V'},
      {0, 0, 0, 0}
      };

    int c = getopt_long( argc, argv, "b:c:e:hi:o:qr:s:tvV", long_options, 0 );
    if( c == -1 ) break;		// all options processed

    switch( c )
      {
      case 'b': hardbs = getnum( optarg, 1, 1, INT_MAX ); break;
      case 'c': cluster = getnum( optarg, 1, 1, INT_MAX ); break;
      case 'e': max_errors = getnum( optarg, hardbs, -1, INT_MAX ); break;
      case 'h': show_help( argv[0], cluster, hardbs ); return 0;
      case 'i': ipos = getnum( optarg, hardbs, 0 ); break;
      case 'o': opos = getnum( optarg, hardbs, 0 ); break;
      case 'q': verbosity = -1; break;
      case 'r': max_retries = getnum( optarg, hardbs, -1, INT_MAX ); break;
      case 's': max_size = getnum( optarg, hardbs, 0 ); break;
      case 't': o_trunc = O_TRUNC; break;
      case 'v': verbosity = 1; break;
      case 'V': show_version(); return 0;
      case '?': more_info( argv[0] ); return 1;		// bad option
      default: show_error( argv[optind] ); more_info( argv[0] ); return 1;
      }
    }

  char *iname = 0, *oname = 0, *bbname = 0;
  if( optind < argc ) iname = argv[optind++];
  if( optind < argc ) oname = argv[optind++];
  if( optind < argc ) bbname = argv[optind++];
  if( optind < argc )
    { show_error( "spurious options" ); more_info( argv[0] ); return 1; }
  if( !iname || !oname )
    { show_error( "both input and output must be specified" );
    more_info( argv[0] ); return 1; }
  if( check_identical ( iname, oname ) )
    { show_error( "infile and outfile are identical" ); return 1; }
  int ides = open( iname, O_RDONLY );
  if( ides < 0 ) { show_error( "cannot open input file", true ); return 1; }
  int odes = open( oname, O_WRONLY | O_CREAT | o_trunc, 0644 );
  if( odes < 0 ) { show_error( "cannot open output file", true ); return 1; }
  off_t isize = lseek( ides, 0, SEEK_END );
  if( isize < 0 )
    { show_error( "input file is not seekable" ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "output file is not seekable" ); return 1; }

  if( ipos < 0 ) ipos = 0;
  else if( isize > 0 && isize <= ipos )
    { show_error( "input file is not so big" ); return 1; }
  if( opos < 0 ) opos = ipos;
  if( max_size <= 0 ) max_size = isize;
  if( isize > ipos && isize < ipos + max_size ) max_size = isize - ipos;
  if( cluster < 1 ) cluster = 1;
  if( hardbs < 1 ) hardbs = 1;

  std::queue< Block > badclusters, badblocks;
  off_t errsize = 0;
  bool bbmode = read_badblocks_file( bbname, badclusters, errsize, max_errors );

  if( verbosity > 0 )
    {
    if( bbmode )
      std::printf( "\nabout to copy %d bad blocks from %s to %s\n",
                   badclusters.size(), iname, oname );
    else
      {
      std::printf( "\nabout to copy %sBytes from %s to %s\n",
                   max_size ? format_num( max_size ) : "an infinitude of ",
                   iname, oname );
      std::printf( "starting positions: infile = %sB,", format_num( ipos ) );
      std::printf( "  outfile = %sB\n", format_num( opos ) );
      std::printf( "copy block size: %d hard blocks\n", cluster );
      }
    std::printf( "hard block size: %d bytes\n", hardbs );
    if( max_errors >= 0 ) std::printf( "Max_errors: %d    ", max_errors );
    if( max_retries >= 0 ) std::printf( "Max_retries: %d    ", max_retries );
    std::printf( "Truncate: %s\n\n", o_trunc ? "yes" : "no" );
    }
  if( verbosity >= 0 )
    { show_status( ipos, opos, 0, 0, 0, true ); std::fflush( stdout ); }

  int retval;
  if( !bbmode ) retval = copyfile( badclusters, badblocks, ipos, opos,
                                   max_size, cluster, hardbs, ides, odes,
                                   max_errors, max_retries, verbosity );
  else retval = copybb( badclusters, badblocks, hardbs, ides, odes,
                        max_retries, verbosity, 0, errsize );
  if( !retval ) retval = write_badblocks_file( bbname, badclusters, badblocks );
  return retval;
  }
