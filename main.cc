/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010
    Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
    Return values: 0 for a normal exit, 1 for environmental problems
    (file not found, invalid flags, I/O errors, etc), 2 to indicate a
    corrupt or invalid input file, 3 for an internal consistency error
    (eg, bug) which caused ddrescue to panic.
*/

#define _FILE_OFFSET_BITS 64

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "arg_parser.h"
#include "block.h"
#include "ddrescue.h"


namespace {

const char * invocation_name = 0;
const char * const Program_name    = "GNU ddrescue";
const char * const program_name    = "ddrescue";
const char * const program_year    = "2010";

#ifdef O_BINARY
const int o_binary = O_BINARY;
#else
const int o_binary = 0;
#endif

const mode_t outmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;


void show_help( const int cluster, const int hardbs ) throw()
  {
  std::printf( "%s - Data recovery tool.\n", Program_name );
  std::printf( "Copies data from one file or block device to another,\n" );
  std::printf( "trying hard to rescue data in case of read errors.\n" );
  std::printf( "\nUsage: %s [options] infile outfile [logfile]\n", invocation_name );
  std::printf( "You should use a logfile unless you know what you are doing.\n" );
  std::printf( "\nOptions:\n" );
  std::printf( "  -h, --help                    display this help and exit\n" );
  std::printf( "  -V, --version                 output version information and exit\n" );
  std::printf( "  -b, --block-size=<bytes>      sector size of input device [default %d]\n", hardbs );
  std::printf( "  -B, --binary-prefixes         show binary multipliers in numbers [SI]\n" );
  std::printf( "  -c, --cluster-size=<sectors>  sectors to copy at a time [%d]\n", cluster );
  std::printf( "  -C, --complete-only           do not read new data beyond logfile limits\n" );
  std::printf( "  -d, --direct                  use direct disc access for input file\n" );
  std::printf( "  -D, --synchronous             use synchronous writes for output file\n" );
  std::printf( "  -e, --max-errors=<n>          maximum number of error areas allowed\n" );
  std::printf( "  -f, --force                   overwrite output device or partition\n" );
  std::printf( "  -F, --fill=<types>            fill given type blocks with infile data (?*/-+)\n" );
  std::printf( "  -g, --generate-logfile        generate approximate logfile from partial copy\n" );
  std::printf( "  -i, --input-position=<pos>    starting position in input file [0]\n" );
  std::printf( "  -m, --domain-logfile=<file>   restrict domain to finished blocks in file\n" );
  std::printf( "  -n, --no-split                do not try to split or retry failed blocks\n" );
  std::printf( "  -o, --output-position=<pos>   starting position in output file [ipos]\n" );
  std::printf( "  -p, --preallocate             preallocate space on disc for output file\n" );
  std::printf( "  -q, --quiet                   suppress all messages\n" );
  std::printf( "  -r, --max-retries=<n>         exit after given retries (-1=infinity) [0]\n" );
  std::printf( "  -R, --retrim                  mark all failed blocks as non-trimmed\n" );
  std::printf( "  -s, --max-size=<bytes>        maximum size of input data to be copied\n" );
  std::printf( "  -S, --sparse                  use sparse writes for output file\n" );
  std::printf( "  -t, --truncate                truncate output file to zero size\n" );
  std::printf( "  -T, --try-again               mark non-split, non-trimmed blocks as non-tried\n" );
  std::printf( "  -v, --verbose                 verbose operation\n" );
  std::printf( "Numbers may be followed by a multiplier: b = blocks, k = kB = 10^3 = 1000,\n" );
  std::printf( "Ki = KiB = 2^10 = 1024, M = 10^6, Mi = 2^20, G = 10^9, Gi = 2^30, etc...\n" );
  std::printf( "\nReport bugs to bug-ddrescue@gnu.org\n");
  std::printf( "Ddrescue home page: http://www.gnu.org/software/ddrescue/ddrescue.html\n" );
  std::printf( "General help using GNU software: http://www.gnu.org/gethelp\n" );
  }


void show_version() throw()
  {
  std::printf( "%s %s\n", Program_name, PROGVERSION );
  std::printf( "Copyright (C) %s Antonio Diaz Diaz.\n", program_year );
  std::printf( "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n" );
  std::printf( "This is free software: you are free to change and redistribute it.\n" );
  std::printf( "There is NO WARRANTY, to the extent permitted by law.\n" );
  }


long long getnum( const char * const ptr, const int bs,
                  const long long min = LLONG_MIN + 1,
                  const long long max = LLONG_MAX ) throw()
  {
  errno = 0;
  char *tail;
  long long result = strtoll( ptr, &tail, 0 );
  if( tail == ptr )
    {
    show_error( "bad or missing numerical argument.", 0, true );
    std::exit( 1 );
    }

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
      {
      show_error( "bad multiplier in numerical argument.", 0, true );
      std::exit( 1 );
      }
    for( int i = 0; i < exponent; ++i )
      {
      if( LLONG_MAX / factor >= llabs( result ) ) result *= factor;
      else { errno = ERANGE; break; }
      }
    }
  if( !errno && ( result < min || result > max ) ) errno = ERANGE;
  if( errno )
    {
    show_error( "numerical argument out of limits." );
    std::exit( 1 );
    }
  return result;
  }


