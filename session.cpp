//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//

#define _WIN32_WINNT 0x0501

#include "session.hpp"
#include <boost/random.hpp>

// packet

static inline int packet_cid( int const * packet )
{
    return packet[0];
}

static inline int packet_seq( int const * packet )
{
    return packet[1];
}

static inline int packet_code( int const * packet )
{
    return packet[2];
}

static inline int packet_len( int const * packet )
{
    return packet[3];
}

static inline int const * packet_data( int const * packet )
{
    return packet + 4;
}

int const cmd_get_challenge = 0x2700;
int const response_challenge = 0x2700;

int const response_unknown_packet = -1;

// session

void session::log_( std::string const & line )
{
    std::fputs( line.c_str(), stderr );
    std::fflush( stderr );
}

template<class A1> void session::log( A1&& a1 )
{
    std::ostringstream os;

    os << "[#" << id_ << ' ' << ep_ << "] " << a1 << std::endl;

    log_( os.str() );
}

template<class A1, class A2> void session::log( A1&& a1, A2&& a2 )
{
    std::ostringstream os;

    os << "[#" << id_ << ' ' << ep_ << "] " << a1 << ' ' << a2 << std::endl;

    log_( os.str() );
}

//

static boost::mt19937_64 s_engine;

session::session( int id, boost::asio::ip::tcp::endpoint const & ep ): id_( id ), ep_( ep ), challenge_( s_engine() )
{
}

void session::receive( int const * packet, std::size_t size )
{
    int const * pend = packet + size / 4;

    if( size < 16 || size % 4 != 0 || packet_len( packet ) != size - 16 )
    {
        log( "Discarded bad packet with size ", size );
        return;
    }

    int code = packet_code( packet );

    switch( code )
    {
    case ::cmd_get_challenge:

        cmd_get_challenge( packet, pend );
        break;

    default:

        respond_unknown_packet( packet, pend );
        break;
    }
}

bool session::send_queue_pop( packet_type & p )
{
    if( send_queue_.empty() )
    {
        return false;
    }
    else
    {
        p = send_queue_.front();
        send_queue_.pop_front();

        return true;
    }
}

//

template<int N> void session::respond( int const * packet, int const * /*pend*/, int code, int const (&data) [N] )
{
    std::vector<int> p;

    p.reserve( N + 4 );

    p.push_back( packet[0] );
    p.push_back( packet[1] );
    p.push_back( code );
    p.push_back( N * 4 );
    p.insert( p.end(), data, data + N );

    send_queue_.push_back( std::move( p ) );
}

void session::cmd_get_challenge( int const * packet, int const * pend )
{
    int data[] = { static_cast<std::uint32_t>( challenge_ & 0xFFFFFFFFull ), static_cast<std::uint32_t>( challenge_ >> 32 ) };
    respond( packet, pend, response_challenge, data );
}

void session::respond_unknown_packet( int const * packet, int const * pend )
{
    int data[] = { packet_code( packet ) };
    respond( packet, pend, response_unknown_packet, data );
}
