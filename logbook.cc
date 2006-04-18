/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006 Antonio Diaz Diaz.

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <unistd.h>

#include "ddrescue.h"


namespace {

int my_fgetc( FILE * f ) throw()
  {
  int ch;
  bool comment = false;

  do {
    ch = std::fgetc( f );
    if( ch == '#' ) comment = true;
    else if( ch == '\n' || ch == EOF ) comment = false;
    }
  while( comment );
  return ch;
  }


const char * my_fgets( FILE * f, int & linenum ) throw()
  {
  const int maxlen = 127;
  static char buf[maxlen+1];
  int ch, len = 1;

  while( len == 1 )
    {
    do { ch = my_fgetc( f ); if( ch == '\n' ) ++linenum; }
    while( std::isspace( ch ) );
    len = 0;
    while( true )
      {
      if( ch == EOF ) { if( len > 0 ) ch = '\n'; else break; }
      if( len < maxlen ) buf[len++] = ch;
      if( ch == '\n' ) { ++linenum; break; }
      ch = my_fgetc( f );
      }
    }
  if( len > 0 ) { buf[len] = 0; return buf; }
  else return 0;
  }


void compact_sblock_vector( std::vector< Sblock > & sblock_vector ) throw()
  {
  for( unsigned int i = 1; i < sblock_vector.size(); )
    {
    Sblock & sb1 = sblock_vector[i-1];
    Sblock & sb2 = sblock_vector[i];
    if( sb1.join( sb2 ) ) sblock_vector.erase( sblock_vector.begin() + i );
    else ++i;
    }
  }


void extend_sblock_vector( std::vector< Sblock > & sblock_vector,
                           const long long isize ) throw()
  {
  if( sblock_vector.size() == 0 )
    {
    Sblock sb( 0, (isize > 0) ? isize : -1, Sblock::non_tried );
    sblock_vector.push_back( sb );
    return;
    }
  Sblock & front = sblock_vector.front();
  if( front.pos() > 0 )
    {
    if( front.status() == Sblock::non_tried ) front.pos( 0 );
    else sblock_vector.insert( sblock_vector.begin(), Sblock( 0, front.pos(), Sblock::non_tried ) );
    }
  Sblock & back = sblock_vector.back();
  const long long end = back.end();
  if( isize > 0 )
    {
    if( back.pos() >= isize )
      {
      if( back.pos() == isize && back.status() == Sblock::non_tried )
        { sblock_vector.pop_back(); return; }
      show_error( "bad logfile; last block begins past end of input file" );
      std::exit(1);
      }
    if( end < 0 || end > isize ) back.size( isize - back.pos() );
    else if( end < isize )
      {
      if( back.status() == Sblock::non_tried ) back.size( isize - back.pos() );
      else
        sblock_vector.push_back( Sblock( end, isize - end, Sblock::non_tried ) );
      }
    }
  else if( end >= 0 )
    {
    if( back.status() == Sblock::non_tried ) back.size( -1 );
    else sblock_vector.push_back( Sblock( end, -1, Sblock::non_tried ) );
    }
  }


void split_domain_border_sblocks( std::vector< Sblock > & sblock_vector,
                                  const Block & domain ) throw()
  {
  unsigned int i = 0;
  for( ; i < sblock_vector.size(); ++i )
    {
    Sblock & sb = sblock_vector[i];
    if( sb.includes( domain.pos() ) )
      {
      if( sb.status() != Sblock::done )
        {
        Sblock head = sb.split( domain.pos() );
        if( head.size() != 0 )
          { sblock_vector.insert( sblock_vector.begin() + i, head ); ++i; }
        }
      break;
      }
    }
  long long end = domain.end();
  if( end < 0 ) return;
  for( ; i < sblock_vector.size(); ++i )
    {
    Sblock & sb = sblock_vector[i];
    if( sb.includes( end ) )
      {
      if( sb.status() != Sblock::done )
        {
        Sblock head = sb.split( end );
        if( head.size() != 0 )
          { sblock_vector.insert( sblock_vector.begin() + i, head ); ++i; }
        }
      break;
      }
    }
  }


void logfile_error( const char * filename, const int linenum ) throw()
  {
  char buf[80];
  std::snprintf( buf, sizeof( buf ), "error in logfile %s, line %d\n",
                 filename, linenum );
  show_error( buf );
  }


// Writes periodically the logfile to disc.
// Returns false only if update is attempted and fails.
//
bool update_logfile( const std::vector< Sblock > & sblock_vector,
                     const char * filename, const int odes,
                     const bool force = false ) throw()
  {
  static time_t t1 = std::time( 0 );

  if( !filename ) return true;
  const int interval = 30 + std::min( 270, (int)sblock_vector.size() / 40 );
  const time_t t2 = std::time( 0 );
  if( !force && t2 - t1 < interval ) return true;
  t1 = t2;
  fsync( odes );

  FILE *f = std::fopen( filename, "w" );
  if( !f )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error opening logfile %s for writing", filename );
    show_error( buf, errno );
    return false;
    }

