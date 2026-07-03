#include "Hunt.h"


#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <timeapi.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "1986"




/*typedef struct tagAudioQuad
{
  float x1,y1,z1;
  float x2,y2,z2;
  float x3,y3,z3;
  float x4,y4,z4;
} AudioQuad;

AudioQuad data[8192];

  HMap[1024][1024];
*/


bool ShowFaces = true;





















































































































// ================================================================
// config.cfg — text-based settings file (shared with Carnivores2Menu)
// ================================================================
// Resolve config.cfg relative to the game executable first, then fall
// back to the current working directory. This ensures the file is found
// regardless of how the game is launched (via Menu or directly from a
// command prompt in a different directory).
