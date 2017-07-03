#ifndef SESSION_HPP_INCLUDED
#define SESSION_HPP_INCLUDED

//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//

#include <boost/asio.hpp>
#include <vector>
#include <list>
#include <cstddef>
#include <cstdint>

class session
{
public:

    using packet_type = std::vector<int>;

private:

    int id_;
    boost::asio::ip::tcp::endpoint ep_;

    std::uint64_t challenge_;

    std::list< packet_type > send_queue_;

private:

    static void log_( std::string const & line );

    template<class A1> void log( A1&& a1 );
    template<class A1, class A2> void log( A1&& a1, A2&& a2 );

public:

    explicit session( int id, boost::asio::ip::tcp::endpoint const & ep );

    void receive( int const * packet, std::size_t size );
    bool send_queue_pop( packet_type & p );

private:

    void cmd_get_challenge( int const * packet, int const * pend );
    void respond_unknown_packet( int const * packet, int const * pend );

    template<int N> void respond( int const * packet, int const * pend, int code, int const (&data) [N] );
};

#endif // #ifndef SESSION_HPP_INCLUDED
