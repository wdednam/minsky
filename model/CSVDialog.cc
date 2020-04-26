/*
  @copyright Steve Keen 2018
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "CSVDialog.h"
#include "group.h"
#include "selection.h"
#include <pango.h>
#include "minsky_epilogue.h"

//#include <boost/beast/example/common/root_certificates.hpp>
//#include "root_certificates.hpp"

//#define BOOST_BEAST_ALLOW_DEPRECATED
#include <boost/beast/core.hpp>      //"beast/core.hpp"       
#include <boost/beast/http.hpp>      //"beast/http.hpp"       
#include <boost/beast/version.hpp>   //"beast/version.hpp"    

//#include "certify/extensions.hpp"         //<boost/certify/extensions.hpp>
//#include "certify/https_verification.hpp" //<boost/certify/https_verification.hpp>

  
#include "certify/include/boost/certify/extensions.hpp"         
#include "certify/include/boost/certify/https_verification.hpp" 



//#include <boost/asio/connect.hpp>
//#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/categories.hpp> // input_filter_tag
#include <boost/iostreams/operations.hpp> // get
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <zlib.h>

using namespace std;
using namespace minsky;
using ecolab::Pango;
using ecolab::cairo::CairoSave;
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
namespace boostio = boost::iostreams;


void CSVDialog::reportFromFile(const std::string& input, const std::string& output)
{
  ifstream is(input);
  ofstream of(output);
  reportFromCSVFile(is,of,spec);
}

// Performs an HTTP GET and prints the response
//void CSVDialog::loadWebFile(int argc, char** argv)
// Return file name after downloading a CSV file from the web.
std::string CSVDialog::loadWebFile(const std::string& url)
{
  // Parse input URL. Also handles URLs of the type username:password@example.com/pathname#section. See https://stackoverflow.com/questions/2616011/easy-way-to-parse-a-url-in-c-cross-platform
  boost::regex ex(R"((http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\??([^ #]*)#?([^ ]*)|^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)");
  boost::cmatch what;
  if (regex_match(url.c_str(), what, ex)) {
   // what[0] contains the whole string 	 
   // what[1] is the protocol
   // what[2] is the domain  
   // what[3] is the port    
   // what[4] is the path    
   // what[5] is the query   
   // what[6] is the fragment		  
  } else throw runtime_error("Failure to match URL: "+url);
         
  auto const protocol =what[1];
  auto const host = what[2];
  auto const port = what[3];
  auto const target = what[4];
  auto const query = what[5];
  auto const fragment = what[6];  
  
  // The io_context is required for all I/O
  boost::asio::io_context ioc;
  
  // The SSL context is required, and holds certificates
  ssl::context ctx{ssl::context::tls_client};
  
  // Verify the remote server's certificate. See https://github.com/djarek/certify/blob/master/examples/get_page.cpp
  ctx.set_verify_mode(ssl::verify_peer | ssl::context::verify_fail_if_no_peer_cert);    
  ctx.set_default_verify_paths();
  
  // tag::ctx_setup_source[]
  boost::certify::enable_native_https_server_verification(ctx);
  // end::ctx_setup_source[]        
  		        
  // These objects perform our I/O
  tcp::resolver resolver{ioc};
  ssl::stream<tcp::socket> stream{ioc, ctx};        
  
  // tag::stream_setup_source[]. See https://github.com/djarek/certify/blob/master/examples/get_page.cpp
  boost::certify::set_server_hostname(stream, host.str());
  boost::certify::sni_hostname(stream, host);
  // end::stream_setup_source[]         

  // Look up the domain name
  auto const results = resolver.resolve(host, protocol);
          
  // Make the connection on the IP address we get from a lookup
  boost::asio::connect(stream.next_layer(), results.begin(), results.end());                   
       
  // Perform the SSL handshake
  stream.handshake(ssl::stream_base::client);             

  // Set up an HTTP GET request message
  http::request<http::string_body> req;
  req.method(http::verb::get);  
  req.target(target.str());     
  req.version(10);
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  // Send the HTTP request to the remote host
  http::write(stream, req);

  // This buffer is used for reading and must be persisted
  boost::beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response_parser<http::string_body> res;
  res.body_limit((std::numeric_limits<std::uint64_t>::max)());

  // Receive the HTTP response
  http::read(stream, buffer, res);
  
  res.eager(true);  // See https://github.com/boostorg/beast/issues/1352
  // Check response status and throw error all values 400 and above. See https://www.boost.org/doc/libs/master/boost/beast/http/status.hpp for status codes
  if (res.get().result_int() >= 400) throw runtime_error("Invalid HTTP response. Response code: " + std::to_string(res.get().result_int()));
  
  //classdesc::pack_t& b;
  //std::ofstream x;
  //b>>a.name>>a.dimension>>size;
  //b>>x;
  //a.push_back(x);  
                                                 
  // Dump the outstream into a temporary file for loading it into Minsky' CSV parser 
  boost::filesystem::path temp = boost::filesystem::unique_path();
  const std::string tempStr    = temp.string();    
  std::ofstream outFile(tempStr, std::ofstream::out);
  outFile << res.get().body();                                            
  
  // Deal with zipped csv files
  //if (fragment.str().find(".zip")) {
  //    std::ifstream file(temp.native(), ios_base::in | ios_base::binary);
  //    boostio::filtering_streambuf<boostio::input> in;
  //    in.push(boostio::zlib_decompressor());
  //    in.push(file);
  //    boostio::copy(in, outFile);
  //}// else outFile << res.get().body();                                            
       
  // Gracefully close the socket
  boost::system::error_code ec;
  stream.shutdown(ec);
  if (ec == boost::asio::error::eof)
  {
      // Rationale:
      // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
      ec.assign(0, ec.category());
  }
  if (ec)
      throw boost::system::system_error{ec}; 

  // If we get here then the connection is closed gracefully         

  // Return the file name for loading the in csvimport.tcl 
  return tempStr;
}

//void CSVDialog::loadWebFile(const char *argv) {
//void CSVDialog::loadWebFile(const string& url) {
//    try {
//
//        boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");        
//        boost::cmatch what;
//        if(regex_match(url.c_str(), what, ex)) 
//        {
//            cout << "protocol: " << string(what[1].first, what[1].second) << endl;
//            cout << "domain:   " << string(what[2].first, what[2].second) << endl;
//            cout << "port:     " << string(what[3].first, what[3].second) << endl;
//            cout << "path:     " << string(what[4].first, what[4].second) << endl;
//            cout << "query:    " << string(what[5].first, what[5].second) << endl;
//            cout << "fragment: " << string(what[6].first, what[6].second) << endl;  
//        }		
//		
//        boost::asio::io_service io_service;
//        // Get a list of endpoints corresponding to the server name.
//        tcp::resolver resolver(io_service);
//        //tcp::resolver::query query(argv, "http");
//        tcp::resolver::query query(what[2], what[1]);
//        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
//        tcp::resolver::iterator end;
//        // Try each endpoint until we successfully establish a connection.
//        tcp::socket socket(io_service);     
//                
//        boost::system::error_code error = boost::asio::error::host_not_found;
//        while (error && endpoint_iterator != end) {
//            socket.close();
//            socket.connect(*endpoint_iterator++, error);
//        }
//        if (error)
//            throw boost::system::system_error(error);   
//            
//        //// The SSL context is required, and holds certificates
//        //ssl::context ctx{ssl::context::sslv23_client};
//        ////
//        //// This holds the root certificate used for verification
//        //load_root_certificates(ctx);
//        ////
//        //// Verify the remote server's certificate
//        //ctx.set_verify_mode(ssl::verify_peer);    
//        // 
//        //ssl::stream<tcp::socket> stream{io_service, ctx};
//        //
//        //// Set SNI Hostname (many hosts need this to handshake successfully)
//        //if(! SSL_set_tlsext_host_name(stream.native_handle(), what[2].first))
//        //{
//        //    boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
//        //    throw boost::system::system_error{ec};
//        //}                        
//        //     
//        //// Perform the SSL handshake
//        //stream.handshake(ssl::stream_base::client);            
//
//        
//        // Form the request. We specify the "Connection: close" header so that the
//        // server will close the socket after transmitting the response. This will
//        // allow us to treat all data up until the EOF as the content.
//        boost::asio::streambuf request;
//        std::ostream request_stream(&request);
//        request_stream << "GET " << what[4] << " HTTP/1.0\r\n";
//        request_stream << "Host: " << what[2] << "\r\n";
//        request_stream << "Accept: */*\r\n";
//        request_stream << "Connection: close\r\n\r\n";
//        
//                
//        // Send the request.
//        boost::asio::write(socket, request);
//        // Read the response status line.
//        boost::asio::streambuf response;
//        boost::asio::read_until(socket, response, "\r\n");
//        // Check that response is OK.
//        std::istream response_stream(&response);
//        std::string http_version;
//        response_stream >> http_version;
//        unsigned int status_code;
//        response_stream >> status_code;
//        std::string status_message;
//        std::getline(response_stream, status_message);
//        if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
//            //std::cerr << "Invalid response\n";
//            return;
//        }
//        if (status_code != 200) {
//            //std::cerr << "Response returned with status code " << status_code << "\n";
//            return;
//        }
//
//        // Read the response headers, which are terminated by a blank line.
//        boost::asio::read_until(socket, response, "\r\n\r\n");
//
//        // Process the response headers.
//        std::string header;
//
//        while (std::getline(response_stream, header) && header != "\r")
//            std::cerr << header << "\n";
//
//        std::ostringstream ss;
//        
//        FILE * tempFile;
//        char * fileptr;
//        fileptr = tmpnam(NULL);        
//        tempFile = fopen(fileptr,"wb+");        
//        
//        // Extract the file name and extension
//        //boost::filesystem::path p(string(what[4].first, what[4].second));        
//        //cout << "filename and extension : " << p.filename() << std::endl; // file.ext
//        //cout << "filename only          : " << p.stem() << std::endl;     // file   
//        
//        // Dump the outstream into a CSV file and load into CSVParser
//        //std::ofstream outFile(p.filename().c_str(), std::ofstream::out);        
//        
//
//        std::cerr << "\n";
//        std::cerr << "Writing content data\n";
//        
//        //std::string line;
//        //while (std::getline(response_stream, line,'\n'))        
//        //    outFile << line;        
//        
//        // Write whatever content we already have to output.
//        if (response.size() > 0) {
//		  ss << &response << "\n";
//        }            
//
//        // Read until EOF, writing data to output as we go.
//        //std::string line;          
//        while (true) { 
//            size_t n = boost::asio::read(socket, response, boost::asio::transfer_at_least(1), error);
//		
//            if (!error)
//            {
//                if (n) {					
//					//outFile << &response << "\n";
//                    ss << &response << "\n";
//			   }
//            }
//		
//            if (error == boost::asio::error::eof)
//                break;
//		
//            if (error)
//                throw boost::system::system_error(error);
//        }
//        
//        fputs(ss.str().data(),tempFile); 
//        
//        spec=DataSpec();
//        //spec.guessFromFile(p.filename().c_str());
//        //ifstream is(p.filename().c_str());
//        spec.guessFromFile(fileptr);
//        ifstream is(fileptr);
//        initialLines.clear();
//        for (size_t i=0; i<numInitialLines && is; ++i)
//          {
//            initialLines.emplace_back();
//            getline(is, initialLines.back());
//          }
//        // Ensure dimensions.size() is the same as nColAxes() upon first load of a CSV file. For ticket 974.
//        if (spec.dimensions.size()<spec.nColAxes()) spec.setDataArea(spec.nRowAxes(),spec.nColAxes());    
//        
//        // close the output file
//        //outFile.close();
//        fclose (tempFile);      
//        
//    }
//    catch (std::exception &e) {
//        std::cerr << "Exception: " << e.what() << "\n";
//    }      
//
//    //std::cerr << "Done\n";
//    //return 0;
//}