void check_fill_types( const std::string filltypes ) throw()
  {
  bool error = false;
  for( unsigned int i = 0; i < filltypes.size(); ++i )
    if( !Sblock::isstatus( filltypes[i] ) )
      { error = true; break; }
  if( !filltypes.size() || error )
    {
    show_error( "invalid type for `fill' option." );
    std::exit( 1 );
    }
  }


bool check_identical( const char * const name1, const char * const name2 ) throw()
  {
  if( !std::strcmp( name1, name2 ) ) return true;
  struct stat stat1, stat2;
  if( stat( name1, &stat1 ) || stat( name2, &stat2 ) ) return false;
  return ( stat1.st_ino == stat2.st_ino && stat1.st_dev == stat2.st_dev );
  }


bool check_files( const char * const iname, const char * const oname,
                  const bool force, const bool preallocate ) throw()
  {
  if( !iname || !oname )
    {
    show_error( "both input and output files must be specified.", 0, true );
    return false;
    }
  if( check_identical( iname, oname ) )
    { show_error( "infile and outfile are the same." ); return false; }
  if( !force || preallocate )
    {
    struct stat st;
    if( stat( oname, &st ) == 0 && !S_ISREG( st.st_mode ) )
      {
      show_error( "output file exists and is not a regular file." );
      if( !force )
        {
        show_error( "Use `--force' if you really want to overwrite it, but be" );
        show_error( "aware that all existing data in output file will be lost.", 0, true );
        }
      else if( preallocate )
        show_error( "Only regular files can be preallocated.", 0, true );
      return false;
      }
    }
  return true;
  }


int do_fill( long long ipos, const long long opos, Domain & domain,
             const char *iname, const char *oname, const char *logname,
             const int cluster, const int hardbs,
             const std::string & filltypes, const bool synchronous )
  {
  if( !logname )
    {
    show_error( "logfile required in fill mode.", 0, true );
    return 1;
    }

  Fillbook fillbook( ipos, opos, domain, logname, cluster, hardbs, synchronous );
  if( fillbook.domain().size() == 0 )
    { show_error( "Nothing to do." ); return 0; }

  const int ides = open( iname, O_RDONLY | o_binary );
  if( ides < 0 )
    { show_error( "cannot open input file", errno ); return 1; }
  if( !fillbook.read_buffer( ides ) )
    { show_error( "error reading fill data from input file." ); return 1; }

  const int odes = open( oname, O_CREAT | O_WRONLY | o_binary, outmode );
  if( odes < 0 )
    { show_error( "cannot open output file", errno ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "output file is not seekable." ); return 1; }

  if( verbosity >= 0 ) std::printf( "\n\n" );
  if( verbosity > 0 )
    {
    std::printf( "About to fill with data from %s blocks of %s marked %s\n",
                 iname, oname, filltypes.c_str() );
    std::printf( "    Maximum size to fill: %sBytes\n",
                 format_num( fillbook.domain().size() ) );
    std::printf( "    Starting positions: infile = %sB",
                 format_num( fillbook.domain().pos() ) );
    std::printf( ",  outfile = %sB\n",
                 format_num( fillbook.domain().pos() + fillbook.offset() ) );
    std::printf( "    Copy block size: %d hard blocks\n", cluster );
    std::printf( "Hard block size: %s bytes\n", format_num( hardbs, 99999 ) );
    std::printf( "\n" );
    }

  return fillbook.do_fill( odes, filltypes );
  }


