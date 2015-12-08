#pragma once
// C RunTime Header Files
// C++ Standard Library Header Files
#include "SimulDirectXHeader.h"
#include <sdkddkver.h>
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#include <dxgi.h>
#endif
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>

#pragma warning(push)
#pragma warning(disable:4251)

#ifdef _DEBUG
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#include <dxgi.h>
#else
	#ifndef _XBOX_ONE
		#include <D3D11_1.h>
	#endif
#endif

#include <string>
#include "Simul/Base/Timer.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "MacrosDX1x.h"

#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Effect.h"
#ifndef _XBOX_ONE
	struct ID3DUserDefinedAnnotation;
#endif
namespace simul
{
	namespace dx11
	{
		/*!
		The Simul DirectX 11 GPU profiler. Usage is as follows:

		* On initialization, when you have a device pointer:
		
				simul::dx11::Profiler::GetGlobalProfiler().Initialize(pd3dDevice);

		* On shutdown, or whenever the device is changed or lost:

				simul::dx11::Profiler::GetGlobalProfiler().Uninitialize();

		* Per-frame, at the start of the frame:

				simul::base::SetGpuProfilingInterface(deviceContext.platform_context,&simul::dx11::Profiler::GetGlobalProfiler());
				SIMUL_COMBINED_PROFILE_STARTFRAME(deviceContext.platform_context)

		*  Wrap these around anything you want to measure:

				SIMUL_COMBINED_PROFILE_START(deviceContext,"Element name")
				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* At frame-end:

				SIMUL_COMBINED_PROFILE_END(deviceContext)

		* To obtain the profiling results - pass true if you want HTML output:

				const char *text=simul::dx11::Profiler::GetGlobalProfiler().GetDebugText(as_html);
		*/
		SIMUL_DIRECTX11_EXPORT_CLASS Profiler:public simul::crossplatform::GpuProfiler
		{
		public:
			static Profiler &GetGlobalProfiler();
			Profiler();
			~Profiler();
			/// Call this when the profiler is to be initialized with a device pointer - must be done before use.
			void Initialize(ID3D11Device* device);
			/// Call this when the profiler is to be shut-down, or the device pointer has been lost or changed.
			void Uninitialize();
			void Begin(crossplatform::DeviceContext &deviceContext,const char *name);
			void End();
			
			void StartFrame(crossplatform::DeviceContext &deviceContext);
			void EndFrame(crossplatform::DeviceContext &deviceContext);
			ID3DUserDefinedAnnotation *pUserDefinedAnnotation;
			float GetTime(const std::string &name) const;
			//! Get all the active profilers as a text report.
			const char *GetDebugText(base::TextStyle style=base::PLAINTEXT) const;
			const base::ProfileData *GetEvent(const base::ProfileData *parent,int i) const;
			std::string GetChildText(const char *name,std::string tab) const;
			std::vector<std::string> last_name;
			std::vector<crossplatform::DeviceContext *> last_context;
			static Profiler GlobalProfiler;

			// Constants
			static const UINT64 QueryLatency = 6;

			struct ProfileData;
			typedef std::map<std::string,ProfileData*> ProfileMap;
			typedef std::map<int,ProfileData*> ChildMap;
			struct ProfileData:public base::ProfileData
			{
				simul::dx11::Query *DisjointQuery;
				simul::dx11::Query *TimestampStartQuery;
				simul::dx11::Query *TimestampEndQuery;
				bool gotResults[QueryLatency];
				ProfileData()
				:DisjointQuery(NULL)
				,TimestampStartQuery(NULL)
				,TimestampEndQuery(NULL)
				{
				}
				~ProfileData()
				{
					delete DisjointQuery;
					delete TimestampStartQuery;
					delete TimestampEndQuery;
				}
				ChildMap children;
			};
		protected:
			ProfileMap profileMap;
			int level;
			UINT64 currFrame;

			ID3D11Device* device;

			simul::base::Timer timer;
			float queryTime;
			std::string Walk(Profiler::ProfileData *p, int tab, float parent_time, base::TextStyle style) const;
		};

		class ProfileBlock
		{
		public:

			ProfileBlock(crossplatform::DeviceContext &c,const std::string& name);
			~ProfileBlock();

			/// Get the previous frame's timing value.
			float GetTime() const;
		protected:
			crossplatform::DeviceContext* context;
			std::string name;
		};
	}
}
#pragma warning(pop)