void CSVDialog::loadFile(const string& fname)
{
  spec=DataSpec();
  spec.guessFromFile(fname);
  ifstream is(fname);
  initialLines.clear();
  for (size_t i=0; i<numInitialLines && is; ++i)
    {
      initialLines.emplace_back();
      getline(is, initialLines.back());
    }
  // Ensure dimensions.size() is the same as nColAxes() upon first load of a CSV file. For ticket 974.
  if (spec.dimensions.size()<spec.nColAxes()) spec.setDataArea(spec.nRowAxes(),spec.nColAxes());    
}

template <class Parser>
vector<vector<string>> parseLines(const Parser& parser, const vector<string>& lines)
{
  vector<vector<string>> r;
  for (auto& line: lines)
    {
      r.emplace_back();
      boost::tokenizer<Parser> tok(line.begin(), line.end(), parser);
      for (auto& t: tok)
        r.back().push_back(t);
    }
  return r;
}

namespace
{
  struct CroppedPango: public Pango
  {
    cairo_t* cairo;
    double w, x=0, y=0;
    CroppedPango(cairo_t* cairo, double width): Pango(cairo), cairo(cairo), w(width) {}
    void setxy(double xx, double yy) {x=xx; y=yy;}
    void show() {
      CairoSave cs(cairo);
      cairo_rectangle(cairo,x,y,w,height());
      cairo_clip(cairo);
      cairo_move_to(cairo,x,y);
      Pango::show();
    }
  };
}

