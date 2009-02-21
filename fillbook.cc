/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Antonio Diaz Diaz.

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

#include <climits>
#include <cstdio>
#include <string>
#include <vector>

#include "block.h"
#include "ddrescue.h"


int Fillbook::fill_areas( const std::string & filltypes )
  {
  bool first_post = true;

  for( int index = 0; index < sblocks(); ++index )
    {
    const Sblock & sb = sblock( index );
    if( !domain().includes( sb ) ) { if( domain() < sb ) break; else continue; }
    if( sb.end() <= current_pos() ||
        filltypes.find( sb.status() ) >= filltypes.size() ) continue;
    Block b( sb.pos(), softbs() );
    if( sb.includes( current_pos() ) ) b.pos( current_pos() );
    if( b.end() > sb.end() ) b.crop( sb );
    current_status( filling );
    while( b.size() > 0 )
      {
      if( verbosity >= 0 )
        { show_status( b.pos(), first_post ); first_post = false; }
      const int retval = fill_block( b );
      if( retval ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      b.pos( b.end() );
      if( b.end() > sb.end() ) b.crop( sb );
      }
    ++filled_areas; --remaining_areas;
    }
  return 0;
  }


int Fillbook::do_fill( const int odes, const std::string & filltypes )
  {
  filled_size = 0, remaining_size = 0;
  filled_areas = 0, remaining_areas = 0;
  _odes = odes;
  if( current_status() != filling || !domain().includes( current_pos() ) )
    current_pos( 0 );

  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) ) { if( domain() < sb ) break; else continue; }
    if( filltypes.find( sb.status() ) >= filltypes.size() ) continue;
    if( sb.end() <= current_pos() ) { ++filled_areas; filled_size += sb.size(); }
    else if( sb.includes( current_pos() ) )
      {
      filled_size += current_pos() - sb.pos();
      ++remaining_areas; remaining_size += sb.end() - current_pos();
      }
    else { ++remaining_areas; remaining_size += sb.size(); }
    }
  set_handler();
  if( verbosity >= 0 )
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
  if( verbosity >= 0 )
    {
    show_status( -1, true );
    if( retval == 0 ) std::printf( "Finished" );
    else if( retval < 0 ) std::printf( "Interrupted by user" );
    std::fputc( '\n', stdout );
    }
  if( retval == 0 ) current_status( finished );
  else if( retval < 0 ) retval = 0;		// interrupted by user
  compact_sblock_vector();
  if( !update_logfile( _odes, true ) && retval == 0 ) retval = 1;
  if( final_msg() ) show_error( final_msg(), final_errno() );
  return retval;
  }
