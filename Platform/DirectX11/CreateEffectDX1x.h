// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.h Create a DirectX .fx effect and report errors.
#ifndef CREATEEFFECTDX1X_H
#define CREATEEFFECTDX1X_H
#include <d3dx11.h>
#include <d3dx11effect.h>
#include <D3Dcompiler.h>
#include <d3dx9.h>
#include <map>
#include "MacrosDX1x.h"
#include "Export.h"
struct VertexXyzRgba;
namespace simul
{
	namespace dx11
	{
		enum ShaderBuildMode
		{
			ALWAYS_BUILD=1,BUILD_IF_NO_BINARY,NEVER_BUILD
		};
		extern SIMUL_DIRECTX11_EXPORT void GetCameraPosVector(D3DXMATRIX &view,bool y_vertical,float *dcam_pos,float *view_dir);
		extern SIMUL_DIRECTX11_EXPORT const float *GetCameraPosVector(D3DXMATRIX &view,bool y_vertical);
		//! Call this to make the FX compiler put its warnings and errors to the standard output when used.
		extern SIMUL_DIRECTX11_EXPORT void PipeCompilerOutput(bool p);
		extern SIMUL_DIRECTX11_EXPORT void SetShaderBuildMode(ShaderBuildMode s);
		extern SIMUL_DIRECTX11_EXPORT void SetShaderPath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT void SetTexturePath(const char *path);
		extern SIMUL_DIRECTX11_EXPORT void SetDevice(ID3D1xDevice* dev);
		extern SIMUL_DIRECTX11_EXPORT void UnsetDevice();
		extern SIMUL_DIRECTX11_EXPORT void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj);
		extern ID3D1xShaderResourceView* LoadTexture(const char *filename);
		extern ID3D1xShaderResourceView* LoadTexture(const TCHAR *filename);
		ID3D1xTexture1D* make1DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w
							,DXGI_FORMAT format
							,const float *src);
		ID3D11Texture2D* make2DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w,int h
							,DXGI_FORMAT format
							,const float *src);
		ID3D1xTexture3D* make3DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w,int l,int d
							,DXGI_FORMAT format
							,const void *src);
							
		void Ensure3DTextureSizeAndFormat(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xTexture3D* &tex
							,ID3D11ShaderResourceView* &srv
							,int w,int l,int d
							,DXGI_FORMAT format);
							
		void SIMUL_DIRECTX11_EXPORT FixProjectionMatrix(struct D3DXMATRIX &proj,float zFar);
		void SIMUL_DIRECTX11_EXPORT FixProjectionMatrix(struct D3DXMATRIX &proj,float zNear,float zFar);
		D3DXMATRIX SIMUL_DIRECTX11_EXPORT ConvertReversedToRegularProjectionMatrix(const D3DXMATRIX &proj);
	
		void setParameter(ID3D1xEffect *effect,const char *name	,ID3D11ShaderResourceView * value);
		void setParameter(ID3D1xEffect *effect,const char *name	,ID3D11UnorderedAccessView * value);
		void setParameter(ID3D1xEffect *effect,const char *name	,float value);
		void setParameter(ID3D1xEffect *effect,const char *name	,float x,float y);
		void setParameter(ID3D1xEffect *effect,const char *name	,int value);
		void setParameter(ID3D1xEffect *effect,const char *name	,float *vec);
		void setMatrix(ID3D1xEffect *effect	,const char *name	,const float *value);
							
		int ByteSizeOfFormatElement( DXGI_FORMAT format );
	}
}

typedef long HRESULT;
extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename);
extern SIMUL_DIRECTX11_EXPORT HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename,const std::map<std::string,std::string>&defines);


extern SIMUL_DIRECTX11_EXPORT HRESULT Map2D(ID3D11DeviceContext *m_pImmediateContext,ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap2D(ID3D11DeviceContext *m_pImmediateContext,ID3D1xTexture2D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map3D(ID3D11DeviceContext *m_pImmediateContext,ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap3D(ID3D11DeviceContext *m_pImmediateContext,ID3D1xTexture3D *tex);
extern SIMUL_DIRECTX11_EXPORT HRESULT Map1D(ID3D11DeviceContext *m_pImmediateContext,ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp);
extern SIMUL_DIRECTX11_EXPORT void Unmap1D(ID3D11DeviceContext *m_pImmediateContext,ID3D1xTexture1D *tex);

extern SIMUL_DIRECTX11_EXPORT HRESULT MapBuffer(ID3D11DeviceContext *m_pImmediateContext,ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE *vert);
extern SIMUL_DIRECTX11_EXPORT void UnmapBuffer(ID3D11DeviceContext *m_pImmediateContext,ID3D1xBuffer *vertexBuffer);
extern SIMUL_DIRECTX11_EXPORT HRESULT ApplyPass(ID3D11DeviceContext *m_pImmediateContext,ID3D1xEffectPass *pass);

extern void SIMUL_DIRECTX11_EXPORT MakeCubeMatrices(D3DXMATRIX g_amCubeMapViewAdjust[],const float *cam_pos);
void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );
void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext );

#define PAD16(n) (((n)+15)/16*16)
#endif
