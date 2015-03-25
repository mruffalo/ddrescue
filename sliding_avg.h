/*  Sliding_average - Calculates the average of the last N terms
    Copyright (C) 2015 Antonio Diaz Diaz.

    This library is free software: you have unlimited permission to
    copy, distribute and modify it.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

class Sliding_average
  {
  unsigned index;
  std::vector<long long> data;

public:
  Sliding_average( const unsigned n ) : index( n ) { data.reserve( n ); }

  void reset() { if( index < data.size() ) index = data.size(); data.clear(); }

  void add_term( const long long term )
    {
    if( index < data.size() ) data[index++] = term;
    else if( index > data.size() ) data.push_back( term );
    if( index == data.size() ) index = 0;
    }

  long long operator()() const
    {
    long long avg = 0;
    for( unsigned i = 0; i < data.size(); ++i ) avg += data[i];
    if( data.size() ) avg /= data.size();
    return avg;
    }
  };
