// $Id: protocol.h,v 1.2 2015-05-12 18:59:40-07 - - $

#ifndef __PROTOCOL__H__
#define __PROTOCOL__H__

#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <iostream>
using namespace std;

#include "sockets.h"

enum cix_command {CIX_ERROR = 0, CIX_EXIT,
                  CIX_GET, CIX_HELP, CIX_LS, CIX_PUT, CIX_RM,
                  CIX_FILE, CIX_LSOUT, CIX_ACK, CIX_NAK};

const unordered_map<int,string> cix_command_map {
   {int (CIX_ERROR), "CIX_ERROR"},
   {int (CIX_EXIT ), "CIX_EXIT" },
   {int (CIX_GET  ), "CIX_GET"  },
   {int (CIX_HELP ), "CIX_HELP" },
   {int (CIX_LS   ), "CIX_LS"   },
   {int (CIX_PUT  ), "CIX_PUT"  },
   {int (CIX_RM   ), "CIX_RM"   },
   {int (CIX_FILE ), "CIX_FILE" },
   {int (CIX_LSOUT), "CIX_LSOUT"},
   {int (CIX_ACK  ), "CIS_ACK"  },
   {int (CIX_NAK  ), "CIS_NAK"  },
};

const unordered_map<int,cix_command> cix_recip_cmds {
	{int(CIX_LS)   , CIX_LSOUT },
	{int(CIX_GET)  , CIX_FILE  },
	{int(CIX_PUT)  , CIX_ACK   },
	{int(CIX_RM)   , CIX_ACK   }
};

size_t constexpr FILENAME_SIZE = 59;
struct cix_header {
   uint32_t nbytes {0};
   uint8_t command {0};
   char filename[FILENAME_SIZE] {};
};

struct buff_info {
	buff_info (char* b, size_t s) : buffer(b), size(s) {};
	char* buffer;
	size_t size;
};

void send_packet (base_socket& socket,
                  const void* buffer, size_t bufsize);

void recv_packet (base_socket& socket, void* buffer, size_t bufsize);

ostream& operator<< (ostream& out, const cix_header& header);

string get_cix_server_host (const vector<string>& args, size_t index);

in_port_t get_cix_server_port (const vector<string>& args,
                               size_t index);

#endif

