// $Id: cixd.cpp,v 1.3 2015-05-12 19:06:46-07 - - $

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream log (cout);
struct cix_exit: public exception {};

void reply_ls (accepted_socket& client_sock, cix_header& header) {
   FILE* ls_pipe = popen ("ls -l", "r");
   if (ls_pipe == NULL) { 
      log << "ls -l: popen failed: " << strerror (errno) << endl;
      header.command = CIX_NAK;
      header.nbytes = errno;
      send_packet (client_sock, &header, sizeof header);
   }
   string ls_output;
   char buffer[0x1000];
   for (;;) {
      char* rc = fgets (buffer, sizeof buffer, ls_pipe);
      if (rc == nullptr) break;
      ls_output.append (buffer);
   }
   pclose(ls_pipe);
   header.command = CIX_LSOUT;
   header.nbytes = ls_output.size();
   memset (header.filename, 0, FILENAME_SIZE);
   log << "sending header " << header << endl;
   send_packet (client_sock, &header, sizeof header);
   send_packet (client_sock, ls_output.c_str(), ls_output.size());
   log << "sent " << ls_output.size() << " bytes" << endl;
}

void reply_get (accepted_socket& client_sock, cix_header& header) {
	// try to open the file
	ifstream get_file(header.filename);
	if (!get_file.is_open()) {
		cerr << "reply_get: couldn't find file " << header.filename << endl; 
		// send a CIX_NAK
		header.command = CIX_NAK;
		header.nbytes = errno;
		send_packet (client_sock, &header, sizeof header);
	}
	struct stat stat_buff;
	stat (header.filename, &stat_buff);
	char buffer[stat_buff.st_size];
	if (!get_file.read(buffer,stat_buff.st_size)) {
		cerr << "reply_get: file read error" << endl;
		throw cix_exit();
	}
	header.command = CIX_FILE;
	header.nbytes = stat_buff.st_size;
	log << "sending header " << header << endl;
	send_packet (client_sock, &header, sizeof header);
	send_packet (client_sock, buffer, sizeof buffer);
	log << "sent " << sizeof buffer << " bytes" << endl;
}

void reply_put (accepted_socket& client, cix_header& header) {
	log << "reply_put: opening file " << header.filename << " for write" << endl;
	ofstream put_file(header.filename);
	if (!put_file.is_open()) {
		log << "reply_put: error opening file " << header.filename << endl;
		cix_exit();
	}
	char buffer[header.nbytes + 1];
	recv_packet (client, buffer, header.nbytes);
	buffer[header.nbytes] = '\0';
	log << "Writing to file " << header.filename << endl;
	put_file.write(buffer, header.nbytes);
	put_file.close();
	header.command = CIX_ACK;
	log << "sending packet " << header  << endl;
	send_packet (client, &header, header.nbytes);
}

void reply_rm (accepted_socket& client_sock, cix_header& header) {
	if (unlink(header.filename) == -1) {
		log << "Error deleting " << header.filename << endl;
		throw cix_exit();
	}
	header.command = CIX_ACK;
	log << "sending packet " << header  << endl;
	send_packet (client_sock, &header, header.nbytes);
}

void run_server (accepted_socket& client_sock) {
   log.execname (log.execname() + "-server");
   log << "connected to " << to_string (client_sock) << endl;
   try {   
      for (;;) {
         cix_header header; 
         recv_packet (client_sock, &header, sizeof header);
         log << "received header " << header << endl;
	 log << "received header command " << int(header.command) << endl;
         switch (header.command) {
            case CIX_LS: 
               reply_ls (client_sock, header);
               break;
	    case CIX_GET:
	       reply_get (client_sock, header);
	       break;
	    case CIX_PUT:
	       reply_put (client_sock, header);
	       break;
	    case CIX_RM:
	       reply_rm (client_sock, header);
	       break;
            default:
               log << "invalid header from client" << endl;
               log << "cix_nbytes = " << header.nbytes << endl;
               log << "cix_command = " << header.command << endl;
               log << "cix_filename = " << header.filename << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      log << "run_server: " << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   throw cix_exit();
}

void fork_cixserver (server_socket& server, accepted_socket& accept) {
   pid_t pid = fork();
   if (pid == 0) { // child
      server.close();
      run_server (accept);
      throw cix_exit();
   }else {
      accept.close();
      if (pid < 0) {
         log << "fork failed: " << strerror (errno) << endl;
      }else {
         log << "forked cixserver pid " << pid << endl;
      }
   }
}


void reap_zombies() {
   for (;;) {
      int status;
      pid_t child = waitpid (-1, &status, WNOHANG);
      if (child <= 0) break;
      log << "child " << child
           << " exit " << (status >> 8)
           << " signal " << (status & 0x7F)
           << " core " << (status >> 7 & 1) << endl;
   }
}

void signal_handler (int signal) {
   log << "signal_handler: caught " << strsignal (signal) << endl;
   reap_zombies();
}

void signal_action (int signal, void (*handler) (int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigfillset (&action.sa_mask);
   action.sa_flags = 0;
   int rc = sigaction (signal, &action, nullptr);
   if (rc < 0) log << "sigaction " << strsignal (signal) << " failed: "
                   << strerror (errno) << endl;
}


int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   signal_action (SIGCHLD, signal_handler);
   in_port_t port = get_cix_server_port (args, 0);
   try {
      server_socket listener (port);
      for (;;) {
         log << to_string (hostinfo()) << " accepting port "
             << to_string (port) << endl;
         accepted_socket client_sock;
         for (;;) {
            try {
               listener.accept (client_sock);
               break;
            }catch (socket_sys_error& error) {
               switch (error.sys_errno) {
                  case EINTR:
                     log << "listener.accept caught "
                         << strerror (EINTR) << endl;
                     break;
                  default:
                     throw;
               }
            }
         }
         log << "accepted " << to_string (client_sock) << endl;
         try {
            fork_cixserver (listener, client_sock);
            reap_zombies();
         }catch (socket_error& error) {
            log << error.what() << endl;
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

