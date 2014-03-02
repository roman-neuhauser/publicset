// Copyright (c) 2009 Roman Neuhauser
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <boost/regex.hpp>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <locale>
#include <memory>
#include <utility>
#include <cstddef>

namespace publicset {

namespace fs = boost::filesystem;
namespace ti = boost::posix_time;

using boost::tie;
using boost::optional;

typedef boost::regex re;
typedef optional<fs::path> maybe_path;
typedef std::pair<unsigned short, maybe_path> cmd_rv_t;

struct xclient_t
// {{{
{
    xclient_t(std::istream& cin, std::ostream& cout)
    : in_(cin), out_(cout)
    {}
    std::istream& in() { return in_; }
    std::ostream& out() { return out_; }
private:
    std::istream& in_;
    std::ostream& out_;
}; // }}}

typedef xclient_t client_t;

std::size_t
write(xclient_t client, std::istream const& in) // {{{
{
    client.out() << in.rdbuf();
    return 42;
} // }}}

namespace http {
std::istream&
getline(xclient_t& c, std::string& s) // {{{
{
    std::string l;
    auto& is(std::getline(c.in(), l));
    if ("\r" == l) {
        l = "";
    }
    s = l;
    return is;
} // }}}
}

std::map<int, std::string> status = // {{{
{
    { 200, "Ok" }
  , { 400, "Bad Request" }
  , { 404, "Not Found" }
  , { 500, "Internal Server Error" }
  , { 501, "Not Implemented" }
  , { 505, "HTTP Version Not Supported" }
}; // }}}

re
mkre(std::string const& e) // {{{
{
    return re(e, re::perl | re::icase);
} // }}}
bool
matches(std::string const& s, std::string const& sex) // {{{
{
    return static_cast<bool>(regex_match(s, mkre(sex)));
} // }}}
std::string
torfc1123(ti::ptime const& t) // {{{
{
    std::string rv("");
    std::stringstream ss(rv);
    std::unique_ptr<ti::time_facet> f(new ti::time_facet());
    f->format("%a, %d %b %Y %T GMT");
    ss.imbue(std::locale(std::locale::classic(), f.get()));
    ss << t;
    return ss.str();
} // }}}
std::string
request_date() // {{{
{
    return torfc1123(ti::second_clock::universal_time());
} // }}}
void
report_status(client_t& client, int code) // {{{
{
    std::stringstream os;
    os
        << "HTTP/1.0 " << code << " " << status[code] << "\r\n"
        << "Date: " << request_date() << "\r\n"
        << "Connection: close" << "\r\n"
    ;
    write(client, os);
} // }}}
cmd_rv_t
mkstatus(int code, maybe_path path) // {{{
{
    return std::make_pair(code, path);
} // }}}
cmd_rv_t
process_command(client_t& client, maybe_path const& docroot) // {{{
{
    std::string r;
    http::getline(client, r);

    maybe_path none;
    if (!docroot) {
        return mkstatus(500, none);
    }

    std::string method("INVALID"), path("INVALID"), proto("INVALID");
    std::stringstream rs(r);

    if (!(rs >> method) || !matches(method, "GET")) {
        return mkstatus(501, none);
    }
    if (!(rs >> path) || !matches(path, "/[-.\\w]+") || "/.." == path) {
        return mkstatus(400, none);
    }
    if (!(rs >> proto) || !matches(proto, "HTTP/1\\.[01]")) {
        return mkstatus(505, none);
    }
    path = (*docroot / path.substr(1)).string();
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        return mkstatus(404, none);
    }

    return mkstatus(200, maybe_path(path));
} // }}}
void
consume_request(client_t& client) // {{{
{
    std::string l;
    while (http::getline(client, l) && !l.empty());
} // }}}
bool
send_file(client_t& client, maybe_path const& path) // {{{
{
    if (!path) {
        return false;
    }

    std::stringstream oheaders;
    oheaders
        << "Last-Modified: "
            << torfc1123(ti::from_time_t(fs::last_write_time(*path))) << "\r\n"
        << "Content-Type: application/octet-stream" << "\r\n"
        << "\r\n"
    ;
    if (!write(client, oheaders)) {
        return false;
    }
    write(client, fs::ifstream(*path));
    return true;
} // }}}
bool
serve(client_t& client, maybe_path const& docroot) // {{{
{
    int rstatus;
    maybe_path path;

    tie(rstatus, path) = process_command(client, docroot);
    consume_request(client);
    report_status(client, rstatus);

    return send_file(client, path);
} // }}}

}

int
main(int argc, char** argv) // {{{
{
    using namespace publicset;

    maybe_path docroot;
    if (2 <= argc && fs::is_directory(argv[1])) {
        docroot = argv[1];
    }

    try {
        client_t stream(std::cin, std::cout);
        serve(stream, docroot);
    } catch (std::exception& e) {
        std::cerr << "BUG:" << e.what() << std::endl;
        return 2;
    }

    return 0;
} // }}}

// vim: et sw=4 sts=4 ff=unix fdm=marker cms=\ //\ %s
