/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Antonio Diaz Diaz.

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
#include <cstdio>
#include <string>
#include <vector>

#include "ddrescue.h"


Block::Block( const long long p, const long long s ) throw()
  {
  if( !verify( p, s ) )
    {
    std::fprintf( stderr, "p = %lld, s = %lld, p + s = %lld\n", p, s, p + s );
    internal_error( "bad parameter building a Block" );
    }
  _pos = p; _size = s;
  }


void Block::pos( const long long p ) throw()
  {
  if( !verify( p, _size ) ) internal_error( "bad pos relocating a Block" );
  _pos = p;
  }


void Block::size( const long long s ) throw()
  {
  if( !verify( _pos, s ) ) internal_error( "bad size resizing a Block" );
  _size = s;
  }


// Move end to e changing size only if needed
//
void Block::end( const long long e ) throw()
  {
  if( e < 0 ) _size = -1;
  else if( _size < 0 )
    { if( _pos < e ) _size = e - _pos; else { _pos = e; _size = 0; } }
  else { _pos = e - _size; if( _pos < 0 ) { _size += _pos; _pos = 0; } }
  }


// Align pos to next boundary if size is big enough
//
void Block::align_pos( const int hardbs ) throw()
  {
  if( hardbs > 1 )
    {
    const int disp = hardbs - ( _pos % hardbs );
    if( disp < hardbs && ( _size < 0 || _size > disp ) )
      { _pos += disp; if( _size > 0 ) _size -= disp; }
    }
  }


// Align end to previous boundary if size is finite and big enough
//
void Block::align_end( const int hardbs ) throw()
  {
  if( hardbs > 1 && _size > 0 )
    {
    const long long new_end = end() - ( end() % hardbs );
    if( _pos < new_end ) _size = new_end - _pos;
    }
  }


void Block::inc_size( const long long delta ) throw()
  {
  if( _size >= 0 )
    {
    if( _size + delta < 0 || !verify( _pos, _size + delta ) )
      internal_error( "bad delta right extending a Block" );
    _size += delta;
    }
  }


bool Block::join( const Block & b ) throw()
  {
  bool done = false;
  if( _size >= 0 && _pos + _size == b._pos )
    { if( b._size >= 0 ) _size += b._size; else _size = -1; done = true; }
  else if( b._size >= 0 && b._pos + b._size == _pos )
    { _pos = b._pos; if( _size >= 0 ) _size += b._size; done = true; }
  if( done && !verify( _pos, _size ) )
    internal_error( "size overflow joining two Blocks" );
  return done;
  }


void Block::crop( const Block & b ) throw()
  {
  const long long p = std::max( _pos, b._pos );
  long long s;
  if( _size < 0 )
    { if( b._size < 0 ) s = -1; else s = std::max( 0LL, b.end() - p ); }
  else
    {
    if( b._size < 0 ) s = std::max( 0LL, end() - p );
    else s = std::max( 0LL, std::min( end(), b.end() ) - p );
    }
  _pos = p; _size = s;
  }


Block Block::split( long long pos, const int hardbs ) throw()
  {
  if( hardbs > 1 ) pos -= pos % hardbs;
  if( _pos < pos && ( _size < 0 || _pos + _size > pos ) )
    {
    const Block b( _pos, pos - _pos );
    _pos = pos; if( _size > 0 ) _size -= b._size;
    return b;
    }
  return Block();
  }


Sblock::Sblock( const Block & b, const Status st ) throw()
  : Block( b )
  {
  if( isstatus( st ) ) _status = st;
  else internal_error( "bad status building a Sblock" );
  }


Sblock::Sblock( const long long p, const long long s, const Status st ) throw()
  : Block( p, s )
  {
  if( isstatus( st ) ) _status = st;
  else internal_error( "bad status building a Sblock" );
  }


void Sblock::status( const Status st ) throw()
  {
  if( isstatus( st ) ) _status = st;
  else internal_error( "bad status change in a Sblock" );
  }
