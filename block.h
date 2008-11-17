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

class Block
  {
  long long _pos, _size;		// size == -1 means undefined size

  static bool verify( const long long p, const long long s ) throw()
    { return ( p >= 0 && s >= -1 && ( s < 0 || p + s >= 0 ) ); }

public:
  Block( const long long p, const long long s ) throw();

  long long pos() const throw() { return _pos; }
  long long size() const throw() { return _size; }
  long long end() const throw() { return (_size < 0) ? -1 : _pos + _size; }

  void pos( const long long p ) throw();
  void size( const long long s ) throw();
  void end( const long long e ) throw();
  void align_pos( const int hardbs ) throw();
  void align_end( const int hardbs ) throw();
  void inc_size( const long long delta ) throw();

  bool follows( const Block & b ) const throw()
    { return ( b._size >= 0 && b._pos + b._size == _pos ); }
  bool includes( const Block & b ) const throw()
    { return ( _pos <= b._pos &&
               ( _size < 0 || ( b._size >= 0 && _pos + _size >= b._pos + b._size ) ) ); }
  bool includes( const long long pos ) const throw()
    { return ( _pos <= pos && ( _size < 0 || _pos + _size > pos ) ); }

  bool join( const Block & b ) throw();
  void crop( const Block & b ) throw();
  Block split( long long pos, const int hardbs = 1 ) throw();
  };


class Sblock : public Block
  {
public:
  enum Status
    { non_tried = '?', non_trimmed = '*', non_split = '/', bad_block = '-',
      finished = '+' };
private:
  Status _status;

public:
  Sblock( const Block & b, const Status st ) throw();
  Sblock( const long long p, const long long s, const Status st ) throw();

  Status status() const throw() { return _status; }
  void status( const Status st ) throw();

  bool join( const Sblock & sb ) throw()
    { if( _status == sb._status ) return Block::join( sb ); else return false; }
  Sblock split( const long long pos, const int hardbs = 1 ) throw()
    { return Sblock( Block::split( pos, hardbs ), _status ); }
  static bool isstatus( const int st ) throw()
    { return ( st == non_tried || st == non_trimmed || st == non_split ||
               st == bad_block || st == finished ); }
  };


class Domain
  {
  std::vector< Block > block_vector;	// blocks are ordered and don't overlap

public:
  Domain( const char * name, const long long p, const long long s ) throw();

  long long pos() const throw()
    { if( block_vector.size() ) return block_vector[0].pos(); else return 0; }

  long long size() const throw()
    {
    long long s = 0;
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      {
      if( block_vector[i].size() < 0 ) return -1;
      else s += block_vector[i].size();
      }
    return s;
    }

  bool operator<( const Block & b ) const throw()
    { return ( block_vector.size() && block_vector.back().size() >= 0 &&
               block_vector.back().end() <= b.pos() ); }

  long long breaks_block_by( const Block & b ) const throw()
    {
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      {
      const Block & db = block_vector[i];
      if( b.includes( db.pos() ) && b.pos() < db.pos() ) return db.pos();
      const long long end = db.end();
      if( end > 0 && b.includes( end ) && b.pos() < end ) return end;
      }
    return 0;
    }

  bool includes( const Block & b ) const throw()
    {
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      if( block_vector[i].includes( b ) ) return true;
    return false;
    }

  bool includes( const long long pos ) const throw()
    {
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      if( block_vector[i].includes( pos ) ) return true;
    return false;
    }

  void clear() throw() { block_vector.clear(); }
  void crop( const Block & b ) throw();
  bool crop_by_file_size( const long long isize ) throw();
  };
