/*  GNU ddrescuelog - Conversion tool for ddrescue logfiles
    Copyright (C) 2011 Antonio Diaz Diaz.

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
    (eg, bug) which caused ddrescuelog to panic.
*/

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>

#include "arg_parser.h"
#include "block.h"
#include "ddrescue.h"



namespace {

const char * const Program_name = "GNU ddrescuelog";
const char * const program_name = "ddrescuelog";
const char * const program_year = "2011";
const char * invocation_name = 0;


void show_help( const int hardbs ) throw()
  {
  std::printf( "%s - Conversion tool for ddrescue logfiles.\n", Program_name );
  std::printf( "Manipulates ddrescue logfiles and converts them to/from other formats.\n" );
  std::printf( "\nUsage: %s [options] logfile\n", invocation_name );
  std::printf( "\nOptions:\n" );
  std::printf( "  -h, --help                    display this help and exit\n" );
  std::printf( "  -V, --version                 output version information and exit\n" );
  std::printf( "  -b, --block-size=<bytes>      block size in bytes [default %d]\n", hardbs );
//  std::printf( "  -f, --force                   overwrite existing output files\n" );
  std::printf( "  -i, --input-position=<pos>    starting position of rescue domain [0]\n" );
  std::printf( "  -l, --list-blocks=<types>     print block numbers of given types (?*/-+)\n" );
  std::printf( "  -o, --output-position=<pos>   starting position in output file [ipos]\n" );
  std::printf( "  -q, --quiet                   suppress all messages\n" );
  std::printf( "  -s, --max-size=<bytes>        maximum size of rescue domain to be processed\n" );
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
    show_error( "Bad or missing numerical argument.", 0, true );
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
      show_error( "Bad multiplier in numerical argument.", 0, true );
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
    show_error( "Numerical argument out of limits." );
    std::exit( 1 );
    }
  return result;
  }


void check_block_types( const std::string & blocktypes ) throw()
  {
  bool error = false;
  for( unsigned int i = 0; i < blocktypes.size(); ++i )
    if( !Sblock::isstatus( blocktypes[i] ) )
      { error = true; break; }
  if( !blocktypes.size() || error )
    {
    show_error( "Invalid type for `list-blocks' option." );
    std::exit( 1 );
    }
  }


int to_badblocks( const long long ipos, const long long opos,
                  Domain & domain, const char * const logname,
                  const int hardbs, const std::string & blocktypes )
  {
  long long last_block = -1;
  Logbook logbook( ipos, opos, domain, 0, logname, 1, hardbs, true );
  if( logbook.domain().in_size() == 0 )
    { show_error( "Nothing to do." ); return 0; }

  logbook.current_pos( 0 );
  logbook.current_status( Logbook::finished );

  for( int i = 0; i < logbook.sblocks(); ++i )
    {
    const Sblock & sb = logbook.sblock( i );
    if( !logbook.domain().includes( sb ) )
      { if( logbook.domain() < sb ) break; else continue; }
    if( blocktypes.find( sb.status() ) >= blocktypes.size() ) continue;
    for( long long block = ( sb.pos() + logbook.offset() ) / hardbs;
         block * hardbs < sb.end() + logbook.offset(); ++block )
      {
      if( block > last_block )
        {
        last_block = block;
        std::printf( "%lld\n", block );
        }
      else if( block < last_block ) internal_error( "block out of order" );
      }
    }
  return 0;
  }

} // end namespace


int verbosity = 0;


void show_error( const char * const msg, const int errcode, const bool help ) throw()
  {
  if( verbosity >= 0 )
    {
    if( msg && msg[0] )
      {
      std::fprintf( stderr, "%s: %s", program_name, msg );
      if( errcode > 0 )
        std::fprintf( stderr, ": %s", std::strerror( errcode ) );
      std::fprintf( stderr, "\n" );
      }
    if( help && invocation_name && invocation_name[0] )
      std::fprintf( stderr, "Try `%s --help' for more information.\n",
                    invocation_name );
    }
  }


void internal_error( const char * const msg )
  {
  if( verbosity >= 0 )
    std::fprintf( stderr, "%s: internal error: %s.\n", program_name, msg );
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
  const int default_hardbs = 4096;
  int hardbs = default_hardbs;
  bool force = false;
  std::string blocktypes;
  invocation_name = argv[0];
  const Arg_parser::Option options[] =
    {
    { 'b', "block-size",       Arg_parser::yes },
    { 'f', "force",            Arg_parser::no  },
    { 'h', "help",             Arg_parser::no  },
    { 'i', "input-position",   Arg_parser::yes },
    { 'l', "list-blocks",      Arg_parser::yes },
    { 'o', "output-position",  Arg_parser::yes },
    { 'q', "quiet",            Arg_parser::no  },
    { 's', "max-size",         Arg_parser::yes },
    { 'v', "verbose",          Arg_parser::no  },
    { 'V', "version",          Arg_parser::no  },
    {  0 , 0,                  Arg_parser::no  } };

  const Arg_parser parser( argc, argv, options );
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
      case 'f': force = true; break;
      case 'h': show_help( default_hardbs ); return 0;
      case 'i': ipos = getnum( arg, hardbs, 0 ); break;
      case 'l': blocktypes = arg; check_block_types( blocktypes ); break;
      case 'o': opos = getnum( arg, hardbs, 0 ); break;
      case 'q': verbosity = -1; break;
      case 's': max_size = getnum( arg, hardbs, -1 ); break;
      case 'v': verbosity = 1; break;
      case 'V': show_version(); return 0;
      default : internal_error( "uncaught option" );
      }
    } // end process options

  if( opos < 0 ) opos = ipos;

  if( argind + 1 != parser.arguments() )
    {
    if( argind < parser.arguments() )
      show_error( "Too many files.", 0, true );
    else
      show_error( "A logfile must be specified.", 0, true );
    return 1;
    }

  const char * const logname = parser.argument( argind++ ).c_str();

  // end scan arguments

  if( !blocktypes.size() )
    {
    show_error( "You must specify the operation to be performed.",
                0, true );
    return 1;
    }

  Domain domain( ipos, max_size );

  return to_badblocks( ipos, opos, domain, logname, hardbs, blocktypes );
  }
