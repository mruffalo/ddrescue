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

class Block
  {
  long long _pos, _size;		// size == -1 means undefined size

public:
  Block() throw() : _pos( 0 ), _size( 0 ) {}
  Block( const long long p, const long long s ) throw();

  long long pos() const throw() { return _pos; }
  long long size() const throw() { return _size; }
  long long end() const throw() { return (_size < 0) ? -1 : _pos + _size; }
  long long hard_blocks( const int hardbs ) const throw();

  void pos( const long long p ) throw();
  void size( const long long s ) throw();

  bool operator<( const Block & b ) const throw() { return _pos < b._pos; }
  bool can_be_split( const int hardbs ) const throw();
  bool follows( const Block & b ) const throw();
  bool includes( const long long pos ) const throw();
  bool overlaps( const Block & b ) const throw();

  bool join( const Block & b ) throw();
  Block overlap( const Block & b ) const throw();
  Block split( const long long pos ) throw();
  Block split_hb( const int hardbs ) throw();
  };


class Sblock : public Block
  {
public:
  enum Status
    { non_tried = '?', non_split = '/', bad_block = '-', done = '+' };
private:
  Status _status;

public:
  Sblock( const Block & b, const Status st ) throw();
  Sblock( const long long p, const long long s, const Status st ) throw();

  Status status() const throw() { return _status; }
  void status( const Status st ) throw();

  bool join( const Sblock & sb ) throw();
  Sblock split( const long long pos ) throw()
    { return Sblock( Block::split( pos ), _status ); }
  static bool isstatus( const int st ) throw();
  };


class Logbook
  {
  long long _offset;			// rescue offset (opos - ipos);
  Block _domain;			// rescue domain
  char *iobuf_base, *iobuf;		// iobuf is aligned to page and hardbs
  const char *filename;
  const int _hardbs, _softbs, _max_errors, _max_retries, _verbosity;
  long long recsize, errsize;		// total recovered and error sizes
  int errors;				// errors found so far
  int _ides, _odes;			// input and output file descriptors
  const bool _nosplit;
  std::vector< Sblock > sblock_vector;	// note: blocks are consecutive

  void set_rescue_domain( const long long ipos, const long long opos,
                          const long long max_size, const long long isize ) throw();
  bool read_logfile() throw();
  int copy_non_tried_block( const Block & block, std::vector< Sblock > & result ) throw();
  int copy_bad_block( const Block & block, std::vector< Sblock > & result ) throw();
  void count_errors() throw();
  int copy_non_tried() throw();
  int split_errors() throw();
  int copy_errors() throw();

public:
  Logbook( const long long ipos, const long long opos,
           const long long max_size, const long long isize,
           const char * name, const int cluster, const int hardbs,
           const int max_errors, const int max_retries, const int verbosity,
           const bool complete_only, const bool nosplit ) throw();
  ~Logbook() throw() { delete[] iobuf_base; }

  long long rescue_ipos() const throw() { return _domain.pos(); }
  long long rescue_opos() const throw() { return _domain.pos() + _offset; }
  long long rescue_size() const throw() { return _domain.size(); }
  bool blank() const throw();

  int do_rescue( const int ides, const int odes ) throw();
  };


// Defined in ddrescue.cc
//
const char * format_num( long long num, long long max = 999999,
                         const int set_prefix = 0 ) throw();
void set_handler() throw();
void show_status( const long long ipos, const long long opos,
                  const long long recsize, const long long errsize,
                  const int errors, const char * msg = 0, bool force = false ) throw();


// Defined in main.cc
//
void internal_error( const char * msg ) throw() __attribute__ ((noreturn));
void show_error( const char * msg,
                 const int errcode = 0, const bool help = false ) throw();
void write_logfile_header( FILE * f ) throw();