void CSVDialog::redraw(int, int, int width, int height)
{
  cairo_t* cairo=surface->cairo();
  CroppedPango pango(cairo, colWidth);
  rowHeight=15;
  pango.setFontSize(0.8*rowHeight);
  
  vector<vector<string>> parsedLines=parseLines();
  
  // LHS row labels
  {
    Pango pango(cairo);
    pango.setText("Dimension");
    cairo_move_to(cairo,xoffs-pango.width()-5,0);
    pango.show();
    pango.setText("Type");
    cairo_move_to(cairo,xoffs-pango.width()-5,rowHeight);
    pango.show();
    pango.setText("Format");
    cairo_move_to(cairo,xoffs-pango.width()-5,2*rowHeight);
    pango.show();
    if (flashNameRow)
      pango.setMarkup("<b>Name</b>");
    else
      pango.setText("Name");
    cairo_move_to(cairo,xoffs-pango.width()-5,3*rowHeight);
    pango.show();
    pango.setText("Header");
    cairo_move_to(cairo,xoffs-pango.width()-5,(4+spec.headerRow)*rowHeight);
    pango.show();
    
  }	
  
  set<size_t> done;
  double x=xoffs, y=0;
  size_t col=0;
  for (; done.size()<parsedLines.size(); ++col)
    {
      if (col<spec.nColAxes())
        {// dimension check boxes
          CairoSave cs(cairo);
          double cbsz=5;
          cairo_set_line_width(cairo,1);
          cairo_translate(cairo,x+0.5*colWidth,y+0.5*rowHeight);
          cairo_rectangle(cairo,-cbsz,-cbsz,2*cbsz,2*cbsz);
          if (spec.dimensionCols.count(col))
            {
              cairo_move_to(cairo,-cbsz,-cbsz);
              cairo_line_to(cairo,cbsz,cbsz);
              cairo_move_to(cairo,cbsz,-cbsz);
              cairo_line_to(cairo,-cbsz,cbsz);
            }
          cairo_stroke(cairo);
        }
      y+=rowHeight;
      // type
      if (spec.dimensionCols.count(col) && col<spec.dimensions.size() && col<spec.nColAxes())
        {
          pango.setText(classdesc::enumKey<Dimension::Type>(spec.dimensions[col].type));
          pango.setxy(x,y);
          pango.show();
        }
      y+=rowHeight;
      if (spec.dimensionCols.count(col) && col<spec.dimensions.size() && col<spec.nColAxes())
        {
          pango.setText(spec.dimensions[col].units);
          pango.setxy(x,y);
          pango.show();
        }
      y+=rowHeight;
      if (spec.dimensionCols.count(col) && col<spec.dimensionNames.size() && col<spec.nColAxes())
        {
          pango.setText(spec.dimensionNames[col]);
          pango.setxy(x,y);
          pango.show();
        }
      y+=rowHeight;
      for (size_t row=0; row<parsedLines.size(); ++row)
        {
          auto& line=parsedLines[row];
          if (col<line.size())
            {
              CairoSave cs(cairo);
              pango.setText(line[col]);
              pango.setxy(x, y);
              if (row==spec.headerRow && !(spec.columnar && col>spec.nColAxes()))
                if (col<spec.nColAxes())
                  cairo_set_source_rgb(surface->cairo(),0,0.7,0);
                else
                  cairo_set_source_rgb(surface->cairo(),0,0,1);
              else if (row<spec.nRowAxes() || (col<spec.nColAxes() && !spec.dimensionCols.count(col)) ||
                       (spec.columnar && col>spec.nColAxes()))
                cairo_set_source_rgb(surface->cairo(),1,0,0);
              else if (col<spec.nColAxes())
                cairo_set_source_rgb(surface->cairo(),0,0,1);
              pango.show();
            }
          else
            done.insert(row);
          y+=rowHeight;
        }
      {
        CairoSave cs(cairo);
        cairo_set_source_rgb(cairo,.5,.5,.5);
        cairo_move_to(cairo,x-2.5,0);
        cairo_rel_line_to(cairo,0,(parsedLines.size()+4)*rowHeight);
        cairo_stroke(cairo);
      }
      x+=colWidth+5;
      y=0;
    }
  for (size_t row=0; row<parsedLines.size()+5; ++row)
    {
      CairoSave cs(cairo);
      cairo_set_source_rgb(cairo,.5,.5,.5);
      cairo_move_to(cairo,xoffs-2.5,row*rowHeight);
      cairo_rel_line_to(cairo,(col-1)*(colWidth+5),0);
      cairo_stroke(cairo);
    }
}

