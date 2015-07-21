#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include <string>
#include <map>
#pragma warning(disable:4251)
namespace simul
{
	namespace opengl
	{
		SIMUL_OPENGL_EXPORT_CLASS Profiler:public simul::base::GpuProfilingInterface
		{
		public:
			static Profiler &GetGlobalProfiler();
			void Initialize(void*);
			void Uninitialize();
			void Begin(void *,const char *name);
			void End();
			
			void StartFrame(void *);
			void EndFrame(void *);
			
			float GetTime(const std::string &name) const;
			const char *GetDebugText(base::TextStyle style=base::PLAINTEXT) const;
		protected:
			std::vector<unsigned> query_stack;
			static Profiler GlobalProfiler;
			// Constants
			static const unsigned int QueryLatency = 5;
			struct ProfileData
			{
				~ProfileData();
				GLuint queryID[2];
				GLuint TimestampQuery[QueryLatency];
				bool QueryStarted;
				bool QueryFinished;
				float time;
				ProfileData()
					:QueryStarted(false)
					,QueryFinished(false)
					, time(0.f)
				{
					for(int i=0;i<QueryLatency;i++)
					{
						TimestampQuery[i]	=0;
					}
				}
			};
			typedef std::map<std::string, ProfileData> ProfileMap;

			ProfileMap profiles;
			unsigned int currFrame;
			std::string last_name;
			simul::base::Timer timer;
			std::string output;
		};
		class ProfileBlock
		{
		public:

			ProfileBlock(void *ctx,const char *name);
			~ProfileBlock();
			/// Get the previous frame's timing value.
			float GetTime() const;
		protected:
			std::string name;
		};

	}
}