  write_logfile_header( f );
  std::fprintf( f, "#      pos        size  status\n" );
  for( unsigned int i = 0; i < sblock_vector.size(); ++i )
    {
    const Sblock & sb = sblock_vector[i];
    if( sb.size() >= 0 )
      std::fprintf( f, "0x%08llX  0x%08llX  %c\n", sb.pos(), sb.size(), sb.status() );
    else std::fprintf( f, "0x%08llX          -1  %c\n", sb.pos(), sb.status() );
    }

  if( std::fclose( f ) )
    {
    char buf[80];
    std::snprintf( buf, sizeof( buf ), "error writing logfile %s", filename );
    show_error( buf, errno );
    return false;
    }
  return true;
  }

} // end namespace


void Logbook::set_rescue_domain( const long long ipos, const long long opos,
                                 const long long max_size, const long long isize ) throw()
  {
  _domain.pos( std::max( 0LL, ipos ) );
  if( opos < 0 ) _offset = 0; else _offset = opos - _domain.pos();
  _domain.size( max_size );
  if( isize > 0 )
    {
    if( _domain.pos() >= isize )
      {
      char buf[80];
      std::snprintf( buf, sizeof( buf ), "can't start reading at pos %lld",
                     _domain.pos() ); show_error( buf );
      std::snprintf( buf, sizeof( buf ), "input file is only %lld bytes long",
                     isize ); show_error( buf );
      std::exit(1);
      }
    if( _domain.size() < 0  || _domain.pos() + _domain.size() > isize )
      _domain.size( isize - _domain.pos() );
    }
  }


bool Logbook::read_logfile() throw()
  {
  if( !filename ) return false;
  FILE *f = std::fopen( filename, "r" );
  if( !f ) return false;
  int linenum = 0;
  sblock_vector.clear();

  while( true )
    {
    const char *line = my_fgets( f, linenum );
    if( !line ) break;
    long long pos, size;
    char ch;
    int n = std::sscanf( line, "%lli %lli %c\n", &pos, &size, &ch );
    if( n == 3 && pos >= 0 && ( size > 0 || size == -1 ) &&
        Sblock::isstatus( ch ) )
      {
      Sblock::Status st = Sblock::Status( ch );
      Sblock sb( pos, size, st );
      if( sblock_vector.size() > 0 && !sb.follows( sblock_vector.back() ) )
        { logfile_error( filename, linenum ); std::exit(1); }
      sblock_vector.push_back( sb );
      }
    else { logfile_error( filename, linenum ); std::exit(2); }
    }

  std::fclose( f );
  return true;
  }


// Read the non-damaged part of the domain, skipping over the damaged areas.
//
int Logbook::copy_non_tried() throw()
  {
  unsigned int index = 0;
  bool first_post = true;
  split_domain_border_sblocks( sblock_vector, _domain );

  while( index < sblock_vector.size() )
    {
    const Sblock & sb = sblock_vector[index];
    if( !sb.overlaps( _domain ) )
      { if( sb < _domain ) { ++index; continue; } else break; }
    if( sb.status() != Sblock::non_tried ) { ++index; continue; }
    bool block_done = false;
    while( !block_done )
      {
      Block & block = sblock_vector[index];
      Block chip = block.split( block.pos() + _softbs );
      if( chip.size() == 0 )
        {
        chip = block; sblock_vector.erase( sblock_vector.begin() + index );
        block_done = true;
        }
      if( _verbosity >= 0 )
        {
        show_status( chip.pos(), chip.pos() + _offset, recsize, errsize, errors,
                     "Copying data...", first_post ); first_post = false;
        }
      std::vector< Sblock > result;
      int retval = copy_non_tried_block( chip, result );
      if( retval == -2 )	// EOF
        {
        sblock_vector.erase( sblock_vector.begin() + index, sblock_vector.end() );
        retval = 0; block_done = true;
        }
      if( result.size() )
        {
        if( index > 0 && sblock_vector[index-1].join( result[0] ) )
          result.erase( result.begin() );
        if( result.size() )
          { sblock_vector.insert( sblock_vector.begin() + index, result.begin(), result.end() );
          index += result.size(); }
        }
      if( retval || ( _max_errors >= 0 && errors > _max_errors ) )
        return retval;
      if( !update_logfile( sblock_vector, filename, _odes ) ) return 1;
      }
    }
  return 0;
  }


