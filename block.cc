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

#include <algorithm>
#include <climits>
#include <cstdio>
#include <string>
#include <vector>

#include "block.h"
#include "ddrescue.h"


namespace {

void input_pos_error( const long long pos, const long long isize )
  {
  char buf[80];
  snprintf( buf, sizeof buf, "can't start reading at pos %lld", pos );
  show_error( buf );
  snprintf( buf, sizeof buf, "input file is only %lld bytes long", isize );
  show_error( buf );
  }

} // end namespace


// Align pos to next boundary if size is big enough
//
void Block::align_pos( const int hardbs ) throw()
  {
  if( hardbs > 1 )
    {
    const int disp = hardbs - ( _pos % hardbs );
    if( disp < hardbs && _size > disp )
      { _pos += disp; _size -= disp; }
    }
  }


// Align end to previous boundary if size is big enough
//
void Block::align_end( const int hardbs ) throw()
  {
  if( hardbs > 1 && _size > 0 )
    {
    const long long new_end = end() - ( end() % hardbs );
    if( _pos < new_end ) _size = new_end - _pos;
    }
  }


void Block::crop( const Block & b ) throw()
  {
  const long long p = std::max( _pos, b._pos );
  const long long s = std::max( 0LL, std::min( end(), b.end() ) - p );
  _pos = p; _size = s;
  }


bool Block::join( const Block & b )
  {
  if( this->follows( b ) ) _pos = b._pos;
  else if( !b.follows( *this ) ) return false;
  _size += b._size;
  if( _size < 0 || _size > LLONG_MAX - _pos )
    internal_error( "size overflow joining two Blocks" );
  return true;
  }


Block Block::split( long long pos, const int hardbs )
  {
  if( hardbs > 1 ) pos -= pos % hardbs;
  if( _pos < pos && end() > pos )
    {
    const Block b( _pos, pos - _pos );
    _pos = pos; _size -= b._size;
    return b;
    }
  return Block( 0, 0 );
  }


void Domain::crop( const Block & b )
  {
  for( int i = block_vector.size() - 1; i >= 0; --i )
    {
    block_vector[i].crop( b );
    if( block_vector[i].size() <= 0 )
      block_vector.erase( block_vector.begin() + i );
    }
  }


bool Domain::crop_by_file_size( const long long isize )
  {
  if( isize > 0 )
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      {
      if( block_vector[i].pos() >= isize )
        {
        if( i == 0 )
          { input_pos_error( block_vector[i].pos(), isize ); return false; }
        block_vector.erase( block_vector.begin() + i, block_vector.end() );
        break;
        }
      if( block_vector[i].end() > isize )
        block_vector[i].size( isize - block_vector[i].pos() );
      }
  return true;
  }
