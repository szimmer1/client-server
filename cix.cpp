// $Id: cix.cpp,v 1.2 2015-05-12 18:59:40-07 - - $

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "util.h"
#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream log (cout);
struct cix_exit: public exception {};

unordered_map<string,cix_command> command_map {
   {"exit", CIX_EXIT},
   {"help", CIX_HELP},
   {"ls"  , CIX_LS  },
   {"get" , CIX_GET },
   {"put" , CIX_PUT }
};

void call_server_with (client_socket& server, cix_command which, const string& filename) {
   auto found_cmd_itor = cix_command_map.find(int(which));
   auto _recip_cmd_itor = cix_recip_cmds.find(int(which));
   unordered_map<int,string>::const_iterator recip_cmd_itor;
   if (found_cmd_itor == cix_command_map.cend()) {
	   log << int(which) << endl;
	   log << "call_server_with: unrecognized cix_command" << endl;
	   throw cix_exit();
   }
   if (_recip_cmd_itor == cix_recip_cmds.cend()) {
	   log << "cal_server_with: unknown reciprocal cix_command" << endl;
	   throw cix_exit();
   }
   recip_cmd_itor = cix_command_map.find(int(_recip_cmd_itor->second));
   cix_header header;
   header.command = which;
   filename.copy(header.filename, filename.size());
   log << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   log << "received header " << header << endl;
   if (header.command != _recip_cmd_itor->second) {
      log << "sent " << found_cmd_itor->second
	      << ", server did not return " << recip_cmd_itor->second << endl;
      log << "server returned " << header << endl;
   }else {
      char buffer[header.nbytes + 1];
      recv_packet (server, buffer, header.nbytes);
      log << "received " << header.nbytes << " bytes" << endl;
      buffer[header.nbytes] = '\0';
      switch (header.command) {
	      case CIX_LSOUT:
		{
			cout << buffer;
			break;
		}
	      case CIX_FILE:
		{
			log << "Opening file for write: header: " << header << endl;
			ofstream get_file(header.filename);
			if (!get_file.is_open()) {
				log << "cix_get: error opening file for write" << endl;
				break;
			}
			log << "Writing to " << header.filename << endl;
			get_file.write(buffer, header.nbytes);
			log << "Write succeeded" << endl;
			get_file.close();
			break;
		}
	      case CIX_ACK:
		{
			log << "Request successfully completed" << endl;
			break;
		}
      }
   }
}

void cix_help() {
   static vector<string> help = {
      "exit         - Exit the program.  Equivalent to EOF.",
      "get filename - Copy remote file to local host.",
      "help         - Print help summary.",
      "ls           - List names of files on remote server.",
      "put filename - Copy local file to remote host.",
      "rm filename  - Remove file from remote server.",
   };
   for (const auto& line: help) cout << line << endl;
}

void cix_ls (client_socket& server) {
	call_server_with (server, CIX_LS, "");
}

void cix_get (client_socket& server, const string& file) {
	call_server_with (server, CIX_GET,file);
}

void cix_put (client_socket& server, const string& file) {
	struct stat stat_info;
	stat (file.c_str(), &stat_info);

	cix_header header;
	header.command = CIX_PUT;
	file.copy(header.filename, file.size());
	header.nbytes = stat_info.st_size;
	send_packet (server, &header, sizeof header);

	log << "put : Opening file " << file << " for write" << endl;
	ifstream put_file(file);
	if (!put_file.is_open()) {
		cerr << "put : error opening file " << file << endl;
		return;
	}
	log << "sending " << header << endl;
	char buffer[stat_info.st_size];
	if (!put_file.read(buffer, stat_info.st_size)) {
		cerr << "put : error reading file " << file << endl;
		cix_exit();
	}
	log << "sending file " << file << endl;
	send_packet (server, buffer, sizeof buffer);
	recv_packet (server, &header, sizeof header);
	log << "recieved " << header << endl;
	if (header.command != CIX_ACK) {
		log << "server sent " << header << endl;
		log << "put failed" << endl;
		return;
	}
}

void usage () {
	cerr << "Usage: " << log.execname() << " [host] [port]" << endl;
	throw cix_exit();
}

int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   if (args.size() > 2) usage();
   string host = get_cix_server_host (args, 0);
   in_port_t port = get_cix_server_port (args, 1);
   log << to_string (hostinfo()) << endl;
   try {
      log << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      log << "connected to " << to_string (server) << endl;
      for (;;) {
         string line;
         getline (cin, line);
	 wordvec words = split (line, " ");
         if (cin.eof()) throw cix_exit();
         log << "command " << words[0] << endl;
         const auto& itor = command_map.find (words[0]);
         cix_command cmd = itor == command_map.end()
                         ? CIX_ERROR : itor->second;
         switch (cmd) {
            case CIX_EXIT:
               throw cix_exit();
               break;
            case CIX_HELP:
               cix_help();
               break;
            case CIX_LS:
               cix_ls (server);
               break;
	    case CIX_GET:
	       {
	       	  string file;
	       	  try {
	       	          file = words.at(1);
	       	  } catch (exception e) {
	       	          cerr << "get : no filename given" << endl;
	       	          break;
	       	  }
	       	  cix_get (server, file);
	       	  break;
	       }
	    case CIX_PUT:
	       {
		       string file;
		       try {
			       file = words.at(1);
		       } catch (exception e) {
			       cerr << "put : no filename given" << endl;
			       break;
		       }
		       cix_put (server, file);
		       break;
	       }
            default:
               log << line << ": invalid command" << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      log << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   return 0;
}

