#include "Mesh.h"
#include <d3dx11.h>
#include "MacrosDX1x.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
using namespace simul;
using namespace dx11;

Mesh::Mesh()
	:vertexBuffer(NULL)
	,indexBuffer(NULL)
	,inputLayout(NULL)
	,stride(0)
	,numVertices(0)
	,numIndices(0)
	,previousInputLayout(NULL)
	,previousTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
{
}

Mesh::~Mesh()
{
	release();
}

bool Mesh::Initialize(void *device,int lPolygonVertexCount,float *lVertices,float *lNormals,float *lUVs,int lPolygonCount,unsigned int *lIndices)
{
	ID3D11Device	*pd3dDevice=(ID3D11Device*)device;
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	stride=0;
	numVertices=0;
	numIndices=0;
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		ID3DX11Effect *effect=NULL;
		CreateEffect(pd3dDevice,&effect,"solid.fx");
		ID3DX11EffectTechnique *tech	=effect->GetTechniqueByName("solid");
		tech->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D11_INPUT_ELEMENT_DESC decl[]=
		{
			{"POSITION"	,0	,DXGI_FORMAT_R32G32B32_FLOAT	,0,0,	D3D11_INPUT_PER_VERTEX_DATA,0},
			{"TEXCOORD"	,0	,DXGI_FORMAT_R32G32_FLOAT		,0,12,	D3D11_INPUT_PER_VERTEX_DATA,0},
			{"TEXCOORD"	,1	,DXGI_FORMAT_R32G32B32_FLOAT	,0,20,	D3D11_INPUT_PER_VERTEX_DATA,0},
		};
		SAFE_RELEASE(inputLayout);
		V_CHECK(pd3dDevice->CreateInputLayout(decl,3,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &inputLayout));
		SAFE_RELEASE(effect);
	}
	// Put positions, texcoords and normals in an array of structs:
	numVertices=lPolygonVertexCount;
	numIndices=lPolygonCount*3;
	stride=0;
	struct Vertex
	{
		vec3 pos;
		vec2 texc;
		vec3 normal;
	};
	stride = sizeof(Vertex);
	Vertex *vertices=new Vertex[lPolygonVertexCount];
	for(int i=0;i<lPolygonVertexCount;i++)
	{
		Vertex &v	=vertices[i];
		v.pos		=&(lVertices[i*3]);
		v.texc		=&(lUVs[i*2]);
		v.normal	=&(lNormals[i*3]);
	}
	init(pd3dDevice,numVertices,numIndices,vertices,lIndices);
	delete [] vertices;
	return true;
}

void Mesh::release()
{
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	stride=0;
	numVertices=0;
	numIndices=0;
}

void Mesh::BeginDraw(void *context,scene::ShadingMode pShadingMode) const
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->IAGetInputLayout( &previousInputLayout );
	pContext->IAGetPrimitiveTopology(&previousTopology);
}
// Draw all the faces with specific material with given shading mode.
void Mesh::Draw(void *context,int pMaterialIndex,scene::ShadingMode pShadingMode) const
{
	UINT offset = 0;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&vertexBuffer,		// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer
	pContext->IASetIndexBuffer(indexBuffer,DXGI_FORMAT_R32_UINT,0);					

	// Set the input layout
	pContext->IASetInputLayout(inputLayout);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pContext->Draw(numIndices,0);
}
// Unbind buffers, reset vertex arrays, turn off lighting and texture.
void Mesh::EndDraw(void *context) const
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

void Mesh::apply(ID3D11DeviceContext *pImmediateContext,unsigned instanceStride,ID3D11Buffer *instanceBuffer)
{
	UINT strides[]={stride,instanceStride};
	UINT offsets[]={0,0};
	ID3D11Buffer *buffers[]={vertexBuffer,instanceBuffer};

	pImmediateContext->IASetVertexBuffers(	0,			// the first input slot for binding
												2,			// the number of buffers in the array
												buffers,	// the array of vertex buffers
												strides,	// array of stride values, one for each buffer
												offsets);	// array of offset values, one for each buffer

	UINT Strides[1];
	UINT Offsets[1];
	Strides[0] = 0;
	Offsets[0] = 0;
	pImmediateContext->IASetIndexBuffer(	indexBuffer,
											DXGI_FORMAT_R16_UINT,	// unsigned short
											0);						// array of offset values, one for each buffer
	
}

void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
}