/*  GNU ddrescue - Data recovery tool
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

#include <algorithm>
#include <queue>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>


// Date of this version: 2004-12-28

const char * invocation_name = 0;
const char * const program_name    = "ddrescue";
const char * const program_version = "0.9";
const char * const program_year    = "2004";

bool volatile interrupted = false;		// user pressed Ctrl-C
void sighandler( int ) throw() { interrupted = true; }

struct Bigblock
  {
  off_t ipos, opos, size;
  bool ignore;
  Bigblock() throw() : ipos( 0 ), opos( 0 ), size( 0 ), ignore( true ) {}
  Bigblock( off_t i, off_t o, off_t s, bool ig = true ) throw()
    : ipos( i ), opos( o ), size( s ), ignore( ig ) {}
  };

struct Block
  {
  off_t ipos, opos;
  size_t size;

  Block() throw() : ipos( 0 ), opos( 0 ), size( 0 ) {}
  Block( off_t i, off_t o, size_t s ) throw()
    : ipos( i ), opos( o ), size( s ) {}

  bool can_be_split( const size_t hardbs ) const throw();
  bool join( const Block & block ) throw();
  };

struct Control
  {
  Bigblock bigblock;
  std::queue< Block > badclusters_ini, badclusters, badblocks_ini, badblocks;
  int blocks() const throw()
    { return badclusters_ini.size() + badclusters.size() +
             badblocks_ini.size() + badblocks.size(); }
  };


// Returns true if 'block' spans more than one hardware block.
bool Block::can_be_split( const size_t hardbs ) const throw()
  {
  size_t chipsize = ( ipos + size ) % hardbs;
  if( chipsize == 0 ) chipsize = hardbs;
  return ( size > chipsize );
  }


bool Block::join( const Block & block ) throw()
  {
  if( (off_t)size + (off_t)block.size <= INT_MAX )
    {
    if( ipos + size == block.ipos && opos + size == block.opos )
      { size += block.size; return true; }
    if( block.ipos + block.size == ipos && block.opos + block.size == opos )
      { ipos = block.ipos; opos = block.opos; size += block.size; return true; }
    }
  return false;
  }


void show_version() throw()
  {
  std::printf( "GNU %s version %s\n", program_name, program_version );
  std::printf( "Copyright (C) %s Antonio Diaz Diaz.\n", program_year );
  std::printf( "This program is free software; you may redistribute it under the terms of\n" );
  std::printf( "the GNU General Public License.  This program has absolutely no warranty.\n" );
  }


void show_error( const char * msg, int errcode = 0, bool help = false ) throw()
  {
  if( msg && msg[0] != 0 )
    {
    std::fprintf( stderr, "%s: %s", program_name, msg );
    if( errcode > 0 ) std::fprintf( stderr, ": %s", strerror( errcode ) );
    std::fprintf( stderr, "\n" );
    }
  if( help && invocation_name && invocation_name[0] != 0 )
    std::fprintf( stderr, "Try `%s --help' for more information.\n", invocation_name );
  }


void show_help( const int cluster, const int hardbs ) throw()
  {
  std::printf( "GNU %s - Data recovery tool.\n", program_name );
  std::printf( "Copies data from one file or block device to another,\n" );
  std::printf( "trying hard to rescue data in case of read errors.\n" );
  std::printf( "\nUsage: %s [options] infile outfile [logfile]\n", invocation_name );
  std::printf( "Options:\n" );
  std::printf( "  -h, --help                   display this help and exit\n" );
  std::printf( "  -V, --version                output version information and exit\n" );
  std::printf( "  -B, --binary-prefixes        show binary multipliers in numbers [default SI]\n" );
  std::printf( "  -b, --block-size=<bytes>     hardware block size of input device [%d]\n", hardbs );
  std::printf( "  -c, --cluster-size=<blocks>  hardware blocks to copy at a time [%d]\n", cluster );
  std::printf( "  -e, --max-errors=<n>         maximum number of error areas allowed\n" );
  std::printf( "  -i, --input-position=<pos>   starting position in input file [0]\n" );
  std::printf( "  -n, --no-split               do not try to split error areas\n" );
  std::printf( "  -o, --output-position=<pos>  starting position in output file [ipos]\n" );
  std::printf( "  -q, --quiet                  quiet operation\n" );
  std::printf( "  -r, --max-retries=<n>        exit after given retries (-1=infinity) [0]\n" );
  std::printf( "  -s, --max-size=<bytes>       maximum size of data to be copied\n" );
  std::printf( "  -t, --truncate               truncate output file\n" );
  std::printf( "  -v, --verbose                verbose operation\n" );
  std::printf( "Numbers may be followed by a multiplier: b = blocks, k = kB = 10^3 = 1000,\n" );
  std::printf( "Ki = KiB = 2^10 = 1024, M = 10^6, Mi = 2^20, G = 10^9, Gi = 2^30, etc...\n" );
  std::printf( "\nIf logfile given and exists, try to resume the rescue described in it.\n" );
  std::printf( "If logfile given and rescue not finished, write to it the status on exit.\n" );
  std::printf( "\nReport bugs to bug-ddrescue@gnu.org\n");
  }


long long getnum( const char * ptr, size_t bs,
                  long long min = LONG_LONG_MIN + 1,
                  long long max = LONG_LONG_MAX ) throw()
  {
  errno = 0;
  char *tail;
  long long result = std::strtoll( ptr, &tail, 0 );
  if( tail == ptr )
    { show_error( "bad or missing numerical argument", 0, true ); std::exit(1); }

  if( !errno && tail[0] )
    {
    int factor = ( tail[1] == 'i' ) ? 1024 : 1000;
    int exponent = 0;
    bool bad_multiplier = false;
    switch( tail[0] )
      {
      case ' ': break;
      case 'b': if( bs > 0 ) { factor = bs; exponent = 1; }
                else bad_multiplier = true;
                break;
      case 'Y': exponent = 8; break;
      case 'Z': exponent = 7; break;
      case 'E': exponent = 6; break;
      case 'P': exponent = 5; break;
      case 'T': exponent = 4; break;
      case 'G': exponent = 3; break;
      case 'M': exponent = 2; break;
      case 'K': if( factor == 1024 ) exponent = 1; else bad_multiplier = true;
                break;
      case 'k': if( factor == 1000 ) exponent = 1; else bad_multiplier = true;
                break;
      default: bad_multiplier = true;
      }
    if( bad_multiplier )
      { show_error( "bad multiplier in numerical argument", 0, true );
      std::exit(1); }
    for( int i = 0; i < exponent; ++i )
      {
      if( LONG_LONG_MAX / factor >= std::llabs( result ) ) result *= factor;
      else { errno = ERANGE; break; }
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


const char * format_num( long long num, long long max = 999999,
                         const int set_prefix = 0 ) throw()
  {
  static const char * const si_prefix[8] =
    { "k", "M", "G", "T", "P", "E", "Z", "Y" };
  static const char * const binary_prefix[8] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  static bool si = true;
  static char buf[16];

  if( set_prefix ) si = ( set_prefix > 0 );
  int factor = ( si ) ? 1000 : 1024;
  const char * const *prefix = ( si ) ? si_prefix : binary_prefix;
  const char *p = "";
  max = std::max( 999LL, std::min( 999999LL, max ) );

  for( int i = 0; i < 8 && std::llabs( num ) > std::llabs( max ); ++i )
    { num /= factor; p = prefix[i]; }
  std::snprintf( buf, sizeof( buf ), "%lld %s", num, p );
  return buf;
  }


void show_status( off_t ipos, off_t opos, off_t recsize, off_t errsize,
                  size_t errors, const char * msg = 0, bool force = false ) throw()
  {
  static const char * const up = "\x1b[A";
  static off_t a_rate = 0, c_rate = 0, last_size = 0;
  static time_t t1 = 0, t2 = 0;
  static int oldlen = 0;
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
    int len = 0;
    if( msg ) { len = std::strlen( msg ); std::printf( msg ); }
    for( int i = len; i < oldlen; ++i ) std::fputc( ' ', stdout );
    if( len || oldlen ) std::fputc( '\r', stdout );
    oldlen = len;
    std::fflush( stdout );
    }
  }


// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
size_t readblock( int fd, char * buf, size_t size, off_t pos ) throw()
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
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


// Returns the number of bytes really written.
// If returned value < size, it is always an error.
size_t writeblock( int fd, char * buf, size_t size, off_t pos ) throw()
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      int n = write( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( errno && errno != EINTR && errno != EAGAIN ) break;
      }
  return ( rest > 0 ) ? size - rest : size;
  }


// Splits front() and returns in 'block' its last hardware block.
// Returns true if returned block is part of a bigger one at front().
// Returns false if returned block is front() itself.
bool split_block( Block & block, std::queue< Block > & block_queue,
                  const size_t hardbs ) throw()
  {
  block = block_queue.front();
  size_t size = ( block.ipos + block.size ) % hardbs;
  if( size == 0 ) size = hardbs;
  if( block.size <= size ) { block_queue.pop(); return false; }

  size = block.size - size;
  block_queue.front().size = size;
  block.ipos += size; block.opos += size; block.size -= size;
  return true;
  }


// Read the non-damaged part of the file, skipping over the damaged areas.
int copy_non_tried( Bigblock & bigblock, std::queue< Block > & badclusters,
                    std::queue< Block > & badblocks, off_t & recsize,
                    off_t & errsize, const size_t cluster, const size_t hardbs,
                    const int ides, const int odes, int errors,
                    const int max_errors, const int verbosity ) throw()
  {
  if( bigblock.ignore ) return 0;
  const size_t softbs = cluster * hardbs;
  char buf[softbs];
  bool first_post = true;

  while( bigblock.size != 0 && !interrupted )
    {
    if( verbosity >= 0 )
      { show_status( bigblock.ipos, bigblock.opos, recsize, errsize, errors,
                     "Copying data...", first_post ); first_post = false; }
    size_t size = softbs;
    if( bigblock.size >= 0 && bigblock.size < (off_t)size )
      size = bigblock.size;

    size_t rd;
    if( size > hardbs )
      {
      rd = readblock( ides, buf, hardbs, bigblock.ipos );
      if( rd == hardbs )
        rd += readblock( ides, buf + rd, size - rd, bigblock.ipos + rd );
      }
    else rd = readblock( ides, buf, size, bigblock.ipos );
    recsize += rd;
    int errno1 = errno;

    if( rd > 0 && writeblock( odes, buf, rd, bigblock.opos ) != rd )
      { show_error( "write error", errno ); return 1; }
    if( rd < size )
      {
      if( !errno1 )	// EOF
        { bigblock.ipos += rd; bigblock.opos += rd; bigblock.size = 0; break; }
      else		// Read error
        {
        Block block( bigblock.ipos + rd, bigblock.opos + rd, size - rd );
        if( block.can_be_split( hardbs ) ) badclusters.push( block );
        else badblocks.push( block );
        ++errors; errsize += ( block.size );
        }
      }
    bigblock.ipos += size; bigblock.opos += size;
    if( bigblock.size > 0 ) bigblock.size -= size;
    if( max_errors >= 0 && errors > max_errors ) break;
    }

  // Delimit the damaged areas.
  first_post = true;
  for( int i = badclusters.size(); i > 0 && !interrupted; --i )
    while( !interrupted )
      {
      Block block;
      if( !split_block( block, badclusters, hardbs ) )
        { badblocks.push( block ); break; }
      if( verbosity >= 0 )
        { show_status( block.ipos, block.opos, recsize, errsize, errors,
                       "Delimiting error areas...", first_post );
        first_post = false; }
      size_t rd = readblock( ides, buf, block.size, block.ipos );
      recsize += rd; errsize -= rd;
      int errno1 = errno;

      if( rd > 0 && writeblock( odes, buf, rd, block.opos ) != rd )
        { show_error( "write error", errno ); return 1; }
      if( rd < block.size )
        {
        if( !errno1 ) errsize -= ( block.size - rd );	// EOF
        else						// read_error
          {
          Block block2( block.ipos + rd, block.opos + rd, block.size - rd );
          if( !badclusters.front().join( block2 ) )
            { ++errors; badblocks.push( block2 ); }
          if( badclusters.front().can_be_split( hardbs ) )
            badclusters.push( badclusters.front() );
          else badblocks.push( badclusters.front() );
          badclusters.pop();
          break;
          }
        }
      }
  return 0;
  }


// Try to read the damaged areas, splitting them into smaller pieces.
int split_errors( std::queue< Block > & badclusters,
                  std::queue< Block > & badblocks,
                  off_t & recsize, off_t & errsize, const size_t hardbs,
                  const int ides, const int odes, int errors,
                  const int max_errors, const int verbosity ) throw()
  {
  char buf[hardbs];
  bool first_post = true;

  while( badclusters.size() && !interrupted )
    {
    Block block;
    if( !split_block( block, badclusters, hardbs ) )
      { badblocks.push( block ); continue; }
    if( verbosity >= 0 )
      { show_status( block.ipos, block.opos, recsize, errsize, errors,
                     "Splitting error areas...", first_post );
      first_post = false; }
    size_t rd = readblock( ides, buf, block.size, block.ipos );
    recsize += rd; errsize -= rd;
    int errno1 = errno;

    if( rd > 0 && writeblock( odes, buf, rd, block.opos ) != rd )
      { show_error( "write error", errno ); return 1; }
    if( rd < block.size )
      {
      if( !errno1 ) errsize -= ( block.size - rd );	// EOF
      else						// read_error
        {
        badblocks.push( Block( block.ipos+rd, block.opos+rd, block.size-rd ) );
        badclusters.push( badclusters.front() ); badclusters.pop();
        ++errors; if( max_errors >= 0 && errors > max_errors ) break;
        }
      }
    }
  return 0;
  }


// Try to read the damaged hardware blocks
int copy_errors( std::queue< Block > & badblocks, off_t & recsize,
                 off_t & errsize, const size_t hardbs, const int ides,
                 const int odes, int errors, const int verbosity,
                 const char * msg ) throw()
  {
  char buf[hardbs];
  bool first_post = true;

  for( int i = badblocks.size(); i > 0 && !interrupted; --i )
    {
    Block block = badblocks.front(); badblocks.pop();
    if( verbosity >= 0 )
      { show_status( block.ipos, block.opos, recsize, errsize, errors,
                     msg, first_post ); first_post = false; }
    size_t rd = readblock( ides, buf, block.size, block.ipos );
    recsize += rd; errsize -= rd;
    if( !errno ) --errors;

    if( rd < block.size )
      {
      if( !errno ) errsize -= ( block.size - rd );	// EOF
      else						// read_error
        badblocks.push( Block( block.ipos+rd, block.opos+rd, block.size-rd ) );
      }
    if( rd > 0 && writeblock( odes, buf, rd, block.opos ) != rd )
      { show_error( "write error", errno ); return 1; }
    }
  return 0;
  }


int copyfile( Control & control, off_t & recsize, off_t & errsize,
              const size_t cluster, const size_t hardbs, const int ides,
              const int odes, const int max_errors, const int max_retries,
              const int verbosity, const bool nosplit ) throw()
  {
  interrupted = false;
  signal( SIGINT, sighandler );
  if( verbosity >= 0 ) std::printf( "Press Ctrl-C to interrupt\n" );

  // Read the non-damaged part of the file, skipping over the damaged areas.
  int retval = copy_non_tried( control.bigblock, control.badclusters,
                               control.badblocks, recsize, errsize, cluster,
                               hardbs, ides, odes, control.blocks(),
                               max_errors, verbosity );
  if( retval || interrupted ||
      ( max_errors >= 0 && control.blocks() > max_errors ) ) return retval;

  // Try to read the damaged areas, splitting them into smaller pieces
  // and reading the non-damaged pieces, until the hardware block size
  // is reached, or until interrupted by the user.
  if( !nosplit )
    {
    retval = split_errors( control.badclusters_ini, control.badblocks,
                           recsize, errsize, hardbs, ides, odes,
                           control.blocks(), max_errors, verbosity );
    if( retval || interrupted ||
        ( max_errors >= 0 && control.blocks() > max_errors ) ) return retval;
    retval = split_errors( control.badclusters, control.badblocks,
                           recsize, errsize, hardbs, ides, odes,
                           control.blocks(), max_errors, verbosity );
    if( retval || interrupted ||
        ( max_errors >= 0 && control.blocks() > max_errors ) ) return retval;
    }

  // Try to read the damaged hardware blocks listed in logfile.
  retval = copy_errors( control.badblocks_ini, recsize, errsize, hardbs,
                        ides, odes, control.blocks(), verbosity,
                        "Copying bad blocks..." );
  if( retval || interrupted ) return retval;

  // Try to read the damaged hardware blocks until the specified number
  // of retries is reached, or until interrupted by the user.
  if( max_retries != 0 )
    {
    while( control.badblocks_ini.size() )
      { control.badblocks.push( control.badblocks_ini.front() );
      control.badblocks_ini.pop(); }

    char msgbuf[80] = "Copying bad blocks... Retry ";
    const int msglen = strlen( msgbuf );
    for( int retry = 1; retry <= max_retries || max_retries < 0; ++retry )
      {
      if( control.badblocks.size() == 0 ) break;
      std::snprintf( msgbuf + msglen, sizeof( msgbuf ) - msglen, "%d", retry );
      retval = copy_errors( control.badblocks, recsize, errsize, hardbs,
                            ides, odes, control.blocks(), verbosity, msgbuf );
      if( retval || interrupted ) return retval;
      }
    }
  return 0;
  }


bool read_logfile( const char * logname, Control & control, off_t & errsize,
                   const int max_errors, const size_t hardbs ) throw()
  {
  if( !logname ) return false;
  FILE *f = std::fopen( logname, "r" );
  if( !f ) return false;

  {
  long long ipos, opos, size;
  int n = std::fscanf( f, "%lli %lli %lli\n", &ipos, &opos, &size );
  if( n < 0 ) return false;	// EOF
  if( n != 3 || ipos < 0 || opos < 0 || size < -1 )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error in first line of logfile %s\n", logname );
    show_error( buf ); std::exit(1);
    }
  control.bigblock.ipos = ipos; control.bigblock.opos = opos;
  control.bigblock.size = size;
  control.bigblock.ignore = ( control.bigblock.size == 0 );
  }

  while( control.badclusters_ini.size() ) control.badclusters_ini.pop();
  while( control.badblocks_ini.size() ) control.badblocks_ini.pop();
  while( control.badclusters.size() ) control.badclusters.pop();
  while( control.badblocks.size() ) control.badblocks.pop();
  errsize = 0;
  int errors;
  for( errors = 0; max_errors < 0 || errors <= max_errors; ++errors )
    {
    long long ipos, opos;
    int size;
    int n = std::fscanf( f, "%lli %lli %i\n", &ipos, &opos, &size );
    if( n < 0 ) break;	// EOF
    if( n != 3 || ipos < 0 || opos < 0 || size <= 0 )
      {
      char buf[80];
      std::snprintf( buf, sizeof( buf ), "error in logfile %s, line %d\n",
                     logname, control.blocks() + 2 );
      show_error( buf ); std::exit(1);
      }
    errsize += size;
    Block block( ipos, opos, size );
    if( block.can_be_split( hardbs ) ) control.badclusters_ini.push( block );
    else control.badblocks_ini.push( block );
    }

  std::fclose( f );
  if( max_errors >= 0 && errors > max_errors )
    { show_error( "too many blocks in logfile" ); std::exit(1); }
  return true;
  }


void write_queue( FILE *f, std::queue< Block > & block_queue ) throw()
  {
  while( block_queue.size() )
    {
    Block block = block_queue.front(); block_queue.pop();
    long long ipos = block.ipos, opos = block.opos;
    std::fprintf( f, "0x%llX  0x%llX  %u\n", ipos, opos, block.size );
    }
  }


int write_logfile( const char * logname, Control & control ) throw()
  {
  if( !logname ) return 0;
  if( !control.bigblock.size && !control.blocks() )
    {
    if( std::remove( logname ) != 0 && errno != ENOENT )
      {
      char buf[80];
      std::snprintf( buf, sizeof( buf ), "error deleting logfile %s", logname );
      show_error( buf, errno ); return 1;
      }
    return 0;
    }

  FILE *f = std::fopen( logname, "w" );
  if( !f )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error opening logfile %s for writing", logname );
    show_error( buf, errno ); return 1;
    }

  {
  long long ipos = control.bigblock.ipos, opos = control.bigblock.opos;
  long long size = control.bigblock.size;
  if( size == 0 ) { ipos = 0; opos = 0; }
  std::fprintf( f, "0x%llX  0x%llX  %lld\n", ipos, opos, size );
  }

  write_queue( f, control.badclusters_ini );
  write_queue( f, control.badclusters );
  write_queue( f, control.badblocks_ini );
  write_queue( f, control.badblocks );

  if( std::fclose( f ) )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error writing logfile %s", logname );
    show_error( buf, errno ); return 1;
    }
  return 0;
  }


void set_bigblock( Bigblock & bigblock, off_t ipos, off_t opos, off_t max_size,
                   off_t isize, bool logmode ) throw()
  {
  if( !logmode )		// No Bigblock read from logfile (no logfile)
    {
    if( ipos >= 0 ) bigblock.ipos = ipos; else bigblock.ipos = 0;
    if( opos >= 0 ) bigblock.opos = opos; else bigblock.opos = bigblock.ipos;
    bigblock.size = max_size;
    bigblock.ignore = ( bigblock.size == 0 );
    }
  else if( ipos >= 0 || opos >= 0 || max_size > 0 )
    {
    show_error( "ipos, opos and max_size are incompatible with logfile input" );
    std::exit(1);
    }
  else if( max_size == 0 ) bigblock.ignore = true;

  if( isize > 0 && !bigblock.ignore )
    {
    if( bigblock.ipos >= isize )
      { show_error( "input file is not so big" ); std::exit(1); }
    if( bigblock.size < 0 ) bigblock.size = isize;
    if( bigblock.ipos + bigblock.size > isize )
      bigblock.size = isize - bigblock.ipos;
    }
  }


int main( int argc, char * argv[] ) throw()
  {
  off_t ipos = -1, opos = -1, max_size = -1;
  const size_t cluster_bytes = 65536, default_hardbs = 512;
  size_t cluster = 0, hardbs = 512;
  int max_errors = -1, max_retries = 0, o_trunc = 0, verbosity = 0;
  bool nosplit = false;
  invocation_name = argv[0];

  while( true )
    {
    static struct option const long_options[] =
      {
      {"binary_prefixes",       no_argument, 0, 'B'},
      {"block-size",      required_argument, 0, 'b'},
      {"cluster-size",    required_argument, 0, 'c'},
      {"help",                  no_argument, 0, 'h'},
      {"input-position",  required_argument, 0, 'i'},
      {"max-errors",      required_argument, 0, 'e'},
      {"max-retries",     required_argument, 0, 'r'},
      {"max-size",        required_argument, 0, 's'},
      {"no-split",              no_argument, 0, 'n'},
      {"output-position", required_argument, 0, 'o'},
      {"quiet",                 no_argument, 0, 'q'},
      {"truncate",              no_argument, 0, 't'},
      {"verbose",               no_argument, 0, 'v'},
      {"version",               no_argument, 0, 'V'},
      {0, 0, 0, 0}
      };

    int c = getopt_long( argc, argv, "BVb:c:e:hi:no:qr:s:tv", long_options, 0 );
    if( c == -1 ) break;		// all options processed

    switch( c )
      {
      case 'B': format_num( 0, 0, -1 ); break;		// set binary prefixes
      case 'V': show_version(); return 0;
      case 'b': hardbs = getnum( optarg, 0, 1, INT_MAX ); break;
      case 'c': cluster = getnum( optarg, 1, 1, INT_MAX ); break;
      case 'e': max_errors = getnum( optarg, 0, -1, INT_MAX ); break;
      case 'h': show_help( cluster_bytes / default_hardbs, default_hardbs ); return 0;
      case 'i': ipos = getnum( optarg, hardbs, 0 ); break;
      case 'n': nosplit = true; break;
      case 'o': opos = getnum( optarg, hardbs, 0 ); break;
      case 'q': verbosity = -1; break;
      case 'r': max_retries = getnum( optarg, 0, -1, INT_MAX ); break;
      case 's': max_size = getnum( optarg, hardbs, -1 ); break;
      case 't': o_trunc = O_TRUNC; break;
      case 'v': verbosity = 1; break;
      case '?': show_error( 0, 0, true ); return 1;		// bad option
      default : show_error( argv[optind], 0, true ); return 1;
      }
    }

  if( hardbs < 1 ) hardbs = default_hardbs;
  if( cluster < 1 ) cluster = cluster_bytes / hardbs;
  if( cluster < 1 ) cluster = 1;

  const char *iname = 0, *oname = 0, *logname = 0;
  if( optind < argc ) iname = argv[optind++];
  if( optind < argc ) oname = argv[optind++];
  if( optind < argc ) logname = argv[optind++];
  if( optind < argc )
    { show_error( "spurious options", 0, true ); return 1; }
  if( !iname || !oname )
    { show_error( "both input and output must be specified", 0, true );
    return 1; }
  if( check_identical ( iname, oname ) )
    { show_error( "infile and outfile are identical" ); return 1; }

  Control control;
  off_t recsize = 0, errsize = 0;
  bool logmode = read_logfile( logname, control, errsize, max_errors, hardbs );
  if( o_trunc && logmode )
    { show_error( "outfile truncation and logfile input are not compatible", 0, true );
    return 1; }

  int ides = open( iname, O_RDONLY );
  if( ides < 0 ) { show_error( "cannot open input file", errno ); return 1; }
  int odes = open( oname, O_WRONLY | O_CREAT | o_trunc, 0644 );
  if( odes < 0 ) { show_error( "cannot open output file", errno ); return 1; }
  off_t isize = lseek( ides, 0, SEEK_END );
  if( isize < 0 )
    { show_error( "input file is not seekable" ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "output file is not seekable" ); return 1; }

  set_bigblock( control.bigblock, ipos, opos, max_size, isize, logmode );

  if( control.bigblock.ignore && !control.badblocks_ini.size() &&
      ( nosplit || !control.badclusters_ini.size() ) )
    { if( verbosity >= 0 ) { show_error( "Nothing to do" ); } return 0; }

  if( verbosity > 0 )
    {
    if( !control.bigblock.ignore )
      {
      std::printf( "\nAbout to copy %sBytes from %s to %s",
                   ( control.bigblock.size >= 0 ) ?
                     format_num( control.bigblock.size ) : "an undefined number of ",
                   iname, oname );
      std::printf( "\n    Starting positions: infile = %sB,  outfile = %sB",
                   format_num( control.bigblock.ipos ),
                   format_num( control.bigblock.opos ) );
      std::printf( "\n    Copy block size: %d hard blocks", cluster );
      }
    if( !nosplit && control.badclusters_ini.size() )
      std::printf( "\nAbout to split and copy %d error areas from %s to %s",
                   control.badclusters_ini.size(), iname, oname );
    if( control.badblocks_ini.size() )
      std::printf( "\nAbout to copy %d bad blocks from %s to %s",
                   control.badblocks_ini.size(), iname, oname );
    std::printf( "\nHard block size: %d bytes\n", hardbs );
    if( max_errors >= 0 ) std::printf( "Max_errors: %d    ", max_errors );
    if( max_retries >= 0 ) std::printf( "Max_retries: %d    ", max_retries );
    std::printf( "Split: %s    ", !nosplit ? "yes" : "no" );
    std::printf( "Truncate: %s\n\n", o_trunc ? "yes" : "no" );
    }

  int retval = copyfile( control, recsize, errsize, cluster, hardbs, ides,
                         odes, max_errors, max_retries, verbosity, nosplit );
  if( verbosity >= 0 )
    {
    const char *msg = 0;
    if( interrupted ) msg = "Interrupted by user";
    else if( max_errors >= 0 && control.blocks() > max_errors )
      msg = "Too many errors in input file";
    show_status( control.bigblock.ipos, control.bigblock.opos,
                 recsize, errsize, control.blocks(), msg, true );
    std::fputc( '\n', stdout );
    }

  if( !retval ) retval = write_logfile( logname, control );
  return retval;
  }
