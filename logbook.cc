/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007 Antonio Diaz Diaz.

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

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
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


void extend_sblock_vector( std::vector< Sblock > & sblock_vector,
                           const long long isize, const int verbosity ) throw()
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
      if( verbosity >= 0 )
        show_error( "bad logfile; last block begins past end of input file" );
      std::exit( 1 );
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


void show_logfile_error( const char * filename, const int linenum ) throw()
  {
  char buf[80];
  snprintf( buf, sizeof( buf ), "error in logfile %s, line %d",
                 filename, linenum );
  show_error( buf );
  }

} // end namespace


bool Logbook::read_logfile() throw()
  {
  if( !_filename ) return false;
  FILE *f = std::fopen( _filename, "r" );
  if( !f ) return false;
  int linenum = 0;
  sblock_vector.clear();

  const char *line = my_fgets( f, linenum );
  if( line )						// status line
    {
    char ch;
    int n = std::sscanf( line, "%lli %c\n", &_current_pos, &ch );
    if( n == 2 && _current_pos >= 0 && isstatus( ch ) )
      _current_status = Logbook::Status( ch );
    else
      {
      if( _verbosity >= 0 )
        { show_logfile_error( _filename, linenum );
          show_error( "Are you using a logfile from ddrescue 1.5 or older?" ); }
      std::exit( 2 );
      }

    while( true )
      {
      line = my_fgets( f, linenum );
      if( !line ) break;
      long long pos, size;
      n = std::sscanf( line, "%lli %lli %c\n", &pos, &size, &ch );
      if( n == 3 && pos >= 0 && ( size > 0 || size == -1 ) &&
          Sblock::isstatus( ch ) )
        {
        Sblock::Status st = Sblock::Status( ch );
        Sblock sb( pos, size, st );
        if( sblock_vector.size() > 0 && !sb.follows( sblock_vector.back() ) )
          { if( _verbosity >= 0 ) show_logfile_error( _filename, linenum );
            std::exit( 2 ); }
        sblock_vector.push_back( sb );
        }
      else
        { if( _verbosity >= 0 ) show_logfile_error( _filename, linenum );
          std::exit( 2 ); }
      }
    if( sblock_vector.size() && sblock_vector.back().size() < 0 &&
        sblock_vector.back().status() != Sblock::non_tried )
      {
      if( _verbosity >= 0 )
        { show_logfile_error( _filename, linenum );
          show_error( "Only areas of type non_tried may have an undefined size" ); }
      std::exit( 2 );
      }
    }

  std::fclose( f );
  return true;
  }


bool Logbook::check_domain_size( const long long isize ) throw()
  {
  if( isize > 0 )
    {
    if( _domain.pos() >= isize )
      { if( _verbosity >= 0 ) input_pos_error( _domain.pos(), isize );
        return false; }
    if( _domain.size() < 0  || _domain.pos() + _domain.size() > isize )
      _domain.size( isize - _domain.pos() );
    }
  return true;
  }


Logbook::Logbook( const long long pos, const long long max_size,
                  const long long isize, const char * name,
                  const int cluster, const int hardbs,
                  const int verbosity, const bool complete_only ) throw()
  : _current_pos( 0 ), _current_status( copying ),
    _domain( std::max( 0LL, pos ), max_size ), _filename( name ),
    _hardbs( hardbs ), _softbs( cluster * hardbs ), _verbosity( verbosity ),
    _final_msg( 0 ), _final_errno( 0 )
  {
  int alignment = sysconf( _SC_PAGESIZE );
  if( alignment < _hardbs || alignment % _hardbs ) alignment = _hardbs;
  if( alignment < 2 || alignment > 65536 ) alignment = 0;
  _iobuf = iobuf_base = new char[ _softbs + alignment ];
  if( alignment > 1 )		// align iobuf for use with raw devices
    {
    const int disp = alignment - ( reinterpret_cast<long> (_iobuf) % alignment );
    if( disp > 0 && disp < alignment ) _iobuf += disp;
    }

  if( !check_domain_size( isize ) ) std::exit( 1 );
  if( _filename ) read_logfile();
  if( !complete_only ) extend_sblock_vector( sblock_vector, isize, _verbosity );
  else		// limit domain to blocks of finite size read from logfile
    {
    if( sblock_vector.size() && sblock_vector.back().size() < 0 )
      sblock_vector.pop_back();
    if( sblock_vector.size() )
      {
      const Block b( sblock_vector.front().pos(),
                     sblock_vector.back().end() - sblock_vector.front().pos() );
      _domain = b.overlap( _domain );
      }
    else _domain.size( 0 );
    }
  compact_sblock_vector();
  }