size_t CSVDialog::columnOver(double x)
{
  return size_t((x-xoffs)/(colWidth+5));
}

size_t CSVDialog::rowOver(double y)
{
  return size_t(y/rowHeight);
}

void CSVDialog::copyHeaderRowToDimNames(size_t row)
{
  auto parsedLines=parseLines();
  if (row>=parseLines().size()) return;
  for (size_t c=0; c<spec.dimensionNames.size() && c<parsedLines[row].size(); ++c)
    spec.dimensionNames[c]=parsedLines[row][c];
}

std::string CSVDialog::headerForCol(size_t col) const
{
  auto parsedLines=parseLines();
  if (spec.headerRow<parsedLines.size() && col<parsedLines[spec.headerRow].size())
    return parsedLines[spec.headerRow][col];
  return "";
}

std::vector<std::vector<std::string>> CSVDialog::parseLines() const
{
  vector<vector<string>> parsedLines;
  if (spec.mergeDelimiters)
    if (spec.separator==' ')
      parsedLines=::parseLines(boost::char_separator<char>(), initialLines);
    else
      {
        char separators[]={spec.separator,'\0'};
        parsedLines=::parseLines
          (boost::char_separator<char>(separators,""),initialLines);
      }
  else
    parsedLines=::parseLines
      (boost::escaped_list_separator<char>(spec.escape,spec.separator,spec.quote),
       initialLines);
  return parsedLines;
}

