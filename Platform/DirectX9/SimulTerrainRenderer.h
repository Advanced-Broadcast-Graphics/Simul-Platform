// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif

#include <vector>
#include "Simul/Math/float3.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Base/Referenced.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/Terrain/HeightMapNode.h"
#include "Simul/Platform/DirectX9/Export.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace sky
	{
		class BaseSkyInterface;
	}
	namespace terrain
	{
		class HeightMapInterface;
		class HeightMapNode;
		struct Cutout;
	}
	namespace clouds
	{
		class CloudShadowCallback;
	}
}
SIMUL_DIRECTX9_EXPORT_CLASS SimulTerrainRenderer:public simul::base::Referenced
{
public:
	SimulTerrainRenderer();
	//standard d3d object interface functions
	bool Create(LPDIRECT3DDEVICE9 pd3dDevice);
	bool RestoreDeviceObjects(void *pd3dDevice);
	bool InvalidateDeviceObjects();

	virtual ~SimulTerrainRenderer();
	bool RenderOnlyDepth();
	bool Render();
	int GetMip(int i,int j) const;
	bool RenderMap(int w);
	void Update(float dt);
	// Set a callback that will return cloud shadow data and textures:
	void SetCloudShadowCallback(simul::clouds::CloudShadowCallback *cb);
	void SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p);
	void SetCloudTextures(void **t,bool wrap);
	void SetSkyInterface(simul::sky::BaseSkyInterface *si){skyInterface=si;}
	simul::terrain::HeightMapInterface *GetHeightMapInterface();
	simul::terrain::HeightMapNode *GetHeightMap();
	void Highlight(const float *x,const float *d);
	void SetCloudScales(const float *s)
	{
		cloud_scales[0]=s[0];
		cloud_scales[1]=s[1];
		cloud_scales[2]=s[2];
	}
	void SetCloudOffset(const float *s)
	{
		cloud_offset[0]=s[0];
		cloud_offset[1]=s[1];
		cloud_offset[2]=s[2];
	}
	void setCloudInterpolation(float s)
	{
		cloud_interp=s;
	}
	void SetExposure(float e)
	{
		exposure=e;
	}
	void SetShowWireframe(bool s)
	{
		show_wireframe=s;
	}
	void SetYVertical(bool s)
	{
		y_vertical=s;
		InvalidateDeviceObjects();
		if(m_pd3dDevice)
			RestoreDeviceObjects(m_pd3dDevice);
	}
	void SetMaxFadeDistanceKm(float dist_km)
	{
		max_fade_distance_metres=dist_km*1000.f;
		RebuildEffect();
	}
	void RebuildEffect()
	{
		rebuild_effect=true;
	}
	void TerrainModified();
	const float *GetHighlightPos() const{return highlight_pos;}
protected:
	simul::terrain::HeightMapInterface *heightMapInterface;
	bool y_vertical;
	bool enabled;
	bool wrap_clouds;
	bool rebuild_effect;
	bool CreateEffect();
	simul::base::SmartPtr<simul::terrain::HeightMapNode> heightmap;
	bool InternalRender(bool depth_only);
	float altitude_tex_coord;
	bool show_wireframe;
	simul::sky::BaseSkyInterface *skyInterface;
	LPDIRECT3DDEVICE9		m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9 m_pVtxDecl;
	LPD3DXEFFECT			m_pTerrainEffect;
	LPDIRECT3DTEXTURE9		terrain_texture;
	LPDIRECT3DTEXTURE9		detail_texture;

	LPDIRECT3DTEXTURE9		elevation_map_texture;
	bool MakeMapTexture();

	LPDIRECT3DTEXTURE9		road_texture;
	LPDIRECT3DVOLUMETEXTURE9 *cloud_textures;
	D3DXHANDLE				worldViewProj;
	D3DXHANDLE              m_hTechniqueTerrain;	
	D3DXHANDLE              m_hTechniqueDepthOnly;	
	D3DXHANDLE              m_hTechniqueMap;	
	D3DXHANDLE              techniqueRoad;			
	D3DXHANDLE				eyePosition;
	D3DXHANDLE				lightDirection;
	D3DXHANDLE				cloudScales;
	D3DXHANDLE				cloudOffset;
	D3DXHANDLE				cloudInterp;
	D3DXHANDLE				exposureParam;
	D3DXHANDLE				altitudeTexCoord;

	D3DXHANDLE				lightColour;
	D3DXHANDLE				ambientColour;

	D3DXHANDLE				g_mainTexture;
	D3DXHANDLE				detailTexture;
	D3DXHANDLE				roadTexture;
	D3DXHANDLE				cloudTexture1;
	D3DXHANDLE				cloudTexture2;
	
	D3DXMATRIX				view,proj;
	D3DXVECTOR3				cam_pos;
	LPDIRECT3DVERTEXBUFFER9	vertexBuffer;

	simul::math::Vector3 dir_to_sun;
	D3DXVECTOR3				highlight_pos;
	// A MIP edge joins a higher-resolution MIP to a lower-resolution one.
	// The inner vertices come from the main grid.
	// The outer vertices are interpolations.
	struct MIPEdge
	{
		LPDIRECT3DINDEXBUFFER9 indexBuffer;
		int num_verts;
		int num_tris;
		void Reset();
	};
	struct MIPEdges
	{
		MIPEdge edge[4];
		void Reset();
	};
	struct TerrainTileMIP
	{
		// The inner square:
		unsigned num_prims;
		unsigned num_verts;
		LPDIRECT3DINDEXBUFFER9 indexBuffer;

		// four MIP edges for each lower MIP level
		std::vector<MIPEdges> edges;

		// extra vertices for cutouts:
		LPDIRECT3DVERTEXBUFFER9	extraVertexBuffer;
		LPDIRECT3DINDEXBUFFER9 extraIndexBuffer;
		typedef std::vector<bool> BitMask;
		typedef std::vector<BitMask> BitMap;
		std::vector<simul::math::float3> extra_vertices;
		std::vector<int> extra_triangles;
		BitMap unimpeded_squares;
		int num_squares;
		bool tri_strip;
		TerrainTileMIP(int tilesize);

		int tile_size;
		void Reset();
		void ApplyCutouts(simul::terrain::HeightMapInterface *hmi,int i,int j,int step,const simul::terrain::Cutout *cutout);
	};
	struct RoadRenderable
	{
		LPDIRECT3DVERTEXBUFFER9	roadVertexBuffer;
		LPDIRECT3DINDEXBUFFER9 roadIndexBuffer;
		int num_verts;
		void Reset();
	};
	std::vector < RoadRenderable  > roads;
	struct TerrainTile
	{
		std::vector<TerrainTileMIP> mips;
		TerrainTile(int tilesize,int num_mips);
		void Reset();
		float pos[3];
	};
	typedef std::vector < TerrainTile  > TerrainRow;
	typedef std::vector < TerrainRow > Terrain2D;

	bool BuildRoad();
	bool BuildMIPEdge(TerrainTile *tile,int i,int j,int mip_level,int lower_level,int nsew);
	bool BuildTile(TerrainTile *tile,int i,int j,int mip_level);
	void ReleaseIndexBuffers();

	Terrain2D tiles;
	unsigned TilePosToIndex(int i,int j,int x,int y) const;

	float cloud_scales[3];
	float cloud_offset[3];
	float cloud_interp;
	void GetVertex(int i,int j,struct TerrainVertex_t *V);
	void GetVertex(float x,float y,struct TerrainVertex_t *V);

	float exposure;
	float max_fade_distance_metres;
	
	unsigned last_overall_checksum;
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif