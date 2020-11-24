#pragma once

#include "RenderTarget.h"

class RenderTarget;

class RenderTargetPool
{
public:

	template<class T>
	void RigisterToRenderTargetPool(RenderTarget*& TargetToRigister, const DXGI_FORMAT& format, const DirectX::FMathLib::Color& inClearColor)
	{
		TargetToRigister = new T(format, inClearColor);
		mRenderTargetPool.push_back(TargetToRigister);
	}
	UINT GetRenderTargetPoolSize(){ return mRenderTargetPool.size(); }

	template<class T>
	void RigisterToRenderTargetPool(RenderTarget*& TargetToRigister)
	{
		TargetToRigister = new T();
		mDepthRenderTargetPool.push_back(TargetToRigister);
	}
	UINT GetDepthRenderTargetPoolSize() { return mDepthRenderTargetPool.size(); }

	~RenderTargetPool();

private:
	
	std::vector<RenderTarget*> mRenderTargetPool;
	std::vector<RenderTarget*> mDepthRenderTargetPool;

};