bool Logbook::blank() const throw()
  {
  return ( sblock_vector.size() == 1 &&
           sblock_vector[0].status() == Sblock::non_tried );
  }


void Logbook::compact_sblock_vector() throw()
  {
  for( unsigned int i = 1; i < sblock_vector.size(); )
    {
    if( sblock_vector[i-1].join( sblock_vector[i] ) )
      sblock_vector.erase( sblock_vector.begin() + i );
    else ++i;
    }
  }


void Logbook::split_domain_border_sblocks() throw()
  {
  unsigned int i = 0;
  for( ; i < sblock_vector.size(); ++i )
    {
    Sblock & sb = sblock_vector[i];
    if( sb.includes( _domain.pos() ) )
      {
      Sblock head = sb.split( _domain.pos() );
      if( head.size() > 0 ) { insert_sblock( i, head ); ++i; }
      break;
      }
    }
  const long long end = _domain.end();
  if( end < 0 ) return;
  for( ; i < sblock_vector.size(); ++i )
    {
    Sblock & sb = sblock_vector[i];
    if( sb.includes( end ) )
      {
      Sblock head = sb.split( end );
      if( head.size() > 0 ) { insert_sblock( i, head ); ++i; }
      break;
      }
    }
  }


// Writes periodically the logfile to disc.
// Returns false only if update is attempted and fails.
//
bool Logbook::update_logfile( const int odes, const bool force ) throw()
  {
  static time_t t1 = std::time( 0 );

  if( !_filename ) return true;
  const int interval = 30 + std::min( 270, sblocks() / 40 );
  const time_t t2 = std::time( 0 );
  if( !force && t2 - t1 < interval ) return true;
  t1 = t2;
  fsync( odes );

  errno = 0;
  FILE *f = std::fopen( _filename, "w" );
  if( !f )
    {
    if( _verbosity >= 0 )
      {
      char buf[80];
      snprintf( buf, sizeof( buf ), "error opening logfile %s for writing", _filename );
      show_error( buf, errno );
      }
    return false;
    }

  write_logfile_header( f );
  std::fprintf( f, "# current_pos  current_status\n" );
  std::fprintf( f, "0x%08llX     %c\n", _current_pos, _current_status );
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
    if( _verbosity >= 0 )
      {
      char buf[80];
      snprintf( buf, sizeof( buf ), "error writing logfile %s", _filename );
      show_error( buf, errno );
      }
    return false;
    }
  return true;
  }


