#pragma once


struct ViewRenderList_t
{
	// TODO: needs improvement and further sorting
	// [ Shader ] = vector of surfaces to draw
	std::unordered_map< Handle, ChVector< SurfaceDraw_t > > aRenderLists;
};


constexpr glm::vec4 gFrustumFaceData[ 8u ] = {
	// Near Face
	{ 1, 1, -1, 1.f },
	{ -1, 1, -1, 1.f },
	{ 1, -1, -1, 1.f },
	{ -1, -1, -1, 1.f },

	// Far Face
	{ 1, 1, 1, 1.f },
	{ -1, 1, 1, 1.f },
	{ 1, -1, 1, 1.f },
	{ -1, -1, 1, 1.f },
};


bool Graphics_CreateVariableUniformLayout( UniformBufferArray_t& srBuffer, const char* spLayoutName, const char* spSetName, int sCount );

void Graphics_DrawShaderRenderables( Handle cmd, Handle shader, ChVector< SurfaceDraw_t >& srRenderList );