int do_generate( const long long ipos, const long long opos, Domain & domain,
                 const char *iname, const char *oname, const char *logname,
                 const int cluster, const int hardbs )
  {
  if( !logname )
    {
    show_error( "logfile must be specified in generate-logfile mode.", 0, true );
    return 1;
    }

  const int ides = open( iname, O_RDONLY | o_binary );
  if( ides < 0 )
    { show_error( "cannot open input file", errno ); return 1; }
  const long long isize = lseek( ides, 0, SEEK_END );
  if( isize < 0 )
    { show_error( "input file is not seekable." ); return 1; }

  Rescuebook genbook( ipos, opos, domain, isize, 0, logname, cluster, hardbs );
  if( genbook.domain().size() == 0 )
    { show_error( "Nothing to do." ); return 0; }
  if( !genbook.blank() && genbook.current_status() != Logbook::generating )
    {
    show_error( "logfile alredy exists and is non-empty.", 0, true );
    return 1;
    }

  const int odes = open( oname, O_RDONLY | o_binary );
  if( odes < 0 )
    { show_error( "cannot open output file", errno ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "output file is not seekable." ); return 1; }

  if( verbosity >= 0 ) std::printf( "\n\n" );
  if( verbosity > 0 )
    {
    std::printf( "About to generate an approximate logfile for %s and %s\n",
                 iname, oname );
    std::printf( "    Starting positions: infile = %sB",
                 format_num( genbook.domain().pos() ) );
    std::printf( ",  outfile = %sB\n",
                 format_num( genbook.domain().pos() + genbook.offset() ) );
    std::printf( "    Copy block size: %d hard blocks\n", cluster );
    std::printf( "Hard block size: %s bytes\n", format_num( hardbs, 99999 ) );
    std::printf( "\n" );
    }
  return genbook.do_generate( odes );
  }


int do_rescue( const long long ipos, const long long opos, Domain & domain,
               const char *iname, const char *oname, const char *logname,
               const int cluster, const int hardbs,
               const int max_errors, const int max_retries,
               const int o_direct, const int o_trunc,
               const bool complete_only, const bool nosplit,
               const bool preallocate, const bool retrim, const bool sparse,
               const bool synchronous, const bool try_again )
  {
  const int ides = open( iname, O_RDONLY | o_direct | o_binary );
  if( ides < 0 )
    { show_error( "cannot open input file", errno ); return 1; }
  const long long isize = lseek( ides, 0, SEEK_END );
  if( isize < 0 )
    { show_error( "input file is not seekable." ); return 1; }

  Rescuebook rescuebook( ipos, opos, domain, isize, iname, logname, cluster,
                         hardbs, max_errors, max_retries, complete_only,
                         nosplit, retrim, sparse, synchronous, try_again );
  if( rescuebook.domain().size() == 0 )
    { show_error( "Nothing to do." ); return 0; }
  if( o_trunc && !rescuebook.blank() )
    {
    show_error( "outfile truncation and logfile input are incompatible.", 0, true );
    return 1;
    }

  const int odes = open( oname, O_CREAT | O_WRONLY | o_trunc | o_binary,
                         outmode );
  if( odes < 0 )
    { show_error( "cannot open output file", errno ); return 1; }
  if( lseek( odes, 0, SEEK_SET ) )
    { show_error( "output file is not seekable." ); return 1; }
  while( preallocate )
    {
    if( posix_fallocate( odes, 0, rescuebook.domain().end() ) == 0 ) break;
    if( errno != EINTR )
      { show_error( "cannot preallocate output file", errno ); return 1; }
    }

  if( !rescuebook.update_logfile( -1, true ) ) return 1;

  if( verbosity >= 0 ) std::printf( "\n\n" );
  if( verbosity > 0 )
    {
    std::printf( "About to copy %sBytes from %s to %s\n",
                 format_num( rescuebook.domain().size() ), iname, oname );
    std::printf( "    Starting positions: infile = %sB",
                 format_num( rescuebook.domain().pos() ) );
    std::printf( ",  outfile = %sB\n",
                 format_num( rescuebook.domain().pos() + rescuebook.offset() ) );
    std::printf( "    Copy block size: %d hard blocks\n", cluster );
    std::printf( "Hard block size: %s bytes\n", format_num( hardbs, 99999 ) );
    bool nl = false;
    if( max_errors >= 0 )
      { nl = true; std::printf( "Max_errors: %d    ", max_errors ); }
    if( max_retries >= 0 )
      { nl = true; std::printf( "Max_retries: %d    ", max_retries ); }
    if( nl ) std::printf( "\n" );
    std::printf( "Direct: %s    ", o_direct ? "yes" : "no" );
    std::printf( "Sparse: %s    ", sparse ? "yes" : "no" );
    std::printf( "Split: %s    ", !nosplit ? "yes" : "no" );
    std::printf( "Truncate: %s\n", o_trunc ? "yes" : "no" );
    if( complete_only ) std::printf( "Complete only\n" );
    std::printf( "\n" );
    }
  return rescuebook.do_rescue( ides, odes );
  }

} // end namespace


