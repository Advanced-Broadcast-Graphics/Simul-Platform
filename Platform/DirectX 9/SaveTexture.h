#define _CRT_SECURE_NO_WARNINGS
// Direct3D includes
#include <d3d9.h>
#include "Simul/Platform/DirectX 9/Export.h"

extern void Screenshot(IDirect3DDevice9* pd3dDevice,const char *txt);
extern void SaveTexture(LPDIRECT3DTEXTURE9 texture,const char *txt);