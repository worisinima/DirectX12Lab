#include "RenderTargetPool.h"
#include "RenderTarget.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;
using namespace FMathLib;
using namespace DirectX::PackedVector;

#ifdef ReleaseRenderTarget
#undef ReleaseRenderTarget
#endif
#define ReleaseRenderTarget(targetToBeRelease) \
if (targetToBeRelease != nullptr)\
{\
	delete(targetToBeRelease);\
	targetToBeRelease = nullptr;\
}

RenderTargetPool::~RenderTargetPool()
{
	for (auto t : mRenderTargetPool)
	{
		ReleaseRenderTarget(t)
	}
	for (auto t : mDepthRenderTargetPool)
	{
		ReleaseRenderTarget(t)
	}
}