int Fillbook::fill_areas( const std::string & filltypes ) throw()
  {
  bool first_post = true;
  split_domain_border_sblocks();

  for( int index = 0; index < sblocks(); ++index )
    {
    const Sblock & sb = sblock( index );
    if( !sb.overlaps( domain() ) ) { if( sb < domain() ) continue; else break; }
    if( filltypes.find( sb.status() ) >= filltypes.size() ) continue;
    if( sb.end() <= current_pos() ) continue;
    Block b( sb.pos(), softbs() );
    if( sb.includes( current_pos() ) ) b.pos( current_pos() );
    if( b.end() > sb.end() ) b = b.overlap( sb );
    current_status( copying );
    while( b.size() > 0 )
      {
      if( verbosity() >= 0 )
        { show_status( b.pos(), first_post ); first_post = false; }
      const int retval = fill_block( b );
      if( retval ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      b.pos( b.pos() + softbs() );
      if( b.end() > sb.end() ) b = b.overlap( sb );
      }
    ++filled_areas; --remaining_areas;
    }
  return 0;
  }


int Fillbook::do_fill( const int odes, const std::string & filltypes ) throw()
  {
  filled_size = 0, remaining_size = 0;
  filled_areas = 0, remaining_areas = 0;
  _odes = odes;
  if( current_status() != copying || !domain().includes( current_pos() ) )
    current_pos( 0 );

  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    const Block b = sb.overlap( domain() );
    if( b.size() == 0 ) { if( sb < domain() ) continue; else break; }
    if( filltypes.find( sb.status() ) >= filltypes.size() ) continue;
    if( b.end() <= current_pos() ) { ++filled_areas; filled_size += b.size(); }
    else if( b.includes( current_pos() ) )
      {
      filled_size += current_pos() - b.pos();
      ++remaining_areas; remaining_size += b.end() - current_pos();
      }
    else { ++remaining_areas; remaining_size += b.size(); }
    }
  set_handler();
  if( verbosity() >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( filename() )
      {
      std::printf( "Initial status (read from logfile)\n" );
      std::printf( "filled size:    %10sB,", format_num( filled_size ) );
      std::printf( "  filled areas:    %7u\n", filled_areas );
      std::printf( "remaining size: %10sB,", format_num( remaining_size ) );
      std::printf( "  remaining areas: %7u\n", remaining_areas );
      std::printf( "Current status\n" );
      }
    }
  int retval = fill_areas( filltypes );
  if( verbosity() >= 0 )
    {
    show_status( -1, true );
    if( retval == 0 ) std::printf( "Finished" );
    else if( retval < 0 ) std::printf( "Interrupted by user" );
    std::fputc( '\n', stdout );
    }
  compact_sblock_vector();
  if( retval == 0 ) current_status( finished );
  else if( retval < 0 ) retval = 0;		// interrupted by user
  if( !update_logfile( _odes, true ) && retval == 0 ) retval = 1;
  if( verbosity() >= 0 && final_msg() )
    { show_error( final_msg(), final_errno() ); }
  return retval;
  }


void Rescuebook::count_errors() throw()
  {
  errors = 0;
  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    const Block b = sb.overlap( domain() );
    if( b.size() == 0 ) { if( sb < domain() ) continue; else break; }
    switch( sb.status() )
      {
      case Sblock::non_tried: break;
      case Sblock::non_trimmed:
      case Sblock::non_split: ++errors; break;
      case Sblock::bad_block: errors += b.hard_blocks( hardbs() ); break;
      case Sblock::finished: break;
      }
    }
  }


bool Rescuebook::find_valid_index( int & index, const Sblock::Status st ) const throw()
  {
  while( index < sblocks() )
    {
    const Sblock & sb = sblock( index );
    if( !sb.overlaps( domain() ) )
      { if( sb < domain() ) { ++index; continue; } else break; }
    if( sb.status() != st ) { ++index; continue; }
    return true;
    }
  return false;
  }


int Rescuebook::copy_and_update( int & index, const Sblock::Status st,
                                 const int size, bool & block_done,
                                 int & copied_size, int & error_size,
                                 const char * msg, bool & first_post ) throw()
  {
  Block & b = sblock( index );
  Block chip = b.split( b.pos() + size, hardbs() );
  if( chip.size() == 0 ) { chip = b; b.size( 0 ); block_done = true; }
  if( verbosity() >= 0 )
    { show_status( chip.pos(), msg, first_post ); first_post = false; }
  int retval = copy_block( chip, copied_size, error_size );
  if( retval ) { b.join( chip ); return retval; }
  if( copied_size > 0 )
    {
    if( index > 0 && sblock( index - 1 ).status() == Sblock::finished )
      sblock( index - 1 ).inc_size( copied_size );
    else
      { insert_sblock( index, Sblock( chip.pos(), copied_size, Sblock::finished ) );
        ++index; }
    recsize += copied_size;
    }
  if( error_size > 0 )
    {
    if( index > 0 && sblock( index - 1 ).status() == st )
      sblock( index - 1 ).inc_size( error_size );
    else
      { insert_sblock( index, Sblock( chip.pos() + copied_size, error_size, st ) );
        ++index; }
    }
  if( copied_size + error_size < chip.size() )		// EOF
    { truncate_vector( index ); block_done = true; }
  else if( block_done && sblock( index ).size() == 0 ) erase_sblock( index );
  count_errors();
  return 0;
  }


// Read the non-damaged part of the domain, skipping over the damaged areas.
//
int Rescuebook::copy_non_tried() throw()
  {
  const int skip_ini = -1;		// skip after 2 consecutive errors
  int index = 0;
  bool first_post = true;
  split_domain_border_sblocks();

  while( find_valid_index( index, Sblock::non_tried ) )
    {
    current_status( copying );
    int skip_counter = skip_ini;
    bool block_done = false;
    while( !block_done )
      {
      if( skip_counter > 0 )
        {
        Block & b = sblock( index );
        long long pos = softbs(); pos *= skip_counter; pos += b.pos();
        const Sblock chip( b.split( pos, hardbs() ), Sblock::non_trimmed );
        if( chip.size() == 0 ) skip_counter = skip_ini;	// can't skip more
        else if( index == 0 || !sblock( index - 1 ).join( chip ) )
          { insert_sblock( index, chip ); ++index; }
        errsize += chip.size();
        }
      int copied_size, error_size;
      int retval = copy_and_update( index, Sblock::non_trimmed, softbs(),
                                    block_done, copied_size, error_size,
                                    "Copying data...", first_post );
      if( error_size > 0 )		// increment counter on error
        { errsize += error_size; ++skip_counter; }
      else skip_counter = skip_ini;	// reset counter on success
      if( retval || too_many_errors() ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      }
    }
  return 0;
  }


// Trim the damaged areas backwards.
//
int Rescuebook::trim_errors() throw()
  {
  int index = 0;
  bool first_post = true;
  split_domain_border_sblocks();

  while( find_valid_index( index, Sblock::non_trimmed ) )
    {
    current_status( trimming );
    bool block_done = false;
    while( !block_done )
      {
      Block & b = sblock( index );
      Block chip = b.backsplit( b.end() - 1, hardbs() );
      if( chip.size() == 0 ) { chip = b; b.size( 0 ); block_done = true; }
      if( verbosity() >= 0 )
        { show_status( chip.pos(), "Trimming error areas...", first_post );
          first_post = false; }
      int copied_size, error_size;
      int retval = copy_block( chip, copied_size, error_size );
      if( retval ) b.join( chip );
      else
        {
        if( error_size > 0 )
          {
          sblock( index ).status( Sblock::non_split ); block_done = true;
          if( error_size == chip.size() ) b.join( chip );
          else if( index + 1 < sblocks() && sblock( index + 1 ).status() == Sblock::non_split )
            sblock( index + 1 ).dec_pos( error_size );
          else
            insert_sblock( index + 1, Sblock( chip.pos() + copied_size, error_size, Sblock::non_split ) );
          }
        if( copied_size > 0 )
          {
          if( index + 1 < sblocks() && sblock( index + 1 ).status() == Sblock::finished )
            sblock( index + 1 ).dec_pos( copied_size );
          else
            insert_sblock( index + 1, Sblock( chip.pos(), copied_size, Sblock::finished ) );
          recsize += copied_size; errsize -= copied_size;
          }
        if( copied_size + error_size < chip.size() )		// EOF
          { if( index + 1 < sblocks() ) truncate_vector( index + 1 ); }
        if( block_done && sblock( index ).size() == 0 ) erase_sblock( index );
        count_errors();
        }
      if( retval || too_many_errors() ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      }
    }
  return 0;
  }


// Try to read the damaged areas, splitting them into smaller pieces.
//
int Rescuebook::split_errors() throw()
  {
  int index = 0;
  bool first_post = true;
  split_domain_border_sblocks();

  while( find_valid_index( index, Sblock::non_split ) )
    {
    current_status( splitting );
    bool block_done = false;
    while( !block_done )
      {
      int copied_size, error_size;
      int retval = copy_and_update( index, Sblock::bad_block, hardbs(),
                                    block_done, copied_size, error_size,
                                    "Splitting error areas...", first_post );
      if( copied_size > 0 ) errsize -= copied_size;
      if( retval || too_many_errors() ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      }
    }
  return 0;
  }


int Rescuebook::copy_errors() throw()
  {
  if( _max_retries != 0 )
    {
    char msgbuf[80] = "Copying bad blocks... Retry ";
    const int msglen = std::strlen( msgbuf );
    bool resume = ( current_status() == retrying &&
                    domain().includes( current_pos() ) );

    for( int retry = 1; _max_retries < 0 || retry <= _max_retries; ++retry )
      {
      snprintf( msgbuf + msglen, sizeof( msgbuf ) - msglen, "%d", retry );
      int index = 0;
      bool first_post = true, bad_block_found = false;
      split_domain_border_sblocks();

      while( find_valid_index( index, Sblock::bad_block ) )
        {
        bad_block_found = true;
        if( resume )
          {
          Sblock & sb = sblock( index );
          if( sb.end() <= current_pos() ) { ++index; continue; }
          if( sb.includes( current_pos() ) )
            {
            Sblock head = sb.split( current_pos() );
            if( head.size() != 0 ) { insert_sblock( index, head ); ++index; }
            }
          resume = false;
          }
        current_status( retrying );
        bool block_done = false;
        while( !block_done )
          {
          int copied_size, error_size;
          int retval = copy_and_update( index, Sblock::bad_block, hardbs(),
                                        block_done, copied_size, error_size,
                                        msgbuf, first_post );
          if( copied_size > 0 ) errsize -= copied_size;
          if( retval || too_many_errors() ) return retval;
          if( !update_logfile( _odes ) ) return 1;
          }
        }
      if( !bad_block_found ) break;
      }
    }
  return 0;
  }


int Rescuebook::do_rescue( const int ides, const int odes ) throw()
  {
  bool copy_pending = false, trim_pending = false, split_pending = false;
  recsize = 0; errsize = 0;
  _ides = ides; _odes = odes;

  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    const Block b = sb.overlap( domain() );
    if( b.size() == 0 ) { if( sb < domain() ) continue; else break; }
    switch( sb.status() )
      {
      case Sblock::non_tried:   copy_pending = trim_pending = split_pending = true;
                                break;
      case Sblock::non_trimmed: trim_pending = true;	// fall through
      case Sblock::non_split:   split_pending = true;	// fall through
      case Sblock::bad_block:   errsize += b.size(); break;
      case Sblock::finished:    recsize += b.size(); break;
      }
    }
  count_errors();
  set_handler();
  if( verbosity() >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( filename() )
      {
      std::printf( "Initial status (read from logfile)\n" );
      std::printf( "rescued: %10sB,", format_num( recsize ) );
      std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
      std::printf( "  errors: %7u\n", errors );
      std::printf( "Current status\n" );
      }
    }
  int retval = 0;
  if( copy_pending && !too_many_errors() ) retval = copy_non_tried();
  if( !retval && trim_pending && !too_many_errors() ) retval = trim_errors();
  if( !retval && split_pending && !_nosplit && !too_many_errors() )
    retval = split_errors();
  if( !retval && !too_many_errors() ) retval = copy_errors();
  if( verbosity() >= 0 )
    {
    show_status( -1, (retval ? 0 : "Finished"), true );
    if( retval < 0 ) std::printf( "\nInterrupted by user" );
    else if( too_many_errors() ) std::printf("\nToo many errors in input file" );
    std::fputc( '\n', stdout );
    }
  compact_sblock_vector();
  if( retval == 0 ) current_status( finished );
  else if( retval < 0 ) retval = 0;		// interrupted by user
  if( !sync_sparse_file() )
    {
    if( verbosity() >= 0 ) show_error( "error syncing sparse output file" );
    if( retval == 0 ) retval = 1;
    }
  if( !update_logfile( _odes, true ) && retval == 0 ) retval = 1;
  if( verbosity() >= 0 && final_msg() )
    { show_error( final_msg(), final_errno() ); }
  return retval;
  }