int verbosity = 0;


void show_error( const char * const msg, const int errcode, const bool help ) throw()
  {
  if( verbosity >= 0 )
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
  }


void internal_error( const char * const msg )
  {
  std::string s( "internal error: " ); s += msg;
  show_error( s.c_str() );
  std::exit( 3 );
  }


void write_logfile_header( FILE * const f ) throw()
  {
  std::fprintf( f, "# Rescue Logfile. Created by %s version %s\n",
                Program_name, PROGVERSION );
  }


int main( const int argc, const char * const argv[] )
  {
  long long ipos = 0;
  long long opos = -1;
  long long max_size = -1;
  const char * domain_logfile_name = 0;
  const int cluster_bytes = 65536;
  const int default_hardbs = 512;
  int cluster = 0;
  int hardbs = 512;
  int max_errors = -1;
  int max_retries = 0;
  int o_direct = 0;
  int o_trunc = 0;
  bool complete_only = false;
  bool force = false;
  bool generate = false;
  bool nosplit = false;
  bool preallocate = false;
  bool retrim = false;
  bool sparse = false;
  bool synchronous = false;
  bool try_again = false;
  std::string filltypes;
  invocation_name = argv[0];

  const Arg_parser::Option options[] =
    {
    { 'b', "block-size",       Arg_parser::yes },
    { 'B', "binary-prefixes",  Arg_parser::no  },
    { 'c', "cluster-size",     Arg_parser::yes },
    { 'C', "complete-only",    Arg_parser::no  },
    { 'd', "direct",           Arg_parser::no  },
    { 'D', "synchronous",      Arg_parser::no  },
    { 'e', "max-errors",       Arg_parser::yes },
    { 'f', "force",            Arg_parser::no  },
    { 'F', "fill",             Arg_parser::yes },
    { 'g', "generate-logfile", Arg_parser::no  },
    { 'h', "help",             Arg_parser::no  },
    { 'i', "input-position",   Arg_parser::yes },
    { 'm', "domain-logfile",   Arg_parser::yes },
    { 'n', "no-split",         Arg_parser::no  },
    { 'o', "output-position",  Arg_parser::yes },
    { 'p', "preallocate",      Arg_parser::no  },
    { 'q', "quiet",            Arg_parser::no  },
    { 'r', "max-retries",      Arg_parser::yes },
    { 'R', "retrim",           Arg_parser::no  },
    { 's', "max-size",         Arg_parser::yes },
    { 'S', "sparse",           Arg_parser::no  },
    { 't', "truncate",         Arg_parser::no  },
    { 'T', "try-again",        Arg_parser::no  },
    { 'v', "verbose",          Arg_parser::no  },
    { 'V', "version",          Arg_parser::no  },
    {  0 , 0,                  Arg_parser::no  } };

  Arg_parser parser( argc, argv, options );
  if( parser.error().size() )				// bad option
    { show_error( parser.error().c_str(), 0, true ); return 1; }

  int argind = 0;
  for( ; argind < parser.arguments(); ++argind )
    {
    const int code = parser.code( argind );
    if( !code ) break;					// no more options
    const char * const arg = parser.argument( argind ).c_str();
    switch( code )
      {
      case 'b': hardbs = getnum( arg, 0, 1, INT_MAX ); break;
      case 'B': format_num( 0, 0, -1 ); break;		// set binary prefixes
      case 'c': cluster = getnum( arg, 1, 1, INT_MAX ); break;
      case 'C': complete_only = true; break;
      case 'd':
#ifdef O_DIRECT
                o_direct = O_DIRECT;
#endif
                if( !o_direct )
                  { show_error( "direct disc access not available." ); return 1; }
                break;
      case 'D': synchronous = true; break;
      case 'e': max_errors = getnum( arg, 0, -1, INT_MAX ); break;
      case 'f': force = true; break;
      case 'F': filltypes = arg; check_fill_types( filltypes ); break;
      case 'g': generate = true; break;
      case 'h': show_help( cluster_bytes / default_hardbs, default_hardbs );
                return 0;
      case 'i': ipos = getnum( arg, hardbs, 0 ); break;
      case 'm': domain_logfile_name = arg; break;
      case 'n': nosplit = true; break;
      case 'o': opos = getnum( arg, hardbs, 0 ); break;
      case 'p': preallocate = true; break;
      case 'q': verbosity = -1; break;
      case 'r': max_retries = getnum( arg, 0, -1, INT_MAX ); break;
      case 'R': retrim = true; break;
      case 's': max_size = getnum( arg, hardbs, -1 ); break;
      case 'S': sparse = true; break;
      case 't': o_trunc = O_TRUNC; break;
      case 'T': try_again = true; break;
      case 'v': verbosity = 1; break;
      case 'V': show_version(); return 0;
      default : internal_error( "uncaught option" );
      }
    } // end process options

  if( opos < 0 ) opos = ipos;
  if( hardbs < 1 ) hardbs = default_hardbs;
  if( cluster >= INT_MAX / hardbs ) cluster = ( INT_MAX / hardbs ) - 1;
  if( cluster < 1 ) cluster = cluster_bytes / hardbs;
  if( cluster < 1 ) cluster = 1;

  const char *iname = 0, *oname = 0, *logname = 0;
  if( argind < parser.arguments() ) iname = parser.argument( argind++ ).c_str();
  if( argind < parser.arguments() ) oname = parser.argument( argind++ ).c_str();
  if( argind < parser.arguments() ) logname = parser.argument( argind++ ).c_str();
  if( argind < parser.arguments() )
    { show_error( "too many files.", 0, true ); return 1; }

  // end scan arguments

  if( !check_files( iname, oname, force, preallocate ) ) return 1;

  Domain domain( domain_logfile_name, ipos, max_size );

  if( filltypes.size() )
    {
    if( max_errors >= 0 || max_retries || o_direct || o_trunc ||
        complete_only || generate || nosplit || preallocate || retrim ||
        sparse || synchronous || try_again )
      show_error( "warning: options -C -d -D -e -g -n -p -r -R -S -t and -T" );
      show_error( "are ignored in fill mode." );

    return do_fill( ipos, opos, domain, iname, oname, logname, cluster,
                    hardbs, filltypes, synchronous );
    }
  if( generate )
    {
    if( max_errors >= 0 || max_retries || o_direct || o_trunc ||
        complete_only || nosplit || preallocate || retrim ||
        sparse || synchronous || try_again )
      show_error( "warning: options -C -d -D -e -n -p -r -R -S -t and -T" );
      show_error( "are ignored in generate-logfile mode." );

    return do_generate( ipos, opos, domain, iname, oname, logname,
                        cluster, hardbs );
    }
  return do_rescue( ipos, opos, domain, iname, oname, logname, cluster,
                    hardbs, max_errors, max_retries, o_direct, o_trunc,
                    complete_only, nosplit, preallocate, retrim, sparse,
                    synchronous, try_again );
  }