// Try to read the damaged areas, splitting them into smaller pieces.
//
int Logbook::split_errors() throw()
  {
  unsigned int index = 0;
  bool first_post = true;
  split_domain_border_sblocks( sblock_vector, _domain );

  while( index < sblock_vector.size() )
    {
    const Sblock & sb = sblock_vector[index];
    if( !sb.overlaps( _domain ) )
      { if( sb < _domain ) { ++index; continue; } else break; }
    if( sb.status() != Sblock::bad_cluster ) { ++index; continue; }
    bool block_done = false;
    while( !block_done )
      {
      Block & block = sblock_vector[index];
      Block chip = block.split_hb( _hardbs );
      if( chip.size() > 0 ) ++errors;
      else
        {
        chip = block; sblock_vector.erase( sblock_vector.begin() + index );
        block_done = true;
        }
      if( _verbosity >= 0 )
        {
        show_status( chip.pos(), chip.pos() + _offset, recsize, errsize, errors,
                     "Splitting error areas...", first_post ); first_post = false;
        }
      std::vector< Sblock > result;
      int retval = copy_bad_block( chip, result );
      if( retval == -2 )	// EOF
        {
        sblock_vector.erase( sblock_vector.begin() + index, sblock_vector.end() );
        retval = 0; block_done = true;
        }
      if( result.size() )
        {
        if( index > 0 && sblock_vector[index-1].join( result[0] ) )
          result.erase( result.begin() );
        if( result.size() )
          { sblock_vector.insert( sblock_vector.begin() + index, result.begin(), result.end() );
          index += result.size(); }
        }
      if( retval || ( _max_errors >= 0 && errors > _max_errors ) )
        return retval;
      if( !update_logfile( sblock_vector, filename, _odes ) ) return 1;
      }
    }
  return 0;
  }


int Logbook::copy_errors() throw()
  {
  if( _max_retries != 0 )
    {
    char msgbuf[80] = "Copying bad blocks... Retry ";
    const int msglen = std::strlen( msgbuf );
    for( int retry = 1; _max_retries < 0 || retry <= _max_retries; ++retry )
      {
      std::snprintf( msgbuf + msglen, sizeof( msgbuf ) - msglen, "%d", retry );
      unsigned int index = 0;
      bool first_post = true, bad_block_found = false;
      split_domain_border_sblocks( sblock_vector, _domain );

      while( index < sblock_vector.size() )
        {
        const Sblock & sb = sblock_vector[index];
        if( !sb.overlaps( _domain ) )
          { if( sb < _domain ) { ++index; continue; } else break; }
        if( sb.status() != Sblock::bad_block ) { ++index; continue; }
        bad_block_found = true;
        bool block_done = false;
        while( !block_done )
          {
          Sblock & sb = sblock_vector[index];
          Block chip = sb.split_hb( _hardbs );
          if( chip.size() == 0 )
            {
            chip = sb; sblock_vector.erase( sblock_vector.begin() + index );
            block_done = true;
            }
          if( _verbosity >= 0 )
            {
            show_status( chip.pos(), chip.pos() + _offset, recsize, errsize, errors,
                         msgbuf, first_post ); first_post = false;
            }
          std::vector< Sblock > result;
          int retval = copy_bad_block( chip, result );
          if( retval == -2 )	// EOF
            {
            sblock_vector.erase( sblock_vector.begin() + index, sblock_vector.end() );
            retval = 0; block_done = true;
            }
          if( result.size() )
            {
            if( index > 0 && sblock_vector[index-1].join( result[0] ) )
              result.erase( result.begin() );
            if( result.size() )
              { sblock_vector.insert( sblock_vector.begin() + index, result.begin(), result.end() );
              index += result.size(); }
            }
          if( retval ) return retval;
          if( !update_logfile( sblock_vector, filename, _odes ) ) return 1;
          }
        }
      if( !bad_block_found ) break;
      }
    }
  return 0;
  }


Logbook::Logbook( const long long ipos, const long long opos,
                  const long long max_size, const long long isize,
                  const char * name, const int cluster, const int hardbs,
                  const int max_errors, const int max_retries, const int verbosity,
                  const bool complete_only, const bool nosplit ) throw()
  : filename( name ), _hardbs( hardbs ), _softbs( cluster * hardbs ),
    _max_errors( max_errors ), _max_retries( max_retries ),
    _verbosity( verbosity ), _nosplit( nosplit )
  {
  int alignment = sysconf( _SC_PAGESIZE );
  if( alignment < _hardbs || alignment % _hardbs ) alignment = _hardbs;
  if( alignment < 2 || alignment > 65536 ) alignment = 0;
  iobuf = iobuf_base = new char[ _softbs + alignment ];
  if( alignment > 1 )		// align iobuf for use with raw devices
    {
    const int disp = alignment - ( reinterpret_cast<long> (iobuf) % alignment );
    if( disp > 0 && disp < alignment ) iobuf += disp;
    }

  set_rescue_domain( ipos, opos, max_size, isize );
  if( filename ) read_logfile();
  if( !complete_only ) extend_sblock_vector( sblock_vector, isize );
  else		// limit _domain to blocks of finite size read from logfile
    {
    if( sblock_vector.size() && sblock_vector.back().size() < 0 )
      sblock_vector.pop_back();
    if( sblock_vector.size() )
      {
      Block b( sblock_vector.front().pos(),
               sblock_vector.back().end() - sblock_vector.front().pos() );
      _domain = b.overlap( _domain );
      }
    else _domain.size( 0 );
    }
  compact_sblock_vector( sblock_vector );
  }


bool Logbook::blank() const throw()
  {
  return ( sblock_vector.size() == 1 &&
           sblock_vector[0].status() == Sblock::non_tried );
  }


int Logbook::do_rescue( const int ides, const int odes ) throw()
  {
  recsize = 0; errsize = 0; errors = 0;
  _ides = ides; _odes = odes;

  for( unsigned i = 0; i < sblock_vector.size(); ++i )
    {
    Sblock & sb = sblock_vector[i];
    Block b = sb.overlap( _domain );
    if( b.size() == 0 ) { if( sb < _domain ) continue; else break; }
    switch( sb.status() )
      {
      case Sblock::non_tried: break;
      case Sblock::bad_cluster: ++errors; errsize += b.size(); break;
      case Sblock::bad_block:
        errors += b.hard_blocks( _hardbs ); errsize += b.size(); break;
      case Sblock::done: recsize += b.size(); break;
      }
    }
  set_handler();
  if( _verbosity >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( filename )
      {
      std::printf( "Initial status (read from logfile)\n" );
      std::printf( "rescued: %10sB,", format_num( recsize ) );
      std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
      std::printf( "  errors: %7u\n", errors );
      std::printf( "Current status\n" );
      }
    }
  int retval = 0;
  if( _max_errors < 0 || errors <= _max_errors )
    retval = copy_non_tried();
  if( retval == 0 && !_nosplit && ( _max_errors < 0 || errors <= _max_errors ) )
    retval = split_errors();
  if( retval == 0 && ( _max_errors < 0 || errors <= _max_errors ) )
    retval = copy_errors();
  if( _verbosity >= 0 )
    {
    const char *msg = 0;
    if( retval < 0 ) { msg = "Interrupted by user"; retval = 0; }
    else if( _max_errors >= 0 && errors > _max_errors )
      msg = "Too many errors in input file";
    show_status( -1, -1, recsize, errsize, errors, msg, true );
    std::fputc( '\n', stdout );
    }
  compact_sblock_vector( sblock_vector );
  if( !update_logfile( sblock_vector, filename, _odes, true ) && retval == 0 )
    retval = 1;
  return retval;
  }
