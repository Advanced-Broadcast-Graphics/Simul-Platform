#ifndef PSFXPROGRAM_H
#define PSFXPROGRAM_H

namespace sfx
{
	class Pass
	{
	public:
		PassState passState;
		Pass();
		Pass(const PassState &passState);
		bool HasShader(ShaderType t) const
		{
			return m_shaderInstanceNames[(int)t].length()>0;
		}
		std::string GetShader(ShaderType t) const
		{
			return m_shaderInstanceNames[(int)t];
		}

	private:
		std::string m_shaderInstanceNames[NUM_OF_SHADER_COMMANDS];
		friend int ::sfxparse();
	};
	class Technique
	{
	public:
		Technique(const std::map<std::string, Pass>& passes);
		const std::map<std::string, Pass>	&GetPasses()  const
		{
			return m_passes;
		}
	private:
		std::map<std::string, Pass>	m_passes;
		friend int ::sfxparse();
	};
}
#endif