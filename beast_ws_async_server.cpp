//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//

#define _WIN32_WINNT 0x0501

#include "session.hpp"

#include <beast/core.hpp>
#include <beast/websocket.hpp>
#include <beast/version.hpp>

#include <boost/asio.hpp>

#include <iostream>
#include <sstream>
#include <memory>
#include <cstdio>

namespace ip = boost::asio::ip;
using tcp = boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace http = beast::http;
namespace websocket = beast::websocket;

// connection

class connection: public std::enable_shared_from_this<connection>
{
private:

    static int next_id_;

    int id_ = ++next_id_;

    websocket::stream<tcp::socket> ws_;
    tcp::endpoint ep_;

    beast::flat_buffer buffer_;

    std::shared_ptr<session> ps_;
    session::packet_type packet_;

private:

    static void log_( std::string const & line )
    {
        std::fputs( line.c_str(), stderr );
        std::fflush( stderr );
    }

    template<class A1> void log( A1&& a1 )
    {
        std::ostringstream os;

        os << "[#" << id_ << ' ' << ep_ << "] " << a1 << std::endl;

        log_( os.str() );
    }

    template<class A1, class A2> void log( A1&& a1, A2&& a2 )
    {
        std::ostringstream os;

        os << "[#" << id_ << ' ' << ep_ << "] " << a1 << ' ' << a2 << std::endl;

        log_( os.str() );
    }

public:

    explicit connection( tcp::socket&& socket ): ws_( std::move(socket) ), ep_( ws_.next_layer().remote_endpoint() ), ps_( std::make_shared<session>( id_, ep_ ) )
    {
        log( "Connected" );
    }

    ~connection()
    {
        log( "Disconnected" );
    }

    void run()
    {
        websocket::permessage_deflate pmd; 

        pmd.client_enable = true; 
        pmd.server_enable = true; 
        pmd.compLevel = 3; 
        
        ws_.auto_fragment( false );
        ws_.set_option( pmd );
        ws_.read_message_max( 65536 );

        ws_.binary( true );

        ws_.async_accept( [self = shared_from_this()](auto ec){ self->on_accept( ec ); } );
    }

private:

    void on_accept( beast::error_code ec )
    {
        if( ec )
        {
            log( "Accept error:", ec.message() );
        }
        else
        {
            log( "Accepted" );
            ws_.async_read( buffer_, [self = shared_from_this()](auto ec){ self->on_read( ec ); } );
        }
    }

    void on_read( beast::error_code ec )
    {
        if( ec == websocket::error::closed )
        {
            log( "Connection closed" );
        }
        else if( ec )
        {
            log( "Read error:", ec.message() );
        }
        else
        {
            auto cb = buffer_.data();

            int const* p = asio::buffer_cast<int const*>( cb );
            std::size_t n = asio::buffer_size( cb );

            if( ws_.got_binary() )
            {
                log( "Received message with size", n );
                ps_->receive( p, n );
            }
            else
            {
                log( "Ignoring text message with size", n );
            }

            buffer_.consume( n );

            read_or_write();
        }
    }

    void read_or_write()
    {
        if( ps_->send_queue_pop( packet_ ) )
        {
            asio::const_buffers_1 cb1( packet_.data(), packet_.size() * 4 );

            log( "Sending message with size", packet_.size() * 4 );
            ws_.async_write( cb1, [self = shared_from_this()](auto ec){ self->on_write( ec ); } );
        }
        else
        {
            ws_.async_read( buffer_, [self = shared_from_this()](auto ec){ self->on_read( ec ); } );
        }
    }

    void on_write( beast::error_code ec )
    {
        if( ec )
        {
            log( "Write error:", ec.message() );
        }
        else
        {
            read_or_write();
        }
    }
};

int connection::next_id_ = 0;

//

static void on_accept( beast::error_code ec, tcp::socket& socket, tcp::acceptor& acceptor )
{
    if( ec )
    {
        std::cerr << "Accept error: " << ec.message() << std::endl;
    }
    else
    {
        std::make_shared<connection>( std::move(socket) )->run();
    }

    socket.close();
    acceptor.async_accept( socket, [&]( auto ec ){ on_accept( ec, socket, acceptor ); } );
}

int main()
{
    try
    {
        auto address = tcp::v4();
        unsigned short port = 6502;

        boost::asio::io_service ios;

        tcp::acceptor acceptor{ ios, { address, port } };

        tcp::socket socket( ios );

        acceptor.async_accept( socket, [&]( auto ec ){ on_accept( ec, socket, acceptor ); });

        ios.run();
    }
    catch( const std::exception& e )